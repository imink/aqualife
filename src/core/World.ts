import { Fish, createFish } from './Fish';
import { Bubble } from './Bubble';

// Landscape orientation (device rotated 90°)
export const DISPLAY_WIDTH = 240;
export const DISPLAY_HEIGHT = 135;

export const MAX_FISH = 15;
export const MAX_BUBBLES = 20;
export const MAX_PLANTS = 6;

// Aquarium boundaries (leaving space for UI)
export const AQUARIUM_TOP = 12;
export const AQUARIUM_BOTTOM = DISPLAY_HEIGHT - 20;
export const AQUARIUM_LEFT = 2;
export const AQUARIUM_RIGHT = DISPLAY_WIDTH - 2;

export interface FoodParticle {
  x: number;
  y: number;
  vy: number;
  alive: boolean;
}

export interface Plant {
  x: number;
  y: number;
  offset: number;
  height: number;
}

export interface World {
  fish: Fish[];
  bubbles: Bubble[];
  food: FoodParticle[];
  plants: Plant[];
  time: number;
}

export function createWorld(): World {
  const world: World = {
    fish: [],
    bubbles: [],
    food: [],
    plants: [],
    time: 0,
  };

  // Add initial fish
  world.fish.push(createFish('whale', 120, 42));
  world.fish.push(createFish('hammerhead', 70, 62));
  world.fish.push(createFish('blue', 150, 60));
  world.fish.push(createFish('gold', 50, 75));

  // Add plants
  for (let i = 0; i < 5; i++) {
    world.plants.push({
      x: 15 + i * 55,
      y: AQUARIUM_BOTTOM,
      offset: Math.random() * Math.PI * 2,
      height: 12 + Math.random() * 16,
    });
  }

  return world;
}
