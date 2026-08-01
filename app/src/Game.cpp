#include "mega/app/Game.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "mega/BotBrain.h"
#include "mega/Brawlers.h"
#include "mega/World.h"
#include "mega/app/Renderer.h"
#include "mega/net/NetClient.h"

namespace mega::app {
namespace {

const char* const kBotNames[] = {"Zeca", "Nina", "Tobias", "Lia", "Rui", "Dara"};

bool matchArg(const std::string& arg, const char* name, const char* value, std::string& out) {
    const std::string prefix = std::string(name) + "=";
    if (arg.rfind(prefix, 0) == 0) {
        out = arg.substr(prefix.size());
        return true;
    }
    if (arg == name && value != nullptr) {
        out = value;
        return true;
    }
    return false;
}

}  // namespace

std::string usageText(Edition edition) {
    std::string text = "Mega Stars - ";
    text += Profile::editionName(edition);
    text +=
        "\n\nUso: mega-stars [opcoes]\n"
        "  --help                 mostra esta ajuda\n"
        "  --name=<nome>          nome do jogador\n"
        "  --brawler=<id>         personagem inicial\n"
        "  --brawlers             lista os personagens disponiveis\n"
        "  --server=<url>         conecta a um servidor (vazio = partida offline)\n"
        "  --seed=<n>             semente da arena\n";
    if (edition == Edition::Pc) {
        text += "  --headless             roda no terminal em ASCII (Linux/macOS)\n";
    }
    return text;
}

bool parseOptions(int argc, char** argv, Edition edition, Options& out, std::string& error) {
    out.profile = Profile::forEdition(edition);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const char* next = (i + 1 < argc) ? argv[i + 1] : nullptr;
        std::string value;

        if (arg == "--help" || arg == "-h") {
            out.showHelp = true;
        } else if (arg == "--headless") {
            out.headless = true;
        } else if (arg == "--brawlers") {
            out.listBrawlers = true;
        } else if (matchArg(arg, "--name", next, value)) {
            out.playerName = value;
            if (arg == "--name") ++i;
        } else if (matchArg(arg, "--brawler", next, value)) {
            out.brawlerId = value;
            if (arg == "--brawler") ++i;
        } else if (matchArg(arg, "--server", next, value)) {
            out.serverUrl = value;
            if (arg == "--server") ++i;
        } else if (matchArg(arg, "--seed", next, value)) {
            out.seed = static_cast<std::uint32_t>(std::stoul(value));
            if (arg == "--seed") ++i;
        } else {
            error = "opcao desconhecida: " + arg;
            return false;
        }
    }
    return true;
}

int runGame(const Options& options) {
    if (options.showHelp) {
        std::fputs(usageText(options.profile.edition).c_str(), stdout);
        return 0;
    }
    if (options.listBrawlers) {
        std::puts("Personagens disponiveis:");
        for (const BrawlerDef& def : brawlerRoster()) {
            std::printf("  %-10s %-10s vida %5d  alcance %.1f\n", def.id.c_str(),
                        def.displayName.c_str(), def.maxHealth, def.attackRange);
        }
        return 0;
    }

    WorldConfig config;
    config.seed = options.seed;
    if (options.profile.edition == Edition::Pocket) {
        // Smaller arena keeps the draw cost down on very old phones.
        config.arenaWidth = 24;
        config.arenaHeight = 26;
    }
    World world(config);

    std::unique_ptr<net::NetClient> network = net::makeNetClient(options.serverUrl);
    if (network != nullptr && !network->connect(options.playerName, options.brawlerId)) {
        std::fprintf(stderr, "Nao foi possivel conectar em %s; jogando offline.\n",
                     options.serverUrl.c_str());
        network.reset();
    }

    // Offline matches are simulated locally against bots; online matches only
    // render the state the server sends.
    EntityId localPlayer = kInvalidEntity;
    const bool offline = network == nullptr;
    if (offline) {
        localPlayer = world.addPlayer(options.playerName, options.brawlerId, Team::Blue, false);
        const std::vector<BrawlerDef>& roster = brawlerRoster();
        for (int i = 0; i < options.profile.maxBots; ++i) {
            const Team team = (i % 2 == 0) ? Team::Red : Team::Blue;
            world.addPlayer(kBotNames[i % 6], roster[(i + 1) % roster.size()].id, team, true);
        }
    }

    std::unique_ptr<Renderer> renderer;
    if (options.headless) {
        renderer = makeAsciiRenderer(options.profile);
    } else {
        renderer = makeSdlRenderer(options.profile);
        if (renderer == nullptr) {
            std::fputs("SDL2 indisponivel; usando o modo ASCII.\n", stderr);
            renderer = makeAsciiRenderer(options.profile);
        }
    }

    const auto tickDuration =
        std::chrono::duration<double>(1.0 / static_cast<double>(options.profile.tickRate));
    auto previous = std::chrono::steady_clock::now();
    bool running = true;

    while (running) {
        const auto frameStart = std::chrono::steady_clock::now();
        const float dt =
            std::chrono::duration<float>(frameStart - previous).count();
        previous = frameStart;

        PlayerInput input;
        running = renderer->pollInput(input);
        if (offline) {
            world.setInput(localPlayer, input);
            driveBots(world);
            world.step(dt);
        } else {
            network->sendInput(input);
            network->poll(world);
            localPlayer = network->localPlayerId();
            if (!network->connected()) {
                std::fputs("Conexao encerrada pelo servidor.\n", stderr);
                running = false;
            }
        }
        renderer->draw(world, localPlayer);

        if (offline && world.match().phase == MatchPhase::Finished &&
            world.match().phaseTimer < -5.0f) {
            running = false;
        }

        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed < tickDuration) {
            std::this_thread::sleep_for(tickDuration - elapsed);
        }
    }

    renderer->shutdown();
    if (network != nullptr) {
        network->disconnect();
    }

    const MatchState& match = world.match();
    std::printf("\nPlacar final - Azul %d x %d Vermelho\n", match.teamGems[0], match.teamGems[1]);
    return 0;
}

}  // namespace mega::app
