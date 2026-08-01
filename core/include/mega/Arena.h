#pragma once

#include <cstdint>
#include <vector>

#include "mega/Math.h"

namespace mega {

enum class Tile : std::uint8_t {
    Floor = 0,
    Wall = 1,   // blocks movement and flat projectiles
    Bush = 2,   // hides players, does not block
    Fence = 3,  // blocks flat projectiles but not movement
};

/// Point-symmetric tile arena. Both teams always get a mirrored layout.
class Arena {
public:
    Arena(int width, int height, std::uint32_t seed);

    /// Builds an arena from tiles received from a server.
    Arena(int width, int height, const std::vector<Tile>& tiles);

    int width() const { return width_; }
    int height() const { return height_; }

    Tile at(int x, int y) const;
    Tile atWorld(const Vec2& p) const;

    bool blocksMovement(const Vec2& p) const;
    bool blocksFlatProjectile(const Vec2& p) const;
    bool hidesPlayers(const Vec2& p) const;

    /// Slides `from` towards `from + delta`, stopping at walls (axis separated
    /// so players glide along obstacles instead of sticking to them).
    Vec2 resolveMove(const Vec2& from, const Vec2& delta, float radius) const;

    Vec2 center() const { return {width_ * 0.5f, height_ * 0.5f}; }
    Vec2 spawnPoint(int teamIndex, int slot) const;

    const std::vector<Tile>& tiles() const { return tiles_; }

private:
    bool circleBlocked(const Vec2& p, float radius) const;
    void set(int x, int y, Tile t);
    void clearMirrored(int x, int y);

    int width_;
    int height_;
    std::vector<Tile> tiles_;
};

}  // namespace mega
