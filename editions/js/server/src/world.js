import { Arena } from './arena.js';
import { findBrawler } from './brawlers.js';

export const PHASE = { WARMUP: 0, PLAYING: 1, COUNTDOWN: 2, FINISHED: 3 };

const PLAYER_RADIUS = 0.42;
const PROJECTILE_RADIUS = 0.25;
const GEM_PICKUP_RADIUS = 0.85;
const MAX_EVENTS = 16;

const norm = (v) => {
  const len = Math.hypot(v.x, v.y);
  return len < 1e-6 ? { x: 0, y: 0 } : { x: v.x / len, y: v.y / len };
};
const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const rotate = (v, rad) => ({
  x: v.x * Math.cos(rad) - v.y * Math.sin(rad),
  y: v.x * Math.sin(rad) + v.y * Math.cos(rad),
});

export const DEFAULT_RULES = {
  gemsToWin: 10,
  countdownSeconds: 15,
  matchSeconds: 150,
  gemSpawnSeconds: 2.5,
  respawnSeconds: 3,
};

/// Authoritative simulation for the JS Edition. It follows the same rules as
/// the native C++ core (core/src/World.cpp); the two implementations are kept
/// in sync by hand and share the wire protocol documented in docs/protocol.md.
export class World {
  constructor({ width = 30, height = 34, seed = 1337, rules = DEFAULT_RULES } = {}) {
    this.arena = new Arena(width, height, seed);
    this.rules = rules;
    this.seed = seed;
    this.players = [];
    this.projectiles = [];
    this.gems = [];
    this.events = [];
    this.nextId = 1;
    this.gemSpawnTimer = 0;
    this.match = {
      phase: PHASE.WARMUP,
      phaseTimer: 3,
      elapsed: 0,
      teamGems: [0, 0],
      winner: 0,
      hasWinner: false,
    };
  }

  addPlayer(name, brawlerId, team, isBot = false) {
    const def = findBrawler(brawlerId);
    const slot = this.players.filter((p) => p.team === team).length % 4;
    const spawn = this.arena.spawnPoint(team, slot);
    const player = {
      id: this.nextId,
      name,
      brawlerId: def.id,
      team,
      x: spawn.x,
      y: spawn.y,
      aimX: team === 0 ? 0 : 0,
      aimY: team === 0 ? -1 : 1,
      health: def.maxHealth,
      maxHealth: def.maxHealth,
      ammo: def.ammoCapacity,
      ammoCapacity: def.ammoCapacity,
      superCharge: 0,
      gemsHeld: 0,
      respawnTimer: 0,
      attackCooldown: 0,
      kills: 0,
      deaths: 0,
      bot: isBot,
      input: { move: { x: 0, y: 0 }, aim: { x: 0, y: 0 }, shoot: false, useSuper: false },
    };
    this.nextId += 1;
    this.players.push(player);
    this.pushEvent(`${name} entrou na partida`);
    return player;
  }

  removePlayer(id) {
    this.players = this.players.filter((p) => p.id !== id);
  }

  setInput(id, input) {
    const player = this.players.find((p) => p.id === id);
    if (player) player.input = input;
  }

  isAlive(player) {
    return player.health > 0 && player.respawnTimer <= 0;
  }

  pushEvent(text) {
    this.events.push({ time: this.match.elapsed, text });
    if (this.events.length > MAX_EVENTS) this.events.shift();
  }

  step(dt) {
    const clamped = Math.min(Math.max(dt, 0), 0.1);
    this.match.elapsed += clamped;
    for (const player of this.players) this.stepPlayer(player, clamped);
    this.stepProjectiles(clamped);
    this.stepGems(clamped);
    this.stepMatch(clamped);
    this.projectiles = this.projectiles.filter((p) => p.alive);
    this.gems = this.gems.filter((g) => g.alive);
  }

  stepPlayer(player, dt) {
    const def = findBrawler(player.brawlerId);
    if (player.respawnTimer > 0) {
      player.respawnTimer -= dt;
      if (player.respawnTimer <= 0) this.respawn(player);
      return;
    }
    if (player.health <= 0) return;

    player.ammo = Math.min(player.ammoCapacity, player.ammo + dt / def.reloadSeconds);
    player.attackCooldown = Math.max(0, player.attackCooldown - dt);

    const input = player.input;
    if (input.move.x !== 0 || input.move.y !== 0) {
      const dir = norm(input.move);
      const gemPenalty = 1 - Math.min(player.gemsHeld * 0.015, 0.15);
      const speed = def.moveSpeed * gemPenalty * dt;
      const moved = this.arena.resolveMove(
        { x: player.x, y: player.y },
        { x: dir.x * speed, y: dir.y * speed },
        PLAYER_RADIUS,
      );
      player.x = moved.x;
      player.y = moved.y;
    }
    if (input.aim.x !== 0 || input.aim.y !== 0) {
      const aim = norm(input.aim);
      player.aimX = aim.x;
      player.aimY = aim.y;
    }

    if (this.match.phase === PHASE.WARMUP) return;
    if (input.useSuper && player.superCharge >= 100) {
      this.fireSuper(player);
    } else if (input.shoot && player.ammo >= 1 && player.attackCooldown <= 0) {
      this.fire(player);
    }
  }

