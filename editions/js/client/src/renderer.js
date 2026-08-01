const TILE = { FLOOR: 0, WALL: 1, BUSH: 2, FENCE: 3 };
const TEAM_COLORS = ['#3e84e8', '#e2574c'];

/// Chunky top-down look drawn entirely with canvas primitives - no imported
/// artwork, so everything shipped with the game is original.
export class Renderer {
  constructor(canvas) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.camera = { x: 15, y: 17 };
    this.tilePixels = 46;
    this.resize();
    window.addEventListener('resize', () => this.resize());
  }

  resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    this.canvas.width = Math.floor(window.innerWidth * dpr);
    this.canvas.height = Math.floor(window.innerHeight * dpr);
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.viewWidth = window.innerWidth;
    this.viewHeight = window.innerHeight;
    // Phones get a slightly wider view so the joystick does not cover the fight.
    this.tilePixels = Math.max(28, Math.min(52, this.viewHeight / 16));
  }

  toScreen(x, y) {
    return {
      x: (x - this.camera.x) * this.tilePixels + this.viewWidth / 2,
      y: (y - this.camera.y) * this.tilePixels + this.viewHeight / 2,
    };
  }

  draw(state, arena, roster, localId) {
    const ctx = this.ctx;
    const self = state.players.find((p) => p.id === localId);
    if (self) {
      this.camera.x += (self.x - this.camera.x) * 0.18;
      this.camera.y += (self.y - this.camera.y) * 0.18;
    }

    ctx.fillStyle = '#0d1020';
    ctx.fillRect(0, 0, this.viewWidth, this.viewHeight);

    this.drawArena(arena);
    for (const gem of state.gems) this.drawGem(gem);
    for (const proj of state.proj) this.drawProjectile(proj);
    for (const player of state.players) this.drawPlayer(player, roster, localId);
    if (self) this.drawHud(self);
  }

  drawArena(arena) {
    const ctx = this.ctx;
    const size = this.tilePixels;
    for (let y = 0; y < arena.height; y += 1) {
      for (let x = 0; x < arena.width; x += 1) {
        const p = this.toScreen(x, y);
        if (p.x < -size || p.y < -size || p.x > this.viewWidth || p.y > this.viewHeight) continue;
        const tile = arena.tiles[y * arena.width + x];

        ctx.fillStyle = (x + y) % 2 === 0 ? '#4a7c59' : '#568a64';
        ctx.fillRect(p.x, p.y, size + 1, size + 1);

        if (tile === TILE.WALL) {
          ctx.fillStyle = '#5c4230';
          ctx.fillRect(p.x, p.y, size + 1, size + 1);
          ctx.fillStyle = '#7c5c42';
          ctx.fillRect(p.x, p.y, size + 1, size * 0.35);
        } else if (tile === TILE.BUSH) {
          ctx.fillStyle = '#28603e';
          ctx.beginPath();
          ctx.arc(p.x + size / 2, p.y + size / 2, size * 0.48, 0, Math.PI * 2);
          ctx.fill();
        } else if (tile === TILE.FENCE) {
          ctx.fillStyle = '#b0965c';
          ctx.fillRect(p.x, p.y + size / 3, size + 1, size / 3);
        }
      }
    }
  }

  drawGem(gem) {
    const ctx = this.ctx;
    const p = this.toScreen(gem.x, gem.y);
    const r = this.tilePixels * 0.24;
    ctx.fillStyle = '#8c52ff';
    ctx.beginPath();
    ctx.moveTo(p.x, p.y - r);
    ctx.lineTo(p.x + r, p.y);
    ctx.lineTo(p.x, p.y + r);
    ctx.lineTo(p.x - r, p.y);
    ctx.closePath();
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.6)';
    ctx.stroke();
  }

  drawProjectile(proj) {
    const ctx = this.ctx;
    const p = this.toScreen(proj.x, proj.y);
    ctx.fillStyle = TEAM_COLORS[proj.tm];
    ctx.beginPath();
    ctx.arc(p.x, p.y, this.tilePixels * 0.15, 0, Math.PI * 2);
    ctx.fill();
  }

  drawPlayer(player, roster, localId) {
    if (player.hp <= 0 || player.rt > 0) return;
    const ctx = this.ctx;
    const p = this.toScreen(player.x, player.y);
    const size = this.tilePixels;
    const def = roster.find((b) => b.id === player.b);
    const bodyColor = def ? def.color : '#ffffff';

    ctx.fillStyle = 'rgba(0,0,0,0.28)';
    ctx.beginPath();
    ctx.ellipse(p.x, p.y + size * 0.34, size * 0.42, size * 0.16, 0, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = TEAM_COLORS[player.tm];
    ctx.beginPath();
    ctx.arc(p.x, p.y, size * 0.44, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = bodyColor;
    ctx.beginPath();
    ctx.arc(p.x, p.y, size * 0.35, 0, Math.PI * 2);
    ctx.fill();

    // Eyes look towards the aim direction.
    ctx.fillStyle = '#14161f';
    for (const side of [-1, 1]) {
      ctx.beginPath();
      ctx.arc(
        p.x + player.ax * size * 0.1 - player.ay * side * size * 0.14,
        p.y + player.ay * size * 0.1 + player.ax * side * size * 0.14,
        size * 0.07,
        0,
        Math.PI * 2,
      );
      ctx.fill();
    }

    ctx.strokeStyle = 'rgba(255,255,255,0.75)';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.moveTo(p.x, p.y);
    ctx.lineTo(p.x + player.ax * size * 0.62, p.y + player.ay * size * 0.62);
    ctx.stroke();

    this.drawHealthBar(p.x, p.y - size * 0.72, size, player, player.id === localId);

    if (player.gm > 0) {
      ctx.fillStyle = '#8c52ff';
      ctx.font = `bold ${Math.round(size * 0.3)}px sans-serif`;
      ctx.textAlign = 'center';
      ctx.fillText(`◆${player.gm}`, p.x, p.y - size * 0.92);
    }
  }

  drawHealthBar(cx, cy, size, player, isLocal) {
    const ctx = this.ctx;
    const w = size * 0.9;
    const h = Math.max(5, size * 0.14);
    ctx.fillStyle = 'rgba(10,10,16,0.85)';
    ctx.fillRect(cx - w / 2, cy, w, h);
    ctx.fillStyle = isLocal ? '#5adc78' : TEAM_COLORS[player.tm];
    ctx.fillRect(cx - w / 2, cy, (w * Math.max(0, player.hp)) / player.mhp, h);
  }

  drawHud(self) {
    const ctx = this.ctx;
    const baseY = this.viewHeight - 46;
    for (let i = 0; i < self.ac; i += 1) {
      ctx.fillStyle = i < Math.floor(self.am) ? '#f0dc78' : 'rgba(60,60,70,0.7)';
      ctx.fillRect(24 + i * 26, baseY, 20, 34);
    }
    ctx.fillStyle = 'rgba(40,40,50,0.85)';
    ctx.fillRect(24, baseY - 24, 20 * self.ac + 6 * self.ac, 14);
    ctx.fillStyle = self.sc >= 100 ? '#ffd640' : '#78b4f0';
    ctx.fillRect(24, baseY - 24, ((20 * self.ac + 6 * self.ac) * self.sc) / 100, 14);
  }
}
