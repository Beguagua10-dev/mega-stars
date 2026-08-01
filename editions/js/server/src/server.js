import http from 'node:http';
import net from 'node:net';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { WebSocketServer } from 'ws';

import { World, PHASE } from './world.js';
import { driveBots } from './bots.js';
import { ROSTER } from './brawlers.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const CLIENT_DIR = path.resolve(here, '../../client');

const TICK_HZ = 20;
const SNAPSHOT_HZ = 20;
const BOT_NAMES = ['Zeca', 'Nina', 'Tobias', 'Lia', 'Rui', 'Dara'];
const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
};

const config = {
  httpPort: Number(process.env.PORT || 8780),
  tcpPort: Number(process.env.TCP_PORT || 8781),
  seed: Number(process.env.SEED || 1337),
  playersPerTeam: Number(process.env.TEAM_SIZE || 3),
};

const world = new World({ seed: config.seed });
/// Every connected client, keyed by its player id.
const clients = new Map();

function balancedTeam() {
  const blue = world.players.filter((p) => p.team === 0 && !p.bot).length;
  const red = world.players.filter((p) => p.team === 1 && !p.bot).length;
  return blue <= red ? 0 : 1;
}

/// Keeps both teams full by adding or removing bots as humans come and go.
function refillBots() {
  for (let team = 0; team < 2; team += 1) {
    const members = world.players.filter((p) => p.team === team);
    const humans = members.filter((p) => !p.bot).length;
    const bots = members.filter((p) => p.bot);
    const wanted = Math.max(0, config.playersPerTeam - humans);

    for (let i = bots.length; i < wanted; i += 1) {
      const name = BOT_NAMES[(team * 3 + i) % BOT_NAMES.length];
      const brawler = ROSTER[(team * 2 + i + 1) % ROSTER.length].id;
      world.addPlayer(name, brawler, team, true);
    }
    for (let i = wanted; i < bots.length; i += 1) {
      world.removePlayer(bots[i].id);
    }
  }
}

function welcomeMessage(playerId) {
  return {
    t: 'welcome',
    id: playerId,
    seed: world.seed,
    w: world.arena.width,
    h: world.arena.height,
    tiles: Array.from(world.arena.tiles),
    roster: ROSTER,
  };
}

function handleMessage(client, raw) {
  let message;
  try {
    message = JSON.parse(raw);
  } catch {
    return;
  }

  if (message.t === 'join') {
    if (client.playerId !== null) return;
    const team = balancedTeam();
    const name = String(message.name || 'Jogador').slice(0, 16);
    const player = world.addPlayer(name, String(message.brawler || 'faisca'), team, false);
    client.playerId = player.id;
    clients.set(player.id, client);
    refillBots();
    client.send(JSON.stringify(welcomeMessage(player.id)));
    return;
  }

  if (message.t === 'input' && client.playerId !== null) {
    world.setInput(client.playerId, {
      move: { x: Number(message.mx) || 0, y: Number(message.my) || 0 },
      aim: { x: Number(message.ax) || 0, y: Number(message.ay) || 0 },
      shoot: Boolean(message.s),
      useSuper: Boolean(message.u),
    });
  }
}

function dropClient(client) {
  if (client.playerId !== null) {
    world.removePlayer(client.playerId);
    clients.delete(client.playerId);
    refillBots();
  }
}

// ------------------------------------------------------------- http ---------
const httpServer = http.createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  if (url.pathname === '/health') {
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ ok: true, players: world.players.length }));
    return;
  }

  if (url.pathname === '/api/roster') {
    res.writeHead(200, { 'content-type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify(ROSTER));
    return;
  }

  const requested = url.pathname === '/' ? '/index.html' : url.pathname;
  const filePath = path.join(CLIENT_DIR, path.normalize(requested).replace(/^(\.\.[/\\])+/, ''));
  if (!filePath.startsWith(CLIENT_DIR)) {
    res.writeHead(403).end('forbidden');
    return;
  }
  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404).end('nao encontrado');
      return;
    }
    res.writeHead(200, { 'content-type': MIME[path.extname(filePath)] || 'application/octet-stream' });
    res.end(data);
  });
});

// -------------------------------------------------------- websockets --------
const wss = new WebSocketServer({ server: httpServer });
wss.on('connection', (socket) => {
  const client = { playerId: null, send: (text) => socket.send(text) };
  socket.on('message', (data) => handleMessage(client, data.toString()));
  socket.on('close', () => dropClient(client));
  socket.on('error', () => dropClient(client));
});

// ---------------------------------------------------------------- tcp -------
// Native editions speak the same JSON messages over a plain socket, so browser
// and desktop/mobile players share one match.
const tcpServer = net.createServer((socket) => {
  socket.setNoDelay(true);
  const client = {
    playerId: null,
    send: (text) => {
      if (!socket.destroyed) socket.write(`${text}\n`);
    },
  };
  let buffer = '';
  socket.on('data', (chunk) => {
    buffer += chunk.toString();
    let index = buffer.indexOf('\n');
    while (index !== -1) {
      handleMessage(client, buffer.slice(0, index));
      buffer = buffer.slice(index + 1);
      index = buffer.indexOf('\n');
    }
  });
  socket.on('close', () => dropClient(client));
  socket.on('error', () => dropClient(client));
});

// --------------------------------------------------------------- loop -------
let last = Date.now();
setInterval(() => {
  const now = Date.now();
  const dt = (now - last) / 1000;
  last = now;
  driveBots(world);
  world.step(dt);
}, 1000 / TICK_HZ);

setInterval(() => {
  if (clients.size === 0) return;
  const payload = JSON.stringify(world.snapshot());
  for (const client of clients.values()) client.send(payload);
}, 1000 / SNAPSHOT_HZ);

// Restart the match a few seconds after it ends so a server can stay online.
setInterval(() => {
  if (world.match.phase !== PHASE.FINISHED || world.match.phaseTimer > -5) return;
  world.match.phase = PHASE.WARMUP;
  world.match.phaseTimer = 3;
  world.match.elapsed = 0;
  world.match.teamGems = [0, 0];
  world.match.hasWinner = false;
  world.gems = [];
  world.projectiles = [];
  for (const player of world.players) {
    player.gemsHeld = 0;
    player.kills = 0;
    player.deaths = 0;
    world.respawn(player);
  }
  world.pushEvent('Nova partida!');
}, 1000);

refillBots();
httpServer.listen(config.httpPort, () => {
  console.log(`Mega Stars JS Edition em http://localhost:${config.httpPort}`);
});
tcpServer.listen(config.tcpPort, () => {
  console.log(`Servidor nativo (PC/Mobile/Pocket) na porta TCP ${config.tcpPort}`);
});
