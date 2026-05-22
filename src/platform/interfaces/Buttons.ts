export interface IButtons {
  update(): void;
  btnA: ButtonState;
  btnB: ButtonState;
  space: ButtonState;
}

export interface ButtonState {
  wasPressed(): boolean;
  pressed(): boolean;
  released(): boolean;
}
