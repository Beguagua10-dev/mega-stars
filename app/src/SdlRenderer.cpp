#include "mega/app/Renderer.h"

#ifndef MEGA_WITH_SDL

namespace mega::app {
std::unique_ptr<Renderer> makeSdlRenderer(const Profile&) {
    return nullptr;  // built without SDL2
}
}  // namespace mega::app

#else

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mega::app {
namespace {

struct Color {
    Uint8 r, g, b, a;
};

constexpr Color kBackground{26, 32, 44, 255};
constexpr Color kFloor{74, 124, 89, 255};
constexpr Color kFloorAlt{86, 138, 100, 255};
constexpr Color kWall{92, 66, 48, 255};
constexpr Color kWallTop{124, 92, 66, 255};
constexpr Color kBush{40, 96, 62, 255};
constexpr Color kFence{176, 150, 92, 255};
constexpr Color kGem{140, 82, 255, 255};
constexpr Color kBlue{62, 132, 232, 255};
constexpr Color kRed{226, 87, 76, 255};

Color fromRgb(std::uint32_t rgb) {
    return Color{static_cast<Uint8>((rgb >> 16) & 0xFF), static_cast<Uint8>((rgb >> 8) & 0xFF),
                 static_cast<Uint8>(rgb & 0xFF), 255};
}

void setColor(SDL_Renderer* r, const Color& c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void fillCircle(SDL_Renderer* r, float cx, float cy, float radius) {
    const int rad = static_cast<int>(radius);
    for (int dy = -rad; dy <= rad; ++dy) {
        const int dx = static_cast<int>(std::sqrt(static_cast<float>(rad * rad - dy * dy)));
        SDL_RenderDrawLine(r, static_cast<int>(cx) - dx, static_cast<int>(cy) + dy,
                           static_cast<int>(cx) + dx, static_cast<int>(cy) + dy);
    }
}

/// Chunky top-down look inspired by modern arena brawlers, drawn entirely from
/// primitives so the game ships with zero third-party art.
class SdlRenderer final : public Renderer {
public:
    explicit SdlRenderer(const Profile& profile) : profile_(profile) {}

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }
        Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        if (profile_.touchControls) {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        window_ = SDL_CreateWindow("Mega Stars", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   profile_.windowWidth, profile_.windowHeight, flags);
        if (window_ == nullptr) {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }
        Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
        if (profile_.edition != Edition::Pocket) {
            rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
        }
        renderer_ = SDL_CreateRenderer(window_, -1, rendererFlags);
        if (renderer_ == nullptr) {
            // Old iMac 2011 GPUs and low-end phones fall back to software.
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        }
        return renderer_ != nullptr;
    }

    bool pollInput(PlayerInput& out) override {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                return false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                return false;
            }
            handleTouch(event);
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Vec2 move;
        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) move.y -= 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) move.y += 1.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) move.x -= 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) move.x += 1.0f;

        int mouseX = 0;
        int mouseY = 0;
        const Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);

        out.move = move.lengthSquared() > 0.0f ? move : touchMove_;
        out.aim = touchAim_.lengthSquared() > 0.0f
                      ? touchAim_
                      : screenToWorldDirection(static_cast<float>(mouseX), static_cast<float>(mouseY));
        out.shoot = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0 || keys[SDL_SCANCODE_SPACE] ||
                    touchShoot_;
        out.useSuper = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0 || keys[SDL_SCANCODE_E] ||
                       touchSuper_;
        touchSuper_ = false;
        return true;
    }

    void draw(const World& world, EntityId localPlayer) override {
        int width = 0;
        int height = 0;
        SDL_GetRendererOutputSize(renderer_, &width, &height);

        const Player* me = world.findPlayer(localPlayer);
        const Vec2 target = me != nullptr ? me->position : world.arena().center();
        if (profile_.smoothCamera) {
            camera_ = camera_ + (target - camera_) * 0.15f;
        } else {
            camera_ = target;
        }
        viewWidth_ = width;
        viewHeight_ = height;

        setColor(renderer_, kBackground);
        SDL_RenderClear(renderer_);

        drawArena(world.arena());
        drawGems(world);
        drawProjectiles(world);
        drawPlayers(world, localPlayer);
        drawHud(world, me);
        if (profile_.touchControls) {
            drawTouchControls();
        }

        SDL_RenderPresent(renderer_);
    }

    void shutdown() override {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    }

