#include "mega/Arena.h"

#include <algorithm>
#include <random>

namespace mega {
namespace {
constexpr float kEpsilon = 0.001f;
}

Arena::Arena(int width, int height, std::uint32_t seed)
    : width_(std::max(width, 12)), height_(std::max(height, 12)) {
    tiles_.assign(static_cast<std::size_t>(width_) * height_, Tile::Floor);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);

    for (int x = 0; x < width_; ++x) {
        set(x, 0, Tile::Wall);
        set(x, height_ - 1, Tile::Wall);
    }
    for (int y = 0; y < height_; ++y) {
        set(0, y, Tile::Wall);
        set(width_ - 1, y, Tile::Wall);
    }

    // Only the top half is generated; the bottom half mirrors it so neither
    // team gets a positional advantage.
    const int halfHeight = height_ / 2;
    for (int y = 2; y < halfHeight; ++y) {
        for (int x = 2; x < width_ - 2; ++x) {
            const float roll = chance(rng);
            Tile t = Tile::Floor;
            if (roll < 0.10f) {
                t = Tile::Wall;
            } else if (roll < 0.20f) {
                t = Tile::Bush;
            } else if (roll < 0.23f) {
                t = Tile::Fence;
            }
            if (t == Tile::Floor) {
                continue;
            }
            set(x, y, t);
            set(width_ - 1 - x, height_ - 1 - y, t);
        }
    }

    // Keep the gem spawn and both team spawns clear.
    const int cx = width_ / 2;
    const int cy = height_ / 2;
    for (int y = cy - 2; y <= cy + 2; ++y) {
        for (int x = cx - 2; x <= cx + 2; ++x) {
            clearMirrored(x, y);
        }
    }
    for (int slot = 0; slot < 4; ++slot) {
        for (int team = 0; team < 2; ++team) {
            const Vec2 sp = spawnPoint(team, slot);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    clearMirrored(static_cast<int>(sp.x) + dx, static_cast<int>(sp.y) + dy);
                }
            }
        }
    }
}

Arena::Arena(int width, int height, const std::vector<Tile>& tiles)
    : width_(std::max(width, 1)), height_(std::max(height, 1)) {
    tiles_.assign(static_cast<std::size_t>(width_) * height_, Tile::Wall);
    const std::size_t count = std::min(tiles_.size(), tiles.size());
    std::copy(tiles.begin(), tiles.begin() + static_cast<std::ptrdiff_t>(count), tiles_.begin());
}

void Arena::clearMirrored(int x, int y) {
    set(x, y, Tile::Floor);
    set(width_ - 1 - x, height_ - 1 - y, Tile::Floor);
}

void Arena::set(int x, int y, Tile t) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    const bool border = x == 0 || y == 0 || x == width_ - 1 || y == height_ - 1;
    if (border && t != Tile::Wall) {
        return;  // the outer ring always stays solid
    }
    tiles_[static_cast<std::size_t>(y) * width_ + x] = t;
}

Tile Arena::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return Tile::Wall;
    }
    return tiles_[static_cast<std::size_t>(y) * width_ + x];
}

Tile Arena::atWorld(const Vec2& p) const {
    return at(static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y)));
}

bool Arena::blocksMovement(const Vec2& p) const {
    return atWorld(p) == Tile::Wall;
}

bool Arena::blocksFlatProjectile(const Vec2& p) const {
    const Tile t = atWorld(p);
    return t == Tile::Wall || t == Tile::Fence;
}

bool Arena::hidesPlayers(const Vec2& p) const {
    return atWorld(p) == Tile::Bush;
}

bool Arena::circleBlocked(const Vec2& p, float radius) const {
    const float offsets[3] = {-radius, 0.0f, radius};
    for (float dx : offsets) {
        for (float dy : offsets) {
            if (blocksMovement({p.x + dx, p.y + dy})) {
                return true;
            }
        }
    }
    return false;
}

Vec2 Arena::resolveMove(const Vec2& from, const Vec2& delta, float radius) const {
    Vec2 result = from;

    Vec2 tryX{result.x + delta.x, result.y};
    if (!circleBlocked(tryX, radius)) {
        result = tryX;
    }

    Vec2 tryY{result.x, result.y + delta.y};
    if (!circleBlocked(tryY, radius)) {
        result = tryY;
    }

    result.x = clampf(result.x, 1.0f + radius + kEpsilon, width_ - 1.0f - radius - kEpsilon);
    result.y = clampf(result.y, 1.0f + radius + kEpsilon, height_ - 1.0f - radius - kEpsilon);
    return result;
}

Vec2 Arena::spawnPoint(int teamIndex, int slot) const {
    const float spread = static_cast<float>(slot - 1) * 2.0f;
    const float x = clampf(width_ * 0.5f + spread, 2.0f, width_ - 3.0f);
    const float y = teamIndex == 0 ? height_ - 3.0f : 3.0f;
    return {x + 0.5f, y + 0.5f};
}

}  // namespace mega
