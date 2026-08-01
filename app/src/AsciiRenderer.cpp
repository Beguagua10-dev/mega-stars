#include <cstdio>
#include <string>
#include <vector>

#include "mega/app/Renderer.h"

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#define MEGA_HAS_TERMIOS 1
#else
#define MEGA_HAS_TERMIOS 0
#endif

namespace mega::app {
namespace {

const char* teamColor(Team team) {
    return team == Team::Blue ? "\033[94m" : "\033[91m";
}

/// Terminal frontend used by `mega-stars --headless` on Linux. It renders the
/// live match as an ASCII grid and reads WASD/IJKL from the raw terminal.
class AsciiRenderer final : public Renderer {
public:
    explicit AsciiRenderer(const Profile& profile) : profile_(profile) { enableRawMode(); }

    ~AsciiRenderer() override { shutdown(); }

    bool pollInput(PlayerInput& out) override {
        out = PlayerInput{};
        move_ = Vec2{};

        char buffer[64];
        const int count = readAvailable(buffer, sizeof(buffer));
        for (int i = 0; i < count; ++i) {
            switch (buffer[i]) {
                case 'w': move_.y -= 1.0f; break;
                case 's': move_.y += 1.0f; break;
                case 'a': move_.x -= 1.0f; break;
                case 'd': move_.x += 1.0f; break;
                case 'i': aim_ = Vec2{0.0f, -1.0f}; break;
                case 'k': aim_ = Vec2{0.0f, 1.0f}; break;
                case 'j': aim_ = Vec2{-1.0f, 0.0f}; break;
                case 'l': aim_ = Vec2{1.0f, 0.0f}; break;
                case ' ': shoot_ = true; break;
                case 'e': super_ = true; break;
                case 'q':
                case 3:  // Ctrl+C
                    return false;
                default:
                    break;
            }
        }

        out.move = move_;
        out.aim = aim_;
        out.shoot = shoot_;
        out.useSuper = super_;
        shoot_ = false;
        super_ = false;
        return true;
    }

    void draw(const World& world, EntityId localPlayer) override {
        const Arena& arena = world.arena();
        std::vector<std::string> grid(
            static_cast<std::size_t>(arena.height()),
            std::string(static_cast<std::size_t>(arena.width()), ' '));

        for (int y = 0; y < arena.height(); ++y) {
            for (int x = 0; x < arena.width(); ++x) {
                char c = '.';
                switch (arena.at(x, y)) {
                    case Tile::Wall: c = '#'; break;
                    case Tile::Bush: c = '%'; break;
                    case Tile::Fence: c = '='; break;
                    case Tile::Floor: c = ' '; break;
                }
                grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = c;
            }
        }

        std::string out = "\033[H\033[2J";
        out += "  MEGA STARS - Linux Edition (ASCII)\n";

        for (const Gem& gem : world.gems()) {
            place(grid, gem.position, '*');
        }
        for (const Projectile& p : world.projectiles()) {
            place(grid, p.position, '-');
        }

        // Players are drawn as coloured letters, so they need per-cell colour
        // codes instead of the plain grid characters.
        std::vector<std::string> overlay(grid.size());
        for (const Player& p : world.players()) {
            if (!p.alive()) {
                continue;
            }
            const int x = static_cast<int>(p.position.x);
            const int y = static_cast<int>(p.position.y);
            if (y < 0 || y >= static_cast<int>(grid.size()) || x < 0 || x >= arena.width()) {
                continue;
            }
            const char symbol = p.id == localPlayer
                                    ? '@'
                                    : static_cast<char>(std::toupper(p.name.empty() ? 'B' : p.name[0]));
            grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = symbol;
            overlay[static_cast<std::size_t>(y)] = teamColor(p.team);
        }

        for (std::size_t y = 0; y < grid.size(); ++y) {
            out += "  ";
            out += overlay[y].empty() ? "" : overlay[y];
            out += grid[y];
            out += "\033[0m\n";
        }

        out += "  " + scoreLine(world) + "\n";
        const Player* me = world.findPlayer(localPlayer);
        if (me != nullptr) {
            out += "  HP " + std::to_string(me->health) + "/" + std::to_string(me->maxHealth) +
                   "  MUNICAO " + std::to_string(static_cast<int>(me->ammo)) + "/" +
                   std::to_string(me->ammoCapacity) + "  SUPER " + std::to_string(me->superCharge) +
                   "%  CRISTAIS " + std::to_string(me->gemsHeld) + "\n";
        }
        if (profile_.showKillFeed) {
            int shown = 0;
            for (auto it = world.events().rbegin(); it != world.events().rend() && shown < 3;
                 ++it, ++shown) {
                out += "  > " + it->text + "\n";
            }
        }
        out += "  [WASD] mover  [IJKL] mirar  [ESPACO] atirar  [E] super  [Q] sair\n";

        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }

    void shutdown() override { disableRawMode(); }

private:
    static void place(std::vector<std::string>& grid, const Vec2& pos, char c) {
        const int x = static_cast<int>(pos.x);
        const int y = static_cast<int>(pos.y);
        if (y < 0 || y >= static_cast<int>(grid.size())) {
            return;
        }
        if (x < 0 || x >= static_cast<int>(grid[static_cast<std::size_t>(y)].size())) {
            return;
        }
        grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = c;
    }

    static std::string scoreLine(const World& world) {
        const MatchState& m = world.match();
        std::string phase;
        switch (m.phase) {
            case MatchPhase::Warmup: phase = "AQUECIMENTO"; break;
            case MatchPhase::Playing: phase = "EM JOGO"; break;
            case MatchPhase::Countdown: phase = "CONTAGEM"; break;
            case MatchPhase::Finished: phase = "FIM"; break;
        }
        return "AZUL " + std::to_string(m.teamGems[0]) + " x " + std::to_string(m.teamGems[1]) +
               " VERMELHO   " + phase + " " + std::to_string(static_cast<int>(m.phaseTimer)) + "s";
    }

    int readAvailable(char* buffer, std::size_t size) {
#if MEGA_HAS_TERMIOS
        const ssize_t n = ::read(STDIN_FILENO, buffer, size);
        return n > 0 ? static_cast<int>(n) : 0;
#else
        (void)buffer;
        (void)size;
        return 0;
#endif
    }

    void enableRawMode() {
#if MEGA_HAS_TERMIOS
        if (::tcgetattr(STDIN_FILENO, &original_) != 0) {
            return;
        }
        rawModeActive_ = true;
        termios raw = original_;
        raw.c_lflag &= ~(static_cast<tcflag_t>(ECHO | ICANON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        std::fputs("\033[?25l", stdout);  // hide cursor
#endif
    }

    void disableRawMode() {
#if MEGA_HAS_TERMIOS
        if (!rawModeActive_) {
            return;
        }
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_);
        rawModeActive_ = false;
        std::fputs("\033[?25h\033[0m\n", stdout);
        std::fflush(stdout);
#endif
    }

    Profile profile_;
    Vec2 move_;
    Vec2 aim_{1.0f, 0.0f};
    bool shoot_ = false;
    bool super_ = false;
#if MEGA_HAS_TERMIOS
    termios original_{};
    bool rawModeActive_ = false;
#endif
};

}  // namespace

std::unique_ptr<Renderer> makeAsciiRenderer(const Profile& profile) {
    return std::make_unique<AsciiRenderer>(profile);
}

}  // namespace mega::app
