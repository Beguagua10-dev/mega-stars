#pragma once

#include <memory>
#include <string>

#include "mega/World.h"

namespace mega::net {

/// Transport used by the native editions to talk to a match server. The game
/// loop only ever sees this interface, so the EOS backend can be swapped for a
/// plain socket backend (or for nothing at all, offline) without touching the
/// simulation.
class NetClient {
public:
    virtual ~NetClient() = default;

    virtual bool connect(const std::string& playerName, const std::string& brawlerId) = 0;
    virtual void sendInput(const PlayerInput& input) = 0;

    /// Applies any authoritative state received from the server to `world`.
    virtual void poll(World& world) = 0;

    virtual void disconnect() = 0;
    virtual bool connected() const = 0;

    /// Id the server assigned to this client, or kInvalidEntity before the
    /// welcome message arrives.
    virtual EntityId localPlayerId() const = 0;
};

/// Returns nullptr for an empty `serverUrl` (offline match against bots).
/// `eos:` URLs use the Epic Online Services backend when the binary was built
/// with MEGA_WITH_EOS; otherwise the call fails and the caller falls back to
/// an offline match.
std::unique_ptr<NetClient> makeNetClient(const std::string& serverUrl);

}  // namespace mega::net
