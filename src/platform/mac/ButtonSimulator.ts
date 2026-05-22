import { IButtons, ButtonState } from '../interfaces/Buttons';

class KeyButton implements ButtonState {
  private _pressed = false;
  private _wasPressed = false;
  private _released = false;
  private _prevDown = false;
  private _down = false;

  constructor(key: string) {
    window.addEventListener('keydown', (e) => {
      if (e.key.toLowerCase() === key) this._down = true;
    });
    window.addEventListener('keyup', (e) => {
      if (e.key.toLowerCase() === key) this._down = false;
    });
  }

  update(): void {
    this._wasPressed = this._down && !this._prevDown;
    this._released = !this._down && this._prevDown;
    this._pressed = this._down;
    this._prevDown = this._down;
  }

  wasPressed(): boolean { return this._wasPressed; }
  pressed(): boolean { return this._pressed; }
  released(): boolean { return this._released; }
}

export class ButtonSimulator implements IButtons {
  btnA: KeyButton;
  btnB: KeyButton;
  space: KeyButton;

  constructor() {
    this.btnA = new KeyButton('a');
    this.btnB = new KeyButton('s');
    this.space = new KeyButton(' ');
  }

  update(): void {
    this.btnA.update();
    this.btnB.update();
    this.space.update();
  }
}
