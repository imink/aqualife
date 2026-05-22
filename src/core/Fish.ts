import { StateMachine } from './StateMachine';

export type FishState = 'Idle' | 'SeekFood' | 'Sleep' | 'Scared' | 'Play' | 'Hidden';

export interface Fish {
  id: string;
  species: string;
  x: number;
  y: number;
  vx: number;
  vy: number;
  direction: number; // 1 = right, -1 = left
  hunger: number;
  happiness: number;
  energy: number;
  age: number;
  friendship: number;
  animationFrame: number;
  animationTimer: number;
  stateMachine: StateMachine<FishState>;
  thinkTimer: number;
  hideTimer: number; // time remaining before reappearing
  visible: boolean;
}

const FISH_STATE_TRANSITIONS: Record<FishState, FishState[]> = {
  Idle: ['SeekFood', 'Sleep', 'Scared', 'Play'],
  SeekFood: ['Idle', 'Scared'],
  Sleep: ['Idle', 'Scared'],
  Scared: ['Idle', 'Hidden'],
  Play: ['Idle', 'Scared'],
  Hidden: ['Idle'],
};

let fishIdCounter = 0;

export function createFish(species: string, x: number, y: number): Fish {
  return {
    id: `fish_${fishIdCounter++}`,
    species,
    x,
    y,
    vx: (Math.random() - 0.5) * 0.5,
    vy: (Math.random() - 0.5) * 0.2,
    direction: Math.random() > 0.5 ? 1 : -1,
    hunger: 80,
    happiness: 70,
    energy: 90,
    age: 0,
    friendship: 0,
    animationFrame: 0,
    animationTimer: 0,
    stateMachine: new StateMachine<FishState>({
      initial: 'Idle',
      transitions: FISH_STATE_TRANSITIONS,
    }),
    thinkTimer: 0,
    hideTimer: 0,
    visible: true,
  };
}

export function serializeFish(fish: Fish) {
  return {
    id: fish.id,
    species: fish.species,
    x: fish.x,
    y: fish.y,
    vx: fish.vx,
    vy: fish.vy,
    direction: fish.direction,
    hunger: fish.hunger,
    happiness: fish.happiness,
    energy: fish.energy,
    age: fish.age,
    friendship: fish.friendship,
    state: fish.stateMachine.getState(),
  };
}

export function deserializeFish(data: ReturnType<typeof serializeFish>): Fish {
  const fish = createFish(data.species, data.x, data.y);
  fish.id = data.id;
  fish.vx = data.vx;
  fish.vy = data.vy;
  fish.direction = data.direction;
  fish.hunger = data.hunger;
  fish.happiness = data.happiness;
  fish.energy = data.energy;
  fish.age = data.age;
  fish.friendship = data.friendship;
  fish.stateMachine.force(data.state as FishState);
  return fish;
}
