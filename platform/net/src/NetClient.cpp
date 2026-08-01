#include "mega/net/NetClient.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "mega/net/Json.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

namespace mega::net {
namespace {

/// Parses "host:port", "ms://host:port" and "tcp://host:port".
bool parseUrl(const std::string& url, std::string& host, std::string& port) {
    std::string rest = url;
    const std::size_t schemeEnd = rest.find("://");
    if (schemeEnd != std::string::npos) {
        rest = rest.substr(schemeEnd + 3);
    }
    const std::size_t slash = rest.find('/');
    if (slash != std::string::npos) {
        rest = rest.substr(0, slash);
    }
    const std::size_t colon = rest.rfind(':');
    if (colon == std::string::npos || colon + 1 >= rest.size()) {
        host = rest;
        port = "8781";
        return !host.empty();
    }
    host = rest.substr(0, colon);
    port = rest.substr(colon + 1);
    return !host.empty();
}

Team teamFromInt(int value) {
    return value == 1 ? Team::Red : Team::Blue;
}

MatchPhase phaseFromInt(int value) {
    switch (value) {
        case 1: return MatchPhase::Playing;
        case 2: return MatchPhase::Countdown;
        case 3: return MatchPhase::Finished;
        default: return MatchPhase::Warmup;
    }
}

Snapshot snapshotFromJson(const Json& root) {
    Snapshot snap;
    const Json& match = root["match"];
    snap.match.phase = phaseFromInt(match["ph"].asInt());
    snap.match.phaseTimer = match["pt"].asFloat();
    snap.match.elapsed = match["el"].asFloat();
    snap.match.teamGems[0] = match["g0"].asInt();
    snap.match.teamGems[1] = match["g1"].asInt();

    for (const Json& item : root["players"].items()) {
        Player p;
        p.id = static_cast<EntityId>(item["id"].asInt());
        p.name = item["n"].asString();
        p.brawlerId = item["b"].asString();
        p.team = teamFromInt(item["tm"].asInt());
        p.position = {item["x"].asFloat(), item["y"].asFloat()};
        p.aim = {item["ax"].asFloat(1.0f), item["ay"].asFloat()};
        p.health = item["hp"].asInt();
        p.maxHealth = item["mhp"].asInt(1);
        p.ammo = item["am"].asFloat();
        p.ammoCapacity = item["ac"].asInt(3);
        p.superCharge = item["sc"].asInt();
        p.gemsHeld = item["gm"].asInt();
        p.respawnTimer = item["rt"].asFloat();
        p.kills = item["k"].asInt();
        p.deaths = item["d"].asInt();
        p.bot = item["bot"].asBool();
        snap.players.push_back(p);
    }
    for (const Json& item : root["proj"].items()) {
        Projectile proj;
        proj.id = static_cast<EntityId>(item["id"].asInt());
        proj.team = teamFromInt(item["tm"].asInt());
        proj.position = {item["x"].asFloat(), item["y"].asFloat()};
        snap.projectiles.push_back(proj);
    }
    for (const Json& item : root["gems"].items()) {
        Gem gem;
        gem.id = static_cast<EntityId>(item["id"].asInt());
        gem.position = {item["x"].asFloat(), item["y"].asFloat()};
        snap.gems.push_back(gem);
    }
    snap.valid = true;
    return snap;
}

/// Newline-delimited JSON over plain TCP. The JS Edition server speaks the very
/// same messages over WebSocket, so browser and native players share matches.
class TcpNetClient final : public NetClient {
public:
    TcpNetClient(std::string host, std::string port)
        : host_(std::move(host)), port_(std::move(port)) {}

    ~TcpNetClient() override { disconnect(); }

    bool connect(const std::string& playerName, const std::string& brawlerId) override {
#if defined(_WIN32)
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }
#endif
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* result = nullptr;
        if (::getaddrinfo(host_.c_str(), port_.c_str(), &hints, &result) != 0) {
            return false;
        }
        for (addrinfo* it = result; it != nullptr; it = it->ai_next) {
            socket_ = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
            if (socket_ == kInvalidSocket) {
                continue;
            }
            if (::connect(socket_, it->ai_addr, static_cast<int>(it->ai_addrlen)) == 0) {
                break;
            }
            closeSocket();
        }
        ::freeaddrinfo(result);

        if (socket_ == kInvalidSocket) {
            return false;
        }
        setNonBlocking();

