/// WebSocket transport for the browser client. It speaks the same JSON
/// messages the native editions send over TCP (see docs/protocol.md).
export class Connection {
  constructor() {
    this.socket = null;
    this.playerId = null;
    this.arena = null;
    this.roster = [];
    this.state = null;
    this.onWelcome = () => {};
    this.onError = () => {};
  }

  static resolveUrl(serverInput) {
    const value = (serverInput || '').trim();
    if (!value) {
      const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
      return `${scheme}://${window.location.host}`;
    }
    if (value.startsWith('ws://') || value.startsWith('wss://')) return value;
    if (value.startsWith('http://')) return `ws://${value.slice(7)}`;
    if (value.startsWith('https://')) return `wss://${value.slice(8)}`;
    return `ws://${value}`;
  }

  connect(serverInput, name, brawlerId) {
    return new Promise((resolve, reject) => {
      let socket;
      try {
        socket = new WebSocket(Connection.resolveUrl(serverInput));
      } catch (err) {
        reject(err);
        return;
      }
      this.socket = socket;

      socket.addEventListener('open', () => {
        socket.send(JSON.stringify({ t: 'join', name, brawler: brawlerId }));
      });
      socket.addEventListener('error', () => {
        reject(new Error('nao foi possivel conectar ao servidor'));
        this.onError();
      });
      socket.addEventListener('close', () => this.onError());
      socket.addEventListener('message', (event) => {
        const message = JSON.parse(event.data);
        if (message.t === 'welcome') {
          this.playerId = message.id;
          this.arena = { width: message.w, height: message.h, tiles: message.tiles };
          this.roster = message.roster;
          this.onWelcome(message);
          resolve(message);
        } else if (message.t === 'state') {
          this.state = message;
        }
      });
    });
  }

  sendInput(input) {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) return;
    this.socket.send(
      JSON.stringify({
        t: 'input',
        mx: Number(input.move.x.toFixed(3)),
        my: Number(input.move.y.toFixed(3)),
        ax: Number(input.aim.x.toFixed(3)),
        ay: Number(input.aim.y.toFixed(3)),
        s: input.shoot ? 1 : 0,
        u: input.useSuper ? 1 : 0,
      }),
    );
  }
}