  fire(player) {
    const def = findBrawler(player.brawlerId);
    player.ammo -= 1;
    player.attackCooldown = 0.28;

    const count = Math.max(1, def.projectilesPerShot);
    const spread = (def.spreadDegrees * Math.PI) / 180;
    for (let i = 0; i < count; i += 1) {
      const t = count === 1 ? 0 : i / (count - 1) - 0.5;
      const dir = rotate({ x: player.aimX, y: player.aimY }, t * spread);
      this.spawnProjectile(player, dir, {
        speed: def.projectileSpeed,
        range: def.attackRange,
        damage: def.damagePerProjectile,
        overWalls: def.attack === 'lobbed',
        friendly: def.attack === 'healing',
      });
    }
  }

  fireSuper(player) {
    const def = findBrawler(player.brawlerId);
    player.superCharge = 0;
    player.attackCooldown = 0.45;

    const count = Math.max(3, def.projectilesPerShot * 2);
    const spread = (Math.max(30, def.spreadDegrees * 1.5) * Math.PI) / 180;
    for (let i = 0; i < count; i += 1) {
      const t = i / (count - 1) - 0.5;
      const dir = rotate({ x: player.aimX, y: player.aimY }, t * spread);
      this.spawnProjectile(player, dir, {
        speed: def.projectileSpeed * 1.15,
        range: def.attackRange * 1.3,
        damage: Math.round(def.damagePerProjectile * 1.5),
        overWalls: true,
        friendly: def.attack === 'healing',
      });
    }
    this.pushEvent(`${player.name} usou o SUPER`);
  }

  spawnProjectile(owner, dir, { speed, range, damage, overWalls, friendly }) {
    this.projectiles.push({
      id: this.nextId++,
      ownerId: owner.id,
      team: owner.team,
      x: owner.x,
      y: owner.y,
      vx: dir.x * speed,
      vy: dir.y * speed,
      remaining: range,
      damage,
      overWalls,
      friendly,
      alive: true,
    });
  }

  stepProjectiles(dt) {
    // Sliced movement so fast shots cannot tunnel through players or walls.
    for (const proj of this.projectiles) {
      const travel = Math.hypot(proj.vx, proj.vy) * dt;
      const slices = Math.max(1, Math.ceil(travel / 0.3));
      for (let i = 0; i < slices && proj.alive; i += 1) {
        this.advanceProjectile(proj, dt / slices);
      }
    }
  }

  advanceProjectile(proj, dt) {
    proj.x += proj.vx * dt;
    proj.y += proj.vy * dt;
    proj.remaining -= Math.hypot(proj.vx, proj.vy) * dt;

    if (proj.remaining <= 0) {
      proj.alive = false;
      return;
    }
    if (!proj.overWalls && this.arena.blocksFlatProjectile(proj)) {
      proj.alive = false;
      return;
    }

    for (const target of this.players) {
      if (!this.isAlive(target) || target.id === proj.ownerId) continue;
      const sameTeam = target.team === proj.team;
      if (proj.friendly !== sameTeam) continue;
      if (dist(target, proj) > PLAYER_RADIUS + PROJECTILE_RADIUS) continue;

      const owner = this.players.find((p) => p.id === proj.ownerId);
      if (proj.friendly) {
        target.health = Math.min(target.maxHealth, target.health + proj.damage);
      } else {
        target.health -= proj.damage;
      }
      if (owner) {
        const ownerDef = findBrawler(owner.brawlerId);
        owner.superCharge = Math.min(100, owner.superCharge + ownerDef.superChargePerHit);
      }
      if (!proj.friendly && target.health <= 0) this.killPlayer(target, owner);
      proj.alive = false;
      return;
    }
  }

  killPlayer(victim, killer) {
    victim.health = 0;
    victim.deaths += 1;
    victim.respawnTimer = this.rules.respawnSeconds;
    for (let i = 0; i < victim.gemsHeld; i += 1) {
      const drop = this.arena.resolveMove(
        { x: victim.x, y: victim.y },
        { x: (Math.random() - 0.5) * 2.4, y: (Math.random() - 0.5) * 2.4 },
        0.2,
      );
      this.gems.push({ id: this.nextId++, x: drop.x, y: drop.y, cooldown: 0.8, alive: true });
    }
    this.match.teamGems[victim.team] -= victim.gemsHeld;
    victim.gemsHeld = 0;

    if (killer) {
      killer.kills += 1;
      killer.superCharge = Math.min(100, killer.superCharge + 20);
      this.pushEvent(`${killer.name} eliminou ${victim.name}`);
    } else {
      this.pushEvent(`${victim.name} foi eliminado`);
    }
  }

