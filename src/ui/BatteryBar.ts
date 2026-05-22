import { IDisplay } from '../platform/interfaces/Display';
import { DISPLAY_WIDTH } from '../core/World';

export function drawBatteryBar(display: IDisplay): void {
  // Battery icon at top right
  const bx = DISPLAY_WIDTH - 18;
  const by = 2;
  // Battery outline
  display.drawRect(bx, by, 14, 7, 0x888888);
  display.drawRect(bx + 14, by + 2, 2, 3, 0x888888);
  // Battery fill (green)
  display.drawRect(bx + 1, by + 1, 10, 5, 0x00cc00);
}
