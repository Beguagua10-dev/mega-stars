const norm = (x, y) => {
  const len = Math.hypot(x, y);
  return len < 1e-6 ? { x: 0, y: 0 } : { x: x / len, y: y / len };
};

/// Keyboard + mouse on desktop, virtual stick + fire pad on touch devices.
export class InputController {
  constructor(canvas, hudElements) {
    this.canvas = canvas;
    this.keys = new Set();
    this.mouse = { x: 0, y: 0, down: false, right: false };
    this.touchMove = { x: 0, y: 0 };
    this.touchAim = { x: 0, y: 0 };
    this.touchShoot = false;
    this.touchSuper = false;
    this.isTouch = window.matchMedia('(pointer: coarse)').matches;

    window.addEventListener('keydown', (e) => {
      this.keys.add(e.key.toLowerCase());
      if (e.key === ' ') e.preventDefault();
    });
    window.addEventListener('keyup', (e) => this.keys.delete(e.key.toLowerCase()));
    window.addEventListener('blur', () => this.keys.clear());

    canvas.addEventListener('mousemove', (e) => {
      this.mouse.x = e.clientX;
      this.mouse.y = e.clientY;
    });
    canvas.addEventListener('mousedown', (e) => {
      if (e.button === 0) this.mouse.down = true;
      if (e.button === 2) this.mouse.right = true;
    });
    window.addEventListener('mouseup', (e) => {
      if (e.button === 0) this.mouse.down = false;
      if (e.button === 2) this.mouse.right = false;
    });
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());

    if (hudElements) this.bindTouch(hudElements);
  }

  bindTouch({ stick, knob, fire, superButton }) {
    const trackStick = (event) => {
      event.preventDefault();
      const rect = stick.getBoundingClientRect();
      const touch = event.touches[0];
      const dx = touch.clientX - (rect.left + rect.width / 2);
      const dy = touch.clientY - (rect.top + rect.height / 2);
      const dir = Math.hypot(dx, dy) > 8 ? norm(dx, dy) : { x: 0, y: 0 };
      this.touchMove = dir;
      knob.style.transform = `translate(${dir.x * 32}px, ${dir.y * 32}px)`;
    };
    stick.addEventListener('touchstart', trackStick, { passive: false });
    stick.addEventListener('touchmove', trackStick, { passive: false });
    stick.addEventListener('touchend', () => {
      this.touchMove = { x: 0, y: 0 };
      knob.style.transform = 'translate(0, 0)';
    });

    // Dragging on the fire pad aims; a tap shoots straight ahead.
    const trackFire = (event) => {
      event.preventDefault();
      const rect = fire.getBoundingClientRect();
      const touch = event.touches[0];
      const dx = touch.clientX - (rect.left + rect.width / 2);
      const dy = touch.clientY - (rect.top + rect.height / 2);
      if (Math.hypot(dx, dy) > 10) this.touchAim = norm(dx, dy);
      this.touchShoot = true;
    };
    fire.addEventListener('touchstart', trackFire, { passive: false });
    fire.addEventListener('touchmove', trackFire, { passive: false });
    fire.addEventListener('touchend', () => {
      this.touchShoot = false;
    });

    superButton.addEventListener('touchstart', (event) => {
      event.preventDefault();
      this.touchSuper = true;
    }, { passive: false });
    superButton.addEventListener('touchend', () => {
      this.touchSuper = false;
    });
  }

  /// `self` is the local player from the last snapshot; it is used to aim
  /// towards the mouse pointer in world space.
  read(self, camera, tilePixels) {
    const move = { x: 0, y: 0 };
    if (this.keys.has('w') || this.keys.has('arrowup')) move.y -= 1;
    if (this.keys.has('s') || this.keys.has('arrowdown')) move.y += 1;
    if (this.keys.has('a') || this.keys.has('arrowleft')) move.x -= 1;
    if (this.keys.has('d') || this.keys.has('arrowright')) move.x += 1;

    let aim = { x: 0, y: 0 };
    if (self && camera) {
      const worldX = camera.x + (this.mouse.x - window.innerWidth / 2) / tilePixels;
      const worldY = camera.y + (this.mouse.y - window.innerHeight / 2) / tilePixels;
      aim = norm(worldX - self.x, worldY - self.y);
    }

    const usingTouch = this.touchMove.x !== 0 || this.touchMove.y !== 0 || this.touchShoot;
    return {
      move: usingTouch && move.x === 0 && move.y === 0 ? this.touchMove : norm(move.x, move.y),
      aim: this.touchShoot && (this.touchAim.x !== 0 || this.touchAim.y !== 0) ? this.touchAim : aim,
      shoot: this.mouse.down || this.keys.has(' ') || this.touchShoot,
      useSuper: this.mouse.right || this.keys.has('e') || this.touchSuper,
    };
  }
}
