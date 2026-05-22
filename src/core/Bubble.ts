export interface Bubble {
  x: number;
  y: number;
  velocity: number;
  drift: number;
  size: number;
  alive: boolean;
}

export function createBubble(x: number, y: number): Bubble {
  return {
    x,
    y,
    velocity: 0.3 + Math.random() * 0.4,
    drift: (Math.random() - 0.5) * 0.2,
    size: 1 + Math.random() * 2,
    alive: true,
  };
}
