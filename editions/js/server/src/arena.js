export const TILE = { FLOOR: 0, WALL: 1, BUSH: 2, FENCE: 3 };

function mulberry32(seed) {
  let a = seed >>> 0;
  return function random() {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/// Point-symmetric tile arena. The server owns it and ships it to clients in
/// the welcome message, so browsers never have to reproduce the generator.
export class Arena {
  constructor(width = 30, height = 34, seed = 1337) {
    this.width = width;
    this.height = height;
    this.tiles = new Uint8Array(width * height);

    for (let x = 0; x < width; x += 1) {
      this.set(x, 0, TILE.WALL);
      this.set(x, height - 1, TILE.WALL);
    }
    for (let y = 0; y < height; y += 1) {
      this.set(0, y, TILE.WALL);
      this.set(width - 1, y, TILE.WALL);
    }

    const random = mulberry32(seed);
    for (let y = 2; y < Math.floor(height / 2); y += 1) {
      for (let x = 2; x < width - 2; x += 1) {
        const roll = random();
        let tile = TILE.FLOOR;
        if (roll < 0.1) tile = TILE.WALL;
        else if (roll < 0.2) tile = TILE.BUSH;
        else if (roll < 0.23) tile = TILE.FENCE;
        if (tile === TILE.FLOOR) continue;
        this.set(x, y, tile);
        this.set(width - 1 - x, height - 1 - y, tile);
      }
    }

    const cx = Math.floor(width / 2);
    const cy = Math.floor(height / 2);
    for (let y = cy - 2; y <= cy + 2; y += 1) {
      for (let x = cx - 2; x <= cx + 2; x += 1) {
        this.clearMirrored(x, y);
      }
    }
    for (let slot = 0; slot < 4; slot += 1) {
      for (let team = 0; team < 2; team += 1) {
        const spawn = this.spawnPoint(team, slot);
        for (let dy = -1; dy <= 1; dy += 1) {
          for (let dx = -1; dx <= 1; dx += 1) {
            this.clearMirrored(Math.floor(spawn.x) + dx, Math.floor(spawn.y) + dy);
          }
        }
      }
    }
  }

  set(x, y, tile) {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return;
    const border = x === 0 || y === 0 || x === this.width - 1 || y === this.height - 1;
    if (border && tile !== TILE.WALL) return;
    this.tiles[y * this.width + x] = tile;
  }

  clearMirrored(x, y) {
    this.set(x, y, TILE.FLOOR);
    this.set(this.width - 1 - x, this.height - 1 - y, TILE.FLOOR);
  }

  at(x, y) {
    if (x < 0 || y < 0 || x >= this.width || y >= this.height) return TILE.WALL;
    return this.tiles[y * this.width + x];
  }

  atWorld(p) {
    return this.at(Math.floor(p.x), Math.floor(p.y));
  }

  blocksFlatProjectile(p) {
    const tile = this.atWorld(p);
    return tile === TILE.WALL || tile === TILE.FENCE;
  }

  circleBlocked(p, radius) {
    for (const dx of [-radius, 0, radius]) {
      for (const dy of [-radius, 0, radius]) {
        if (this.at(Math.floor(p.x + dx), Math.floor(p.y + dy)) === TILE.WALL) return true;
      }
    }
    return false;
  }

  /// Axis-separated sliding so players glide along walls instead of sticking.
  resolveMove(from, delta, radius) {
    let x = from.x;
    let y = from.y;
    if (!this.circleBlocked({ x: x + delta.x, y }, radius)) x += delta.x;
    if (!this.circleBlocked({ x, y: y + delta.y }, radius)) y += delta.y;
    const lo = 1 + radius + 0.001;
    return {
      x: Math.min(Math.max(x, lo), this.width - 1 - radius - 0.001),
      y: Math.min(Math.max(y, lo), this.height - 1 - radius - 0.001),
    };
  }

  center() {
    return { x: this.width / 2, y: this.height / 2 };
  }

  spawnPoint(teamIndex, slot) {
    const spread = (slot - 1) * 2;
    const x = Math.min(Math.max(this.width / 2 + spread, 2), this.width - 3);
    const y = teamIndex === 0 ? this.height - 3 : 3;
    return { x: x + 0.5, y: y + 0.5 };
  }
}
