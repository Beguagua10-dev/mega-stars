#include "mega/BotBrain.h"

#include <limits>

namespace mega {
namespace {

const Player* nearestEnemy(const World& world, const Player& self, float* outDistance) {
    const Player* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (const Player& other : world.players()) {
        if (other.team == self.team || !other.alive()) {
            continue;
        }
        const float d = distance(other.position, self.position);
        if (d < bestDist) {
            bestDist = d;
            best = &other;
        }
    }
    if (outDistance != nullptr) {
        *outDistance = bestDist;
    }
    return best;
}

const Gem* nearestGem(const World& world, const Player& self) {
    const Gem* best = nullptr;
    float bestDist = std::numeric_limits<float>::max();
    for (const Gem& gem : world.gems()) {
        if (!gem.alive) {
            continue;
        }
        const float d = distance(gem.position, self.position);
        if (d < bestDist) {
            bestDist = d;
            best = &gem;
        }
    }
    return best;
}

}  // namespace

PlayerInput decideBotInput(const World& world, const Player& self) {
    PlayerInput input;
    if (!self.alive()) {
        return input;
    }

    const BrawlerDef& def = findBrawler(self.brawlerId);
    float enemyDist = 0.0f;
    const Player* enemy = nearestEnemy(world, self, &enemyDist);
    const Gem* gem = nearestGem(world, self);

    if (enemy != nullptr) {
        input.aim = (enemy->position - self.position).normalized();
        input.shoot = enemyDist <= def.attackRange * 0.9f;
        input.useSuper = self.superCharge >= 100 && enemyDist <= def.attackRange;
    }

    const float healthRatio = static_cast<float>(self.health) / static_cast<float>(self.maxHealth);
    if (enemy != nullptr && healthRatio < 0.35f) {
        // Too hurt to trade: fall back towards our own spawn.
        input.move = (self.position - enemy->position).normalized();
        return input;
    }

    if (gem != nullptr) {
        input.move = (gem->position - self.position).normalized();
    } else if (enemy != nullptr) {
        const Vec2 toEnemy = enemy->position - self.position;
        const float ideal = def.attackRange * 0.7f;
        input.move = enemyDist > ideal ? toEnemy.normalized() : (toEnemy.normalized() * -1.0f);
    } else {
        input.move = (world.arena().center() - self.position).normalized();
    }
    return input;
}

void driveBots(World& world) {
    for (const Player& p : world.players()) {
        if (p.bot) {
            world.setInput(p.id, decideBotInput(world, p));
        }
    }
}

}  // namespace mega
