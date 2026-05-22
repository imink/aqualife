import { Fish, FishState } from '../core/Fish';
import { World, AQUARIUM_TOP, AQUARIUM_BOTTOM, AQUARIUM_LEFT, AQUARIUM_RIGHT, FoodParticle } from '../core/World';

const THINK_INTERVAL = 200; // 5 Hz = 200ms
const MAX_SPEED = 1.0;
const SCARED_SPEED = 2.0;

export function updateFishSystem(world: World, dt: number, accelX: number): void {
  for (const fish of world.fish) {
    fish.thinkTimer += dt;

    // AI decision at 5 Hz
    if (fish.thinkTimer >= THINK_INTERVAL) {
      fish.thinkTimer = 0;
      think(fish, world, accelX);
    }

    // Skip movement/animation for hidden fish
    if (!fish.visible) continue;

    // Movement (every frame)
    updateMovement(fish, dt);

    // Animation
    fish.animationTimer += dt;
    if (fish.animationTimer > 150) {
      fish.animationFrame = (fish.animationFrame + 1) % 8;
      fish.animationTimer = 0;
    }

    // Pet stat decay
    fish.hunger = Math.max(0, fish.hunger - 0.001 * dt);
    fish.happiness = Math.max(0, fish.happiness - 0.0005 * dt);
    fish.age += dt / 1000;
  }
}

function think(fish: Fish, world: World, accelX: number): void {
  const state = fish.stateMachine.getState();
  const stateTime = fish.stateMachine.getTimer();

  // Check for scare (high acceleration)
  if (Math.abs(accelX) > 1.5 && fish.visible) {
    fish.stateMachine.force('Scared');
    return;
  }

  switch (state) {
    case 'Idle':
      // Random direction change
      if (Math.random() < 0.1) {
        fish.vx = (Math.random() - 0.5) * MAX_SPEED;
        fish.vy = (Math.random() - 0.5) * MAX_SPEED * 0.5;
      }
      // Seek food if hungry
      if (fish.hunger < 50 && world.food.length > 0) {
        fish.stateMachine.transition('SeekFood');
      }
      // Sleep if low energy
      if (fish.energy < 20 && stateTime > 5000) {
        fish.stateMachine.transition('Sleep');
      }
      // Play randomly
      if (Math.random() < 0.02 && fish.happiness > 50) {
        fish.stateMachine.transition('Play');
      }
      break;

    case 'SeekFood':
      seekFood(fish, world);
      if (world.food.length === 0 || fish.hunger > 80) {
        fish.stateMachine.transition('Idle');
      }
      break;

    case 'Sleep':
      fish.vx *= 0.5;
      fish.vy *= 0.5;
      fish.energy = Math.min(100, fish.energy + 0.5);
      if (fish.energy > 80 || stateTime > 10000) {
        fish.stateMachine.transition('Idle');
      }
      break;

    case 'Scared':
      // Dart quickly toward edges
      if (stateTime < 300) {
        fish.vx = (fish.direction > 0 ? 1 : -1) * SCARED_SPEED * 2;
        fish.vy = (Math.random() - 0.5) * SCARED_SPEED;
      }
      // After brief dart, hide
      if (stateTime > 400) {
        fish.stateMachine.transition('Hidden');
        fish.visible = false;
        // Each fish gets a staggered reappear time (3-6s)
        fish.hideTimer = 3000 + Math.random() * 3000;
      }
      break;

    case 'Play':
      fish.vx = Math.sin(fish.age * 3) * MAX_SPEED;
      fish.vy = Math.cos(fish.age * 2) * MAX_SPEED * 0.5;
      fish.happiness = Math.min(100, fish.happiness + 0.5);
      if (stateTime > 3000) {
        fish.stateMachine.transition('Idle');
      }
      break;

    case 'Hidden':
      // Fish is hidden, count down timer
      fish.hideTimer -= THINK_INTERVAL;
      fish.vx = 0;
      fish.vy = 0;
      if (fish.hideTimer <= 0) {
        // Reappear from a random edge
        fish.visible = true;
        const fromLeft = Math.random() > 0.5;
        fish.x = fromLeft ? AQUARIUM_LEFT : AQUARIUM_RIGHT - 20;
        fish.y = AQUARIUM_TOP + 10 + Math.random() * (AQUARIUM_BOTTOM - AQUARIUM_TOP - 40);
        fish.direction = fromLeft ? 1 : -1;
        fish.vx = fish.direction * 0.3;
        fish.stateMachine.transition('Idle');
      }
      break;
  }

  fish.stateMachine.update(THINK_INTERVAL);
}

function seekFood(fish: Fish, world: World): void {
  let nearest: FoodParticle | null = null;
  let minDist = Infinity;

  for (const food of world.food) {
    if (!food.alive) continue;
    const dx = food.x - fish.x;
    const dy = food.y - fish.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist < minDist) {
      minDist = dist;
      nearest = food;
    }
  }

  if (nearest) {
    const dx = nearest.x - fish.x;
    const dy = nearest.y - fish.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    fish.vx = (dx / dist) * MAX_SPEED;
    fish.vy = (dy / dist) * MAX_SPEED;

    // Eat food if close enough
    if (dist < 8) {
      nearest.alive = false;
      fish.hunger = Math.min(100, fish.hunger + 25);
      fish.happiness = Math.min(100, fish.happiness + 10);
      fish.friendship = Math.min(100, fish.friendship + 3);
    }
  }
}

function updateMovement(fish: Fish, dt: number): void {
  const speed = dt / 40; // normalize

  fish.x += fish.vx * speed;
  fish.y += fish.vy * speed;

  // Update direction based on velocity
  if (Math.abs(fish.vx) > 0.05) {
    fish.direction = fish.vx > 0 ? 1 : -1;
  }

  // Boundary bounce
  if (fish.x < AQUARIUM_LEFT + 4) {
    fish.x = AQUARIUM_LEFT + 4;
    fish.vx = Math.abs(fish.vx) * 0.8;
  }
  if (fish.x > AQUARIUM_RIGHT - 16) {
    fish.x = AQUARIUM_RIGHT - 16;
    fish.vx = -Math.abs(fish.vx) * 0.8;
  }
  if (fish.y < AQUARIUM_TOP + 4) {
    fish.y = AQUARIUM_TOP + 4;
    fish.vy = Math.abs(fish.vy) * 0.8;
  }
  if (fish.y > AQUARIUM_BOTTOM - 20) {
    fish.y = AQUARIUM_BOTTOM - 20;
    fish.vy = -Math.abs(fish.vy) * 0.8;
  }
}
