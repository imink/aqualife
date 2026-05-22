import { IIMU } from '../interfaces/IMU';

export class IMUSimulator implements IIMU {
  private accel = { x: 0, y: 0, z: 1 };
  private lastMouseX = 0;
  private lastMouseY = 0;
  private mouseDown = false;
  private velocityX = 0;
  private velocityY = 0;

  constructor(canvas: HTMLCanvasElement) {
    canvas.addEventListener('mousedown', (e) => {
      this.mouseDown = true;
      this.lastMouseX = e.clientX;
      this.lastMouseY = e.clientY;
    });

    canvas.addEventListener('mousemove', (e) => {
      if (this.mouseDown) {
        this.velocityX = (e.clientX - this.lastMouseX) * 0.1;
        this.velocityY = (e.clientY - this.lastMouseY) * 0.1;
        this.lastMouseX = e.clientX;
        this.lastMouseY = e.clientY;
      }
    });

    canvas.addEventListener('mouseup', () => {
      this.mouseDown = false;
    });

    canvas.addEventListener('mouseleave', () => {
      this.mouseDown = false;
    });
  }

  update(): void {
    if (this.mouseDown) {
      this.accel.x = Math.max(-2, Math.min(2, this.velocityX));
      this.accel.y = Math.max(-2, Math.min(2, this.velocityY));
    } else {
      this.accel.x *= 0.9;
      this.accel.y *= 0.9;
    }
    this.velocityX *= 0.8;
    this.velocityY *= 0.8;
  }

  getAccel(): { x: number; y: number; z: number } {
    return { ...this.accel };
  }
}
