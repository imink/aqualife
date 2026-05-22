export interface IIMU {
  getAccel(): { x: number; y: number; z: number };
  update(): void;
}