private:
    SDL_FPoint worldToScreen(const Vec2& p) const {
        const float scale = profile_.tilePixels;
        return SDL_FPoint{(p.x - camera_.x) * scale + viewWidth_ * 0.5f,
                          (p.y - camera_.y) * scale + viewHeight_ * 0.5f};
    }

    Vec2 screenToWorldDirection(float sx, float sy) const {
        return Vec2{sx - viewWidth_ * 0.5f, sy - viewHeight_ * 0.5f}.normalized();
    }

    void drawArena(const Arena& arena) {
        const float scale = profile_.tilePixels;
        for (int y = 0; y < arena.height(); ++y) {
            for (int x = 0; x < arena.width(); ++x) {
                const SDL_FPoint tl = worldToScreen(Vec2{static_cast<float>(x), static_cast<float>(y)});
                if (tl.x < -scale || tl.y < -scale || tl.x > viewWidth_ || tl.y > viewHeight_) {
                    continue;
                }
                SDL_Rect rect{static_cast<int>(tl.x), static_cast<int>(tl.y),
                              static_cast<int>(scale) + 1, static_cast<int>(scale) + 1};
                switch (arena.at(x, y)) {
                    case Tile::Floor:
                        setColor(renderer_, ((x + y) % 2 == 0) ? kFloor : kFloorAlt);
                        SDL_RenderFillRect(renderer_, &rect);
                        break;
                    case Tile::Wall: {
                        setColor(renderer_, kWall);
                        SDL_RenderFillRect(renderer_, &rect);
                        SDL_Rect top = rect;
                        top.h = static_cast<int>(scale * 0.35f);
                        setColor(renderer_, kWallTop);
                        SDL_RenderFillRect(renderer_, &top);
                        break;
                    }
                    case Tile::Bush:
                        setColor(renderer_, kFloor);
                        SDL_RenderFillRect(renderer_, &rect);
                        setColor(renderer_, kBush);
                        fillCircle(renderer_, tl.x + scale * 0.5f, tl.y + scale * 0.5f, scale * 0.48f);
                        break;
                    case Tile::Fence: {
                        setColor(renderer_, kFloor);
                        SDL_RenderFillRect(renderer_, &rect);
                        setColor(renderer_, kFence);
                        SDL_Rect bar{rect.x, rect.y + rect.h / 3, rect.w, rect.h / 3};
                        SDL_RenderFillRect(renderer_, &bar);
                        break;
                    }
                }
            }
        }
    }

    void drawGems(const World& world) {
        const float scale = profile_.tilePixels;
        for (const Gem& gem : world.gems()) {
            const SDL_FPoint p = worldToScreen(gem.position);
            setColor(renderer_, kGem);
            const float r = scale * 0.22f;
            // Diamond shape.
            for (int i = -static_cast<int>(r); i <= static_cast<int>(r); ++i) {
                const int halfWidth = static_cast<int>(r) - std::abs(i);
                SDL_RenderDrawLine(renderer_, static_cast<int>(p.x) - halfWidth,
                                   static_cast<int>(p.y) + i, static_cast<int>(p.x) + halfWidth,
                                   static_cast<int>(p.y) + i);
            }
        }
    }

    void drawProjectiles(const World& world) {
        const float scale = profile_.tilePixels;
        for (const Projectile& proj : world.projectiles()) {
            const SDL_FPoint p = worldToScreen(proj.position);
            setColor(renderer_, proj.team == Team::Blue ? kBlue : kRed);
            fillCircle(renderer_, p.x, p.y, scale * 0.14f);
        }
    }

    void drawPlayers(const World& world, EntityId localPlayer) {
        const float scale = profile_.tilePixels;
        for (const Player& player : world.players()) {
            if (!player.alive()) {
                continue;
            }
            const SDL_FPoint p = worldToScreen(player.position);
            const BrawlerDef& def = findBrawler(player.brawlerId);

            // Team ring, then the character body in its own colour.
            setColor(renderer_, player.team == Team::Blue ? kBlue : kRed);
            fillCircle(renderer_, p.x, p.y, scale * 0.46f);
            setColor(renderer_, fromRgb(def.colorRgb));
            fillCircle(renderer_, p.x, p.y, scale * 0.36f);

            // Aim indicator.
            setColor(renderer_, Color{255, 255, 255, 255});
            SDL_RenderDrawLine(renderer_, static_cast<int>(p.x), static_cast<int>(p.y),
                               static_cast<int>(p.x + player.aim.x * scale * 0.6f),
                               static_cast<int>(p.y + player.aim.y * scale * 0.6f));

            drawHealthBar(p.x, p.y - scale * 0.7f, scale,
                          static_cast<float>(player.health) / static_cast<float>(player.maxHealth),
                          player.team, player.id == localPlayer);

            for (int i = 0; i < player.gemsHeld && i < 10; ++i) {
                setColor(renderer_, kGem);
                SDL_Rect g{static_cast<int>(p.x - scale * 0.4f + i * 5), static_cast<int>(p.y - scale), 4, 4};
                SDL_RenderFillRect(renderer_, &g);
            }
        }
    }

    void drawHealthBar(float cx, float cy, float scale, float ratio, Team team, bool isLocal) {
        const int w = static_cast<int>(scale * 0.9f);
        const int h = std::max(4, static_cast<int>(scale * 0.14f));
        SDL_Rect back{static_cast<int>(cx) - w / 2, static_cast<int>(cy), w, h};
        setColor(renderer_, Color{18, 18, 24, 220});
        SDL_RenderFillRect(renderer_, &back);

        SDL_Rect fill = back;
        fill.w = static_cast<int>(w * clampf(ratio, 0.0f, 1.0f));
        setColor(renderer_, isLocal ? Color{90, 220, 120, 255} : (team == Team::Blue ? kBlue : kRed));
        SDL_RenderFillRect(renderer_, &fill);
    }

    void drawHud(const World& world, const Player* me) {
        const MatchState& match = world.match();
        // Score bar: one pip per gem, blue on the left, red on the right.
        const int pip = 14;
        for (int team = 0; team < 2; ++team) {
            for (int i = 0; i < match.teamGems[team] && i < 20; ++i) {
                SDL_Rect r{team == 0 ? 20 + i * (pip + 3) : viewWidth_ - 20 - (i + 1) * (pip + 3),
                           20, pip, pip};
                setColor(renderer_, team == 0 ? kBlue : kRed);
                SDL_RenderFillRect(renderer_, &r);
            }
        }

        if (me == nullptr) {
            return;
        }
        // Ammo pips at the bottom left, super meter above them.
        for (int i = 0; i < me->ammoCapacity; ++i) {
            SDL_Rect r{24 + i * 26, viewHeight_ - 60, 20, 34};
            setColor(renderer_, static_cast<float>(i) < me->ammo ? Color{240, 220, 120, 255}
                                                                 : Color{60, 60, 70, 200});
            SDL_RenderFillRect(renderer_, &r);
        }
        SDL_Rect superBack{24, viewHeight_ - 84, 20 * 3 + 52, 14};
        setColor(renderer_, Color{40, 40, 50, 220});
        SDL_RenderFillRect(renderer_, &superBack);
        SDL_Rect superFill = superBack;
        superFill.w = static_cast<int>(superBack.w * (me->superCharge / 100.0f));
        setColor(renderer_, me->superCharge >= 100 ? Color{255, 214, 64, 255} : Color{120, 180, 240, 255});
        SDL_RenderFillRect(renderer_, &superFill);
    }

    void drawTouchControls() {
        setColor(renderer_, Color{255, 255, 255, 60});
        fillCircle(renderer_, joystickOrigin_.x, joystickOrigin_.y, 70.0f);
        setColor(renderer_, Color{255, 255, 255, 120});
        fillCircle(renderer_, joystickOrigin_.x + touchMove_.x * 40.0f,
                   joystickOrigin_.y + touchMove_.y * 40.0f, 30.0f);

        setColor(renderer_, Color{240, 180, 80, 110});
        fillCircle(renderer_, static_cast<float>(viewWidth_) - 120.0f,
                   static_cast<float>(viewHeight_) - 120.0f, 70.0f);
    }

    void handleTouch(const SDL_Event& event) {
        if (!profile_.touchControls) {
            return;
        }
        if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERMOTION &&
            event.type != SDL_FINGERUP) {
            return;
        }
        const float x = event.tfinger.x * viewWidth_;
        const float y = event.tfinger.y * viewHeight_;
        const bool leftHalf = x < viewWidth_ * 0.5f;

        if (event.type == SDL_FINGERUP) {
            if (leftHalf) {
                touchMove_ = Vec2{};
            } else {
                touchShoot_ = false;
                touchAim_ = Vec2{};
            }
            return;
        }
        if (leftHalf) {
            if (event.type == SDL_FINGERDOWN) {
                joystickOrigin_ = Vec2{x, y};
            }
            const Vec2 delta{x - joystickOrigin_.x, y - joystickOrigin_.y};
            touchMove_ = delta.length() > 8.0f ? delta.normalized() : Vec2{};
        } else {
            const Vec2 origin{viewWidth_ - 120.0f, viewHeight_ - 120.0f};
            const Vec2 delta{x - origin.x, y - origin.y};
            touchAim_ = delta.length() > 8.0f ? delta.normalized() : touchAim_;
            touchShoot_ = true;
        }
    }

    Profile profile_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Vec2 camera_;
    int viewWidth_ = 1280;
    int viewHeight_ = 720;
    Vec2 touchMove_;
    Vec2 touchAim_;
    Vec2 joystickOrigin_{140.0f, 580.0f};
    bool touchShoot_ = false;
    bool touchSuper_ = false;
};

}  // namespace

std::unique_ptr<Renderer> makeSdlRenderer(const Profile& profile) {
    auto renderer = std::make_unique<SdlRenderer>(profile);
    if (!renderer->init()) {
        return nullptr;
    }
    return renderer;
}

}  // namespace mega::app

#endif  // MEGA_WITH_SDL
