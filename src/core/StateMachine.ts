export type State = string;

export interface StateMachineConfig<T extends State> {
  initial: T;
  transitions: Record<T, T[]>;
}

export class StateMachine<T extends State> {
  private current: T;
  private transitions: Record<string, string[]>;
  private timer = 0;

  constructor(config: StateMachineConfig<T>) {
    this.current = config.initial;
    this.transitions = config.transitions as Record<string, string[]>;
  }

  getState(): T {
    return this.current;
  }

  getTimer(): number {
    return this.timer;
  }

  update(dt: number): void {
    this.timer += dt;
  }

  canTransition(to: T): boolean {
    const allowed = this.transitions[this.current];
    return allowed ? allowed.includes(to) : false;
  }

  transition(to: T): boolean {
    if (this.canTransition(to)) {
      this.current = to;
      this.timer = 0;
      return true;
    }
    return false;
  }

  force(to: T): void {
    this.current = to;
    this.timer = 0;
  }
}
