#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mega/Math.h"

namespace mega {

using EntityId = std::uint32_t;
constexpr EntityId kInvalidEntity = 0;

enum class Team : std::uint8_t { Blue = 0, Red = 1 };

/// Attack shapes a brawler archetype can use. All values are original designs;
/// no third-party character is referenced.
enum class AttackKind : std::uint8_t {
    SingleShot,  // one straight projectile
    Spread,      // several projectiles in a cone
    Lobbed,      // arcs over walls and explodes on landing
    Burst,       // fast sequence of weak projectiles
    Beam,        // long range, high damage, slow reload
    Healing,     // friendly projectile that restores health
};

/// Static definition of a playable character.
struct BrawlerDef {
    std::string id;
    std::string displayName;
    AttackKind attack = AttackKind::SingleShot;
    int maxHealth = 3600;
    float moveSpeed = 4.2f;         // tiles per second
    float attackRange = 7.0f;       // tiles
    float reloadSeconds = 0.9f;     // per ammo slot
    int ammoCapacity = 3;
    int projectilesPerShot = 1;
    float spreadDegrees = 0.0f;
    int damagePerProjectile = 1000;
    float projectileSpeed = 12.0f;
    int superChargePerHit = 12;     // percent
    std::uint32_t colorRgb = 0xFFFFFF;
};

enum class ProjectileFlags : std::uint8_t {
    None = 0,
    OverWalls = 1 << 0,
    Friendly = 1 << 1,
};

inline bool hasFlag(std::uint8_t flags, ProjectileFlags f) {
    return (flags & static_cast<std::uint8_t>(f)) != 0;
}

struct Projectile {
    EntityId id = kInvalidEntity;
    EntityId ownerId = kInvalidEntity;
    Team team = Team::Blue;
    Vec2 position;
    Vec2 velocity;
    float remainingRange = 0.0f;
    int damage = 0;
    std::uint8_t flags = 0;
    bool alive = true;
};

struct Gem {
    EntityId id = kInvalidEntity;
    Vec2 position;
    float pickupCooldown = 0.0f;  // seconds before it can be picked up
    bool alive = true;
};

/// Per-tick intent coming from a human player or a bot.
struct PlayerInput {
    Vec2 move;      // normalized movement direction
    Vec2 aim;       // normalized aim direction
    bool shoot = false;
    bool useSuper = false;
};

struct Player {
    EntityId id = kInvalidEntity;
    std::string name;
    std::string brawlerId;
    Team team = Team::Blue;
    Vec2 position;
    Vec2 aim{1.0f, 0.0f};
    int health = 0;
    int maxHealth = 0;
    float ammo = 0.0f;        // fractional: regenerates continuously
    int ammoCapacity = 0;
    int superCharge = 0;      // 0..100
    float respawnTimer = 0.0f;
    float attackCooldown = 0.0f;
    int gemsHeld = 0;
    int kills = 0;
    int deaths = 0;
    bool bot = false;
    bool connected = true;

    bool alive() const { return health > 0 && respawnTimer <= 0.0f; }
};

enum class MatchPhase : std::uint8_t { Warmup, Playing, Countdown, Finished };

struct MatchState {
    MatchPhase phase = MatchPhase::Warmup;
    float phaseTimer = 0.0f;
    float elapsed = 0.0f;
    int teamGems[2] = {0, 0};
    Team winner = Team::Blue;
    bool hasWinner = false;
};

}  // namespace mega
