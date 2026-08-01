#include "mega/World.h"

#include <algorithm>
#include <cmath>

namespace mega {
namespace {

constexpr float kPlayerRadius = 0.42f;
constexpr float kProjectileRadius = 0.25f;
constexpr float kGemPickupRadius = 0.85f;
constexpr std::size_t kMaxEvents = 24;
constexpr float kPi = 3.14159265358979323846f;

Vec2 rotate(const Vec2& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

}  // namespace

World::World(const WorldConfig& config)
    : config_(config),
      arena_(config.arenaWidth, config.arenaHeight, config.seed),
      rng_(config.seed) {
    match_.phase = MatchPhase::Warmup;
    match_.phaseTimer = 3.0f;
}

EntityId World::addPlayer(const std::string& name, const std::string& brawlerId, Team team, bool bot) {
    const BrawlerDef& def = findBrawler(brawlerId);

    Player p;
    p.id = nextId_++;
    p.name = name;
    p.brawlerId = def.id;
    p.team = team;
    p.maxHealth = def.maxHealth;
    p.health = def.maxHealth;
    p.ammoCapacity = def.ammoCapacity;
    p.ammo = static_cast<float>(def.ammoCapacity);
    p.bot = bot;
    p.position = arena_.spawnPoint(teamIndex(team), teamPlayerCount(team) % 4);
    p.aim = team == Team::Blue ? Vec2{0.0f, -1.0f} : Vec2{0.0f, 1.0f};

    players_.push_back(p);
    inputs_[p.id] = PlayerInput{};
    pushEvent(name + " entrou na partida");
    return p.id;
}

void World::removePlayer(EntityId id) {
    inputs_.erase(id);
    players_.erase(std::remove_if(players_.begin(), players_.end(),
                                  [id](const Player& p) { return p.id == id; }),
                   players_.end());
}

void World::setInput(EntityId id, const PlayerInput& input) {
    auto it = inputs_.find(id);
    if (it != inputs_.end()) {
        it->second = input;
    }
}

const Player* World::findPlayer(EntityId id) const {
    for (const Player& p : players_) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

Player* World::findPlayerMutable(EntityId id) {
    for (Player& p : players_) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

int World::teamPlayerCount(Team t) const {
    int count = 0;
    for (const Player& p : players_) {
        if (p.team == t) {
            ++count;
        }
    }
    return count;
}

void World::pushEvent(const std::string& text) {
    events_.push_back(GameEvent{match_.elapsed, text});
    while (events_.size() > kMaxEvents) {
        events_.pop_front();
    }
}

void World::step(float dt) {
    dt = clampf(dt, 0.0f, 0.1f);
    match_.elapsed += dt;

    for (Player& p : players_) {
        stepPlayer(p, dt);
    }
    stepProjectiles(dt);
    stepGems(dt);
    stepMatch(dt);

    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                      [](const Projectile& p) { return !p.alive; }),
                       projectiles_.end());
    gems_.erase(std::remove_if(gems_.begin(), gems_.end(),
                               [](const Gem& g) { return !g.alive; }),
                gems_.end());
}

void World::applySnapshot(const Snapshot& snapshot) {
    if (!snapshot.valid) {
        return;
    }
    match_ = snapshot.match;
    projectiles_ = snapshot.projectiles;
    gems_ = snapshot.gems;

    // Keep the input map in sync with the server's player list.
    players_ = snapshot.players;
    for (const Player& p : players_) {
        inputs_.emplace(p.id, PlayerInput{});
    }
}

void World::stepPlayer(Player& player, float dt) {
    const BrawlerDef& def = findBrawler(player.brawlerId);

    if (player.respawnTimer > 0.0f) {
        player.respawnTimer -= dt;
        if (player.respawnTimer <= 0.0f) {
            respawn(player);
        }
        return;
    }
    if (player.health <= 0) {
        return;
    }

    if (player.ammo < static_cast<float>(player.ammoCapacity)) {
        player.ammo = std::min(static_cast<float>(player.ammoCapacity),
                               player.ammo + dt / def.reloadSeconds);
    }
    player.attackCooldown = std::max(0.0f, player.attackCooldown - dt);

    const PlayerInput& in = inputs_[player.id];

    if (in.move.lengthSquared() > 0.0001f) {
        const Vec2 dir = in.move.normalized();
        // Carrying gems is heavy: every gem shaves a little speed off.
        const float gemPenalty = 1.0f - clampf(player.gemsHeld * 0.015f, 0.0f, 0.15f);
        const Vec2 delta = dir * (def.moveSpeed * gemPenalty * dt);
        player.position = arena_.resolveMove(player.position, delta, kPlayerRadius);
    }
    if (in.aim.lengthSquared() > 0.0001f) {
        player.aim = in.aim.normalized();
    }

    if (match_.phase == MatchPhase::Warmup) {
        return;
    }

    if (in.useSuper && player.superCharge >= 100) {
        fireSuper(player);
    } else if (in.shoot && player.ammo >= 1.0f && player.attackCooldown <= 0.0f) {
        fire(player);
    }
}

void World::fire(Player& player) {
    const BrawlerDef& def = findBrawler(player.brawlerId);
    player.ammo -= 1.0f;
    player.attackCooldown = 0.28f;

    const int count = std::max(1, def.projectilesPerShot);
    const float spreadRad = def.spreadDegrees * kPi / 180.0f;

    for (int i = 0; i < count; ++i) {
        const float t = count == 1 ? 0.0f : (static_cast<float>(i) / (count - 1)) - 0.5f;
        const Vec2 dir = rotate(player.aim, t * spreadRad);

        Projectile proj;
        proj.id = nextId_++;
        proj.ownerId = player.id;
        proj.team = player.team;
        proj.position = player.position;
        proj.velocity = dir * def.projectileSpeed;
        proj.remainingRange = def.attackRange;
        proj.damage = def.damagePerProjectile;
        if (def.attack == AttackKind::Lobbed) {
            proj.flags |= static_cast<std::uint8_t>(ProjectileFlags::OverWalls);
        }
        if (def.attack == AttackKind::Healing) {
            proj.flags |= static_cast<std::uint8_t>(ProjectileFlags::Friendly);
        }
        projectiles_.push_back(proj);
    }
}

void World::fireSuper(Player& player) {
    const BrawlerDef& def = findBrawler(player.brawlerId);
    player.superCharge = 0;
    player.attackCooldown = 0.45f;

    // The super is a wider, stronger, longer version of the normal attack.
    const int count = std::max(3, def.projectilesPerShot * 2);
    const float spreadRad = std::max(30.0f, def.spreadDegrees * 1.5f) * kPi / 180.0f;

    for (int i = 0; i < count; ++i) {
        const float t = (static_cast<float>(i) / (count - 1)) - 0.5f;
        const Vec2 dir = rotate(player.aim, t * spreadRad);

        Projectile proj;
        proj.id = nextId_++;
        proj.ownerId = player.id;
        proj.team = player.team;
        proj.position = player.position;
        proj.velocity = dir * (def.projectileSpeed * 1.15f);
        proj.remainingRange = def.attackRange * 1.3f;
        proj.damage = static_cast<int>(def.damagePerProjectile * 1.5f);
        proj.flags |= static_cast<std::uint8_t>(ProjectileFlags::OverWalls);
        if (def.attack == AttackKind::Healing) {
            proj.flags |= static_cast<std::uint8_t>(ProjectileFlags::Friendly);
        }
        projectiles_.push_back(proj);
    }
    pushEvent(player.name + " usou o SUPER");
}

void World::stepProjectiles(float dt) {
    // Fast projectiles would tunnel through players and walls in a single tick,
    // so each one advances in slices no longer than a third of a tile.
    for (Projectile& proj : projectiles_) {
        if (!proj.alive) {
            continue;
        }
        const float travel = proj.velocity.length() * dt;
        const int slices = std::max(1, static_cast<int>(std::ceil(travel / 0.3f)));
        for (int i = 0; i < slices && proj.alive; ++i) {
            advanceProjectile(proj, dt / static_cast<float>(slices));
        }
    }
}

void World::advanceProjectile(Projectile& proj, float dt) {
    const Vec2 step = proj.velocity * dt;
    proj.position += step;
    proj.remainingRange -= step.length();

    if (proj.remainingRange <= 0.0f) {
        proj.alive = false;
        return;
    }
    if (!hasFlag(proj.flags, ProjectileFlags::OverWalls) &&
        arena_.blocksFlatProjectile(proj.position)) {
        proj.alive = false;
        return;
    }

    {
        const bool friendly = hasFlag(proj.flags, ProjectileFlags::Friendly);
        for (Player& target : players_) {
            if (!target.alive() || target.id == proj.ownerId) {
                continue;
            }
            const bool sameTeam = target.team == proj.team;
            if (friendly != sameTeam) {
                continue;
            }
            if (distance(target.position, proj.position) > kPlayerRadius + kProjectileRadius) {
                continue;
            }

            Player* owner = findPlayerMutable(proj.ownerId);
            if (friendly) {
                target.health = std::min(target.maxHealth, target.health + proj.damage);
            } else {
                target.health -= proj.damage;
            }
            if (owner != nullptr) {
                const BrawlerDef& ownerDef = findBrawler(owner->brawlerId);
                owner->superCharge = std::min(100, owner->superCharge + ownerDef.superChargePerHit);
            }
            if (!friendly && target.health <= 0) {
                killPlayer(target, owner);
            }
            proj.alive = false;
            break;
        }
    }
}

void World::killPlayer(Player& victim, Player* killer) {
    victim.health = 0;
    victim.deaths += 1;
    victim.respawnTimer = config_.rules.respawnSeconds;
    dropGems(victim);

    if (killer != nullptr) {
        killer->kills += 1;
        killer->superCharge = std::min(100, killer->superCharge + 20);
        pushEvent(killer->name + " eliminou " + victim.name);
    } else {
        pushEvent(victim.name + " foi eliminado");
    }
}

void World::dropGems(const Player& victim) {
    std::uniform_real_distribution<float> jitter(-1.2f, 1.2f);
    for (int i = 0; i < victim.gemsHeld; ++i) {
        Gem gem;
        gem.id = nextId_++;
        gem.position = arena_.resolveMove(victim.position,
                                          {jitter(rng_), jitter(rng_)}, 0.2f);
        gem.pickupCooldown = 0.8f;
        gems_.push_back(gem);
    }
    const_cast<Player&>(victim).gemsHeld = 0;
}

void World::respawn(Player& player) {
    const BrawlerDef& def = findBrawler(player.brawlerId);
    player.health = def.maxHealth;
    player.ammo = static_cast<float>(def.ammoCapacity);
    player.respawnTimer = 0.0f;
    player.position = arena_.spawnPoint(teamIndex(player.team), static_cast<int>(player.id % 4));
}

void World::spawnGem() {
    std::uniform_real_distribution<float> jitter(-1.5f, 1.5f);
    Gem gem;
    gem.id = nextId_++;
    gem.position = arena_.resolveMove(arena_.center(), {jitter(rng_), jitter(rng_)}, 0.2f);
    gems_.push_back(gem);
}

void World::stepGems(float dt) {
    if (match_.phase == MatchPhase::Playing || match_.phase == MatchPhase::Countdown) {
        gemSpawnTimer_ -= dt;
        if (gemSpawnTimer_ <= 0.0f) {
            gemSpawnTimer_ = config_.rules.gemSpawnSeconds;
            spawnGem();
        }
    }

    for (Gem& gem : gems_) {
        if (!gem.alive) {
            continue;
        }
        gem.pickupCooldown = std::max(0.0f, gem.pickupCooldown - dt);
        if (gem.pickupCooldown > 0.0f) {
            continue;
        }
        for (Player& p : players_) {
            if (!p.alive()) {
                continue;
            }
            if (distance(p.position, gem.position) <= kGemPickupRadius) {
                gem.alive = false;
                p.gemsHeld += 1;
                match_.teamGems[teamIndex(p.team)] += 1;
                break;
            }
        }
    }
}

void World::stepMatch(float dt) {
    switch (match_.phase) {
        case MatchPhase::Warmup:
            match_.phaseTimer -= dt;
            if (match_.phaseTimer <= 0.0f) {
                match_.phase = MatchPhase::Playing;
                match_.phaseTimer = config_.rules.matchSeconds;
                pushEvent("A partida comecou!");
            }
            break;

        case MatchPhase::Playing: {
            match_.phaseTimer -= dt;
            for (int team = 0; team < 2; ++team) {
                if (match_.teamGems[team] >= config_.rules.gemsToWin) {
                    match_.phase = MatchPhase::Countdown;
                    match_.phaseTimer = config_.rules.countdownSeconds;
                    match_.winner = team == 0 ? Team::Blue : Team::Red;
                    pushEvent("Contagem regressiva iniciada!");
                    break;
                }
            }
            if (match_.phase == MatchPhase::Playing && match_.phaseTimer <= 0.0f) {
                match_.phase = MatchPhase::Finished;
                match_.hasWinner = match_.teamGems[0] != match_.teamGems[1];
                match_.winner = match_.teamGems[0] > match_.teamGems[1] ? Team::Blue : Team::Red;
                pushEvent("Tempo esgotado");
            }
            break;
        }

        case MatchPhase::Countdown: {
            const int leadingTeam = teamIndex(match_.winner);
            if (match_.teamGems[leadingTeam] < config_.rules.gemsToWin) {
                match_.phase = MatchPhase::Playing;
                match_.phaseTimer = std::max(10.0f, config_.rules.matchSeconds - match_.elapsed);
                pushEvent("Contagem interrompida!");
                break;
            }
            match_.phaseTimer -= dt;
            if (match_.phaseTimer <= 0.0f) {
                match_.phase = MatchPhase::Finished;
                match_.hasWinner = true;
                pushEvent(match_.winner == Team::Blue ? "Time Azul venceu!" : "Time Vermelho venceu!");
            }
            break;
        }

        case MatchPhase::Finished:
            break;
    }
}

}  // namespace mega
