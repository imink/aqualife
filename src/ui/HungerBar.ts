import { IDisplay } from '../platform/interfaces/Display';

export function drawHungerBar(display: IDisplay, hunger: number, x: number, y: number): void {
  // Label
  display.drawText('HGR', x, y, 0xaaaaaa, 6);
  // Bar background
  display.drawRect(x + 16, y, 40, 5, 0x333333);
  // Bar fill
  const fillWidth = Math.floor((hunger / 100) * 38);
  const color = hunger > 50 ? 0x00cc00 : hunger > 25 ? 0xcccc00 : 0xcc0000;
  display.drawRect(x + 17, y + 1, fillWidth, 3, color);
}
