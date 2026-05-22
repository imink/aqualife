import { World } from '../core/World';
import { serializeFish, deserializeFish } from '../core/Fish';

const SAVE_KEY = 'aqualife_save';
const SAVE_INTERVAL = 30000; // 30 seconds

let saveTimer = 0;

export interface SaveData {
  fish: ReturnType<typeof serializeFish>[];
  settings: {
    volume: number;
  };
  timestamp: number;
}

export function updateSaveSystem(world: World, dt: number): void {
  saveTimer += dt;
  if (saveTimer >= SAVE_INTERVAL) {
    saveTimer = 0;
    saveWorld(world);
  }
}

export function saveWorld(world: World): void {
  const data: SaveData = {
    fish: world.fish.map(serializeFish),
    settings: { volume: 1 },
    timestamp: Date.now(),
  };
  try {
    localStorage.setItem(SAVE_KEY, JSON.stringify(data));
  } catch (e) {
    console.warn('Failed to save:', e);
  }
}

export function loadWorld(world: World): boolean {
  try {
    const raw = localStorage.getItem(SAVE_KEY);
    if (!raw) return false;
    const data: SaveData = JSON.parse(raw);
    if (data.fish && data.fish.length > 0) {
      world.fish = data.fish.map(deserializeFish);
      return true;
    }
  } catch (e) {
    console.warn('Failed to load save:', e);
  }
  return false;
}
