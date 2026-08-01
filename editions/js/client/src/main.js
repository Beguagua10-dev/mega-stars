import { Connection } from './net.js';
import { InputController } from './input.js';
import { Renderer } from './renderer.js';

const PHASE_LABEL = ['AQUECIMENTO', 'EM JOGO', 'CONTAGEM', 'FIM'];

const canvas = document.getElementById('game');
const menu = document.getElementById('menu');
const hud = document.getElementById('hud');
const touch = document.getElementById('touch');
const statusText = document.getElementById('status');
const rosterEl = document.getElementById('roster');
const nameInput = document.getElementById('name');
const serverInput = document.getElementById('server');
const scoreBlue = document.getElementById('score-blue');
const scoreRed = document.getElementById('score-red');
const timerEl = document.getElementById('timer');
const feedEl = document.getElementById('feed');
const superButton = document.getElementById('super');

const renderer = new Renderer(canvas);
const connection = new Connection();
const input = new InputController(canvas, {
  stick: document.getElementById('stick'),
  knob: document.querySelector('#stick .knob'),
  fire: document.getElementById('fire'),
  superButton,
});

let selectedBrawler = 'faisca';
let playing = false;

nameInput.value = localStorage.getItem('megastars.name') || '';
serverInput.value = localStorage.getItem('megastars.server') || '';

/// The menu roster is fetched from the server so browser and native clients
/// always show the same characters.
async function loadRoster() {
  const roster = await fetch('/api/roster').then((r) => r.json());
  rosterEl.innerHTML = '';
  for (const brawler of roster) {
    const card = document.createElement('div');
    card.className = `card${brawler.id === selectedBrawler ? ' selected' : ''}`;
    card.innerHTML = `
      <div class="portrait" style="background:${brawler.color}"></div>
      <div class="name">${brawler.displayName}</div>
      <div class="blurb">${brawler.blurb}</div>
      <div class="stats">${brawler.maxHealth} vida · alcance ${brawler.attackRange}</div>`;
    card.addEventListener('click', () => {
      selectedBrawler = brawler.id;
      for (const other of rosterEl.children) other.classList.remove('selected');
      card.classList.add('selected');
    });
    rosterEl.appendChild(card);
  }
}

document.getElementById('play').addEventListener('click', async () => {
  const name = nameInput.value.trim() || 'Jogador';
  localStorage.setItem('megastars.name', name);
  localStorage.setItem('megastars.server', serverInput.value.trim());
  statusText.textContent = 'Conectando...';
  try {
    await connection.connect(serverInput.value, name, selectedBrawler);
    menu.classList.add('hidden');
    hud.classList.remove('hidden');
    if (input.isTouch) touch.classList.remove('hidden');
    playing = true;
  } catch (err) {
    statusText.textContent = `Falha ao conectar: ${err.message}`;
  }
});

connection.onError = () => {
  if (!playing) return;
  playing = false;
  menu.classList.remove('hidden');
  hud.classList.add('hidden');
  statusText.textContent = 'Conexao encerrada pelo servidor.';
};

function updateHud(state, self) {
  scoreBlue.textContent = state.match.g0;
  scoreRed.textContent = state.match.g1;
  timerEl.textContent = `${PHASE_LABEL[state.match.ph]} ${Math.max(0, Math.ceil(state.match.pt))}s`;
  feedEl.innerHTML = (state.feed || []).map((line) => `<div>${line}</div>`).join('');
  if (self) superButton.classList.toggle('ready', self.sc >= 100);
}

function frame() {
  requestAnimationFrame(frame);
  const state = connection.state;
  if (!playing || !state || !connection.arena) return;

  const self = state.players.find((p) => p.id === connection.playerId);
  connection.sendInput(input.read(self, renderer.camera, renderer.tilePixels));
  renderer.draw(state, connection.arena, connection.roster, connection.playerId);
  updateHud(state, self);
}

loadRoster();
requestAnimationFrame(frame);
