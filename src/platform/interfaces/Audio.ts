export interface IAudio {
  play(sound: string): void;
  tone(frequency: number, duration: number): void;
  stop(): void;
}