        int one = 1;
        ::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one),
                     sizeof(one));

        send("{\"t\":\"join\",\"name\":\"" + Json::escape(playerName) + "\",\"brawler\":\"" +
             Json::escape(brawlerId) + "\"}\n");
        connected_ = true;
        return true;
    }

    void sendInput(const PlayerInput& input) override {
        if (!connected_) {
            return;
        }
        char buffer[192];
        std::snprintf(buffer, sizeof(buffer),
                      "{\"t\":\"input\",\"mx\":%.3f,\"my\":%.3f,\"ax\":%.3f,\"ay\":%.3f,\"s\":%d,"
                      "\"u\":%d}\n",
                      input.move.x, input.move.y, input.aim.x, input.aim.y, input.shoot ? 1 : 0,
                      input.useSuper ? 1 : 0);
        send(buffer);
    }

    void poll(World& world) override {
        if (!connected_) {
            return;
        }
        char buffer[8192];
        for (;;) {
#if defined(_WIN32)
            const int received = ::recv(socket_, buffer, sizeof(buffer), 0);
#else
            const ssize_t received = ::recv(socket_, buffer, sizeof(buffer), 0);
#endif
            if (received > 0) {
                inbox_.append(buffer, static_cast<std::size_t>(received));
                continue;
            }
            if (received == 0) {
                connected_ = false;
            }
            break;
        }

        std::size_t newline = inbox_.find('\n');
        while (newline != std::string::npos) {
            const std::string line = inbox_.substr(0, newline);
            inbox_.erase(0, newline + 1);
            handleLine(line, world);
            newline = inbox_.find('\n');
        }
    }

    void disconnect() override {
        if (socket_ != kInvalidSocket) {
            closeSocket();
        }
        connected_ = false;
#if defined(_WIN32)
        WSACleanup();
#endif
    }

    bool connected() const override { return connected_; }

    EntityId localPlayerId() const override { return localId_; }

private:
    void handleLine(const std::string& line, World& world) {
        const Json root = Json::parse(line);
        const std::string type = root["t"].asString();
        if (type == "welcome") {
            localId_ = static_cast<EntityId>(root["id"].asInt());
            const Json& tiles = root["tiles"];
            const int width = root["w"].asInt();
            const int height = root["h"].asInt();
            if (width > 0 && height > 0 && tiles.size() == static_cast<std::size_t>(width * height)) {
                std::vector<Tile> decoded;
                decoded.reserve(tiles.size());
                for (const Json& tile : tiles.items()) {
                    decoded.push_back(static_cast<Tile>(tile.asInt()));
                }
                world.setArena(Arena(width, height, decoded));
            }
        } else if (type == "state") {
            world.applySnapshot(snapshotFromJson(root));
        }
    }

    void send(const std::string& payload) {
        if (socket_ == kInvalidSocket) {
            return;
        }
#if defined(_WIN32)
        ::send(socket_, payload.c_str(), static_cast<int>(payload.size()), 0);
#else
        ::send(socket_, payload.c_str(), payload.size(), MSG_NOSIGNAL);
#endif
    }

    void setNonBlocking() {
#if defined(_WIN32)
        u_long mode = 1;
        ::ioctlsocket(socket_, FIONBIO, &mode);
#else
        const int flags = ::fcntl(socket_, F_GETFL, 0);
        ::fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
#endif
    }

    void closeSocket() {
#if defined(_WIN32)
        ::closesocket(socket_);
#else
        ::close(socket_);
#endif
        socket_ = kInvalidSocket;
    }

    std::string host_;
    std::string port_;
    socket_t socket_ = kInvalidSocket;
    std::string inbox_;
    EntityId localId_ = kInvalidEntity;
    bool connected_ = false;
};

}  // namespace

std::unique_ptr<NetClient> makeEosNetClient(const std::string& lobbyId);

std::unique_ptr<NetClient> makeNetClient(const std::string& serverUrl) {
    if (serverUrl.empty()) {
        return nullptr;
    }
    if (serverUrl.rfind("eos:", 0) == 0) {
        return makeEosNetClient(serverUrl.substr(4));
    }

    std::string host;
    std::string port;
    if (!parseUrl(serverUrl, host, port)) {
        return nullptr;
    }
    return std::make_unique<TcpNetClient>(host, port);
}

}  // namespace mega::net
