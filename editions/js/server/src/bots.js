import { findBrawler } from './brawlers.js';

const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const norm = (x, y) => {
  const len = Math.hypot(x, y);
  return len < 1e-6 ? { x: 0, y: 0 } : { x: x / len, y: y / len };
};

/// Mirrors core/src/BotBrain.cpp: chase gems, shoot the closest enemy in range
/// and back off when badly hurt.
export function decideBotInput(world, self) {
  const idle = { move: { x: 0, y: 0 }, aim: { x: 0, y: 0 }, shoot: false, useSuper: false };
  if (!world.isAlive(self)) return idle;

  const def = findBrawler(self.brawlerId);
  let enemy = null;
  let enemyDist = Infinity;
  for (const other of world.players) {
    if (other.team === self.team || !world.isAlive(other)) continue;
    const d = dist(other, self);
    if (d < enemyDist) {
      enemyDist = d;
      enemy = other;
    }
  }

  let gem = null;
  let gemDist = Infinity;
  for (const candidate of world.gems) {
    const d = dist(candidate, self);
    if (d < gemDist) {
      gemDist = d;
      gem = candidate;
    }
  }

  const input = { move: { x: 0, y: 0 }, aim: { x: 0, y: 0 }, shoot: false, useSuper: false };
  if (enemy) {
    input.aim = norm(enemy.x - self.x, enemy.y - self.y);
    input.shoot = enemyDist <= def.attackRange * 0.9;
    input.useSuper = self.superCharge >= 100 && enemyDist <= def.attackRange;
  }

  if (enemy && self.health / self.maxHealth < 0.35) {
    input.move = norm(self.x - enemy.x, self.y - enemy.y);
    return input;
  }
  if (gem) {
    input.move = norm(gem.x - self.x, gem.y - self.y);
  } else if (enemy) {
    const ideal = def.attackRange * 0.7;
    const dir = norm(enemy.x - self.x, enemy.y - self.y);
    const sign = enemyDist > ideal ? 1 : -1;
    input.move = { x: dir.x * sign, y: dir.y * sign };
  } else {
    const center = world.arena.center();
    input.move = norm(center.x - self.x, center.y - self.y);
  }
  return input;
}

/// Bots have no pathfinding, so they wedge themselves into walls. When one
/// stops making progress it strafes sideways for a moment until it is free.
function unstick(world, player, input) {
  const memory = player.botMemory || (player.botMemory = { x: player.x, y: player.y, strafeUntil: 0, side: 1 });
  const progress = Math.hypot(player.x - memory.x, player.y - memory.y);
  memory.x = player.x;
  memory.y = player.y;

  const now = world.match.elapsed;
  const wantsToMove = input.move.x !== 0 || input.move.y !== 0;
  if (wantsToMove && progress < 0.01 && now > memory.strafeUntil) {
    memory.strafeUntil = now + 0.6;
    memory.side = -memory.side;
  }
  if (now < memory.strafeUntil) {
    input.move = { x: -input.move.y * memory.side, y: input.move.x * memory.side };
  }
  return input;
}

export function driveBots(world) {
  for (const player of world.players) {
    if (!player.bot) continue;
    world.setInput(player.id, unstick(world, player, decideBotInput(world, player)));
  }
}
