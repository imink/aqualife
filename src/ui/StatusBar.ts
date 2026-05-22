import { IDisplay } from '../platform/interfaces/Display';
import { DISPLAY_WIDTH } from '../core/World';

export function drawStatusBar(display: IDisplay, time: string): void {
  // Time display at top left
  display.drawText(time, 3, 3, 0xcccccc, 6);
}

export function drawMoodIndicator(display: IDisplay, happiness: number, x: number, y: number): void {
  display.drawText('MOD', x, y, 0xaaaaaa, 6);
  // Mood face
  const faceX = x + 18;
  const faceColor = happiness > 60 ? 0x00cc00 : happiness > 30 ? 0xcccc00 : 0xcc0000;
  display.drawCircle(faceX + 4, y + 3, 3, faceColor);
}

export function drawButtonHints(display: IDisplay, y: number): void {
  display.drawText('A FEED', 5, y, 0x666688, 6);
  display.drawText('B PLAY', DISPLAY_WIDTH - 40, y, 0x666688, 6);
}