  respawn(player) {
    const def = findBrawler(player.brawlerId);
    const spawn = this.arena.spawnPoint(player.team, player.id % 4);
    player.health = def.maxHealth;
    player.ammo = def.ammoCapacity;
    player.respawnTimer = 0;
    player.x = spawn.x;
    player.y = spawn.y;
  }

  stepGems(dt) {
    if (this.match.phase === PHASE.PLAYING || this.match.phase === PHASE.COUNTDOWN) {
      this.gemSpawnTimer -= dt;
      if (this.gemSpawnTimer <= 0) {
        this.gemSpawnTimer = this.rules.gemSpawnSeconds;
        const spot = this.arena.resolveMove(
          this.arena.center(),
          { x: (Math.random() - 0.5) * 3, y: (Math.random() - 0.5) * 3 },
          0.2,
        );
        this.gems.push({ id: this.nextId++, x: spot.x, y: spot.y, cooldown: 0, alive: true });
      }
    }

    for (const gem of this.gems) {
      gem.cooldown = Math.max(0, gem.cooldown - dt);
      if (gem.cooldown > 0) continue;
      for (const player of this.players) {
        if (!this.isAlive(player)) continue;
        if (dist(player, gem) <= GEM_PICKUP_RADIUS) {
          gem.alive = false;
          player.gemsHeld += 1;
          this.match.teamGems[player.team] += 1;
          break;
        }
      }
    }
  }

  stepMatch(dt) {
    const match = this.match;
    switch (match.phase) {
      case PHASE.WARMUP:
        match.phaseTimer -= dt;
        if (match.phaseTimer <= 0) {
          match.phase = PHASE.PLAYING;
          match.phaseTimer = this.rules.matchSeconds;
          this.pushEvent('A partida começou!');
        }
        break;

      case PHASE.PLAYING: {
        match.phaseTimer -= dt;
        const leader = match.teamGems.findIndex((gems) => gems >= this.rules.gemsToWin);
        if (leader >= 0) {
          match.phase = PHASE.COUNTDOWN;
          match.phaseTimer = this.rules.countdownSeconds;
          match.winner = leader;
          this.pushEvent('Contagem regressiva iniciada!');
        } else if (match.phaseTimer <= 0) {
          match.phase = PHASE.FINISHED;
          match.hasWinner = match.teamGems[0] !== match.teamGems[1];
          match.winner = match.teamGems[0] > match.teamGems[1] ? 0 : 1;
          this.pushEvent('Tempo esgotado');
        }
        break;
      }

      case PHASE.COUNTDOWN:
        if (match.teamGems[match.winner] < this.rules.gemsToWin) {
          match.phase = PHASE.PLAYING;
          match.phaseTimer = Math.max(10, this.rules.matchSeconds - match.elapsed);
          this.pushEvent('Contagem interrompida!');
          break;
        }
        match.phaseTimer -= dt;
        if (match.phaseTimer <= 0) {
          match.phase = PHASE.FINISHED;
          match.hasWinner = true;
          this.pushEvent(match.winner === 0 ? 'Time Azul venceu!' : 'Time Vermelho venceu!');
        }
        break;

      case PHASE.FINISHED:
        // Keeps counting into negative values; the server uses it to know how
        // long the result screen has been up before starting a new match.
        match.phaseTimer -= dt;
        break;

      default:
        break;
    }
  }

  /// Compact snapshot shared by the WebSocket and TCP transports.
  snapshot() {
    return {
      t: 'state',
      match: {
        ph: this.match.phase,
        pt: Math.round(this.match.phaseTimer * 100) / 100,
        el: Math.round(this.match.elapsed * 100) / 100,
        g0: this.match.teamGems[0],
        g1: this.match.teamGems[1],
      },
      players: this.players.map((p) => ({
        id: p.id,
        n: p.name,
        b: p.brawlerId,
        tm: p.team,
        x: Math.round(p.x * 100) / 100,
        y: Math.round(p.y * 100) / 100,
        ax: Math.round(p.aimX * 100) / 100,
        ay: Math.round(p.aimY * 100) / 100,
        hp: p.health,
        mhp: p.maxHealth,
        am: Math.round(p.ammo * 100) / 100,
        ac: p.ammoCapacity,
        sc: p.superCharge,
        gm: p.gemsHeld,
        rt: Math.round(p.respawnTimer * 100) / 100,
        k: p.kills,
        d: p.deaths,
        bot: p.bot,
      })),
      proj: this.projectiles.map((p) => ({
        id: p.id,
        tm: p.team,
        x: Math.round(p.x * 100) / 100,
        y: Math.round(p.y * 100) / 100,
      })),
      gems: this.gems.map((g) => ({
        id: g.id,
        x: Math.round(g.x * 100) / 100,
        y: Math.round(g.y * 100) / 100,
      })),
      feed: this.events.slice(-3).map((e) => e.text),
    };
  }
}
