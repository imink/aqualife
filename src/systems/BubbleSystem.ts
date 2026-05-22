import { World, AQUARIUM_LEFT, AQUARIUM_RIGHT, AQUARIUM_BOTTOM, AQUARIUM_TOP } from '../core/World';
import { createBubble, Bubble } from '../core/Bubble';
import { MAX_BUBBLES } from '../core/World';

let spawnTimer = 0;
const SPAWN_MIN = 500;
const SPAWN_MAX = 2000;
let nextSpawn = SPAWN_MIN + Math.random() * (SPAWN_MAX - SPAWN_MIN);

export function updateBubbleSystem(world: World, dt: number): void {
  // Spawn bubbles
  spawnTimer += dt;
  if (spawnTimer >= nextSpawn && world.bubbles.length < MAX_BUBBLES) {
    const x = AQUARIUM_LEFT + Math.random() * (AQUARIUM_RIGHT - AQUARIUM_LEFT);
    world.bubbles.push(createBubble(x, AQUARIUM_BOTTOM - 5));
    spawnTimer = 0;
    nextSpawn = SPAWN_MIN + Math.random() * (SPAWN_MAX - SPAWN_MIN);
  }

  // Update bubbles
  for (const bubble of world.bubbles) {
    if (!bubble.alive) continue;
    bubble.y -= bubble.velocity * (dt / 40);
    bubble.x += bubble.drift * (dt / 40);

    // Random drift adjustment
    if (Math.random() < 0.02) {
      bubble.drift = (Math.random() - 0.5) * 0.3;
    }

    // Remove at top
    if (bubble.y < AQUARIUM_TOP) {
      bubble.alive = false;
    }
  }

  // Clean dead bubbles
  world.bubbles = world.bubbles.filter(b => b.alive);
}
