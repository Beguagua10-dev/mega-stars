import test from 'node:test';
import assert from 'node:assert/strict';

import { World, PHASE } from '../src/world.js';
import { driveBots } from '../src/bots.js';
import { Arena, TILE } from '../src/arena.js';

test('arena is point symmetric', () => {
  const arena = new Arena(30, 34, 7);
  for (let y = 0; y < arena.height; y += 1) {
    for (let x = 0; x < arena.width; x += 1) {
      assert.equal(arena.at(x, y), arena.at(arena.width - 1 - x, arena.height - 1 - y));
    }
  }
});

test('the outer ring is always solid', () => {
  const arena = new Arena(30, 34, 99);
  assert.equal(arena.at(0, 5), TILE.WALL);
  assert.equal(arena.at(29, 5), TILE.WALL);
});

test('players cannot walk through walls', () => {
  const arena = new Arena(30, 34, 3);
  const moved = arena.resolveMove({ x: 1.5, y: 1.5 }, { x: -5, y: -5 }, 0.42);
  assert.ok(moved.x >= 1 && moved.y >= 1);
});

test('a hit reduces health and charges the super', () => {
  const world = new World({ seed: 11 });
  const attacker = world.addPlayer('A', 'mira', 0);
  const victim = world.addPlayer('B', 'faisca', 1);
  for (let i = 0; i < 200; i += 1) world.step(1 / 30);
  assert.equal(world.match.phase, PHASE.PLAYING);

  const center = world.arena.center();
  attacker.x = center.x - 1;
  attacker.y = center.y + 0.5;
  victim.x = center.x + 1;
  victim.y = center.y + 0.5;
  const before = victim.health;

  world.setInput(attacker.id, { move: { x: 0, y: 0 }, aim: { x: 1, y: 0 }, shoot: true, useSuper: false });
  world.step(1 / 30);
  world.setInput(attacker.id, { move: { x: 0, y: 0 }, aim: { x: 1, y: 0 }, shoot: false, useSuper: false });
  for (let i = 0; i < 15; i += 1) world.step(1 / 30);

  assert.ok(victim.health < before);
  assert.ok(attacker.superCharge > 0);
});

test('picking up a gem scores for the team', () => {
  const world = new World();
  const player = world.addPlayer('A', 'faisca', 0);
  for (let i = 0; i < 300 && world.match.teamGems[0] === 0; i += 1) {
    const center = world.arena.center();
    player.x = center.x;
    player.y = center.y;
    world.step(1 / 30);
  }
  assert.ok(world.match.teamGems[0] > 0);
});

test('bots move on their own', () => {
  const world = new World();
  const bot = world.addPlayer('Bot', 'bruto', 1, true);
  const start = { x: bot.x, y: bot.y };
  for (let i = 0; i < 120; i += 1) {
    driveBots(world);
    world.step(1 / 30);
  }
  assert.ok(Math.hypot(bot.x - start.x, bot.y - start.y) > 0.5);
});

test('snapshot has the fields the clients read', () => {
  const world = new World();
  world.addPlayer('A', 'faisca', 0);
  const snapshot = world.snapshot();
  assert.equal(snapshot.t, 'state');
  assert.ok('g0' in snapshot.match && 'ph' in snapshot.match);
  assert.deepEqual(Object.keys(snapshot.players[0]).includes('hp'), true);
});
