export interface PetState {
  hunger: number;
  happiness: number;
  energy: number;
  age: number;
  friendship: number;
}

export function createPetState(): PetState {
  return {
    hunger: 80,
    happiness: 70,
    energy: 90,
    age: 0,
    friendship: 0,
  };
}

const DECAY_RATE = 0.02; // per second

export function updatePetState(pet: PetState, dt: number): void {
  const seconds = dt / 1000;
  pet.hunger = Math.max(0, pet.hunger - DECAY_RATE * seconds);
  pet.happiness = Math.max(0, pet.happiness - DECAY_RATE * 0.5 * seconds);
  pet.energy = Math.max(0, Math.min(100, pet.energy + 0.01 * seconds));
  pet.age += seconds;
}

export function feedPet(pet: PetState): void {
  pet.hunger = Math.min(100, pet.hunger + 20);
  pet.friendship = Math.min(100, pet.friendship + 2);
  pet.happiness = Math.min(100, pet.happiness + 5);
}
