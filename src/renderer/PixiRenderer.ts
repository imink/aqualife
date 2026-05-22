import { IDisplay } from '../platform/interfaces/Display';
import { DISPLAY_WIDTH, DISPLAY_HEIGHT } from '../core/World';
import { Application, Container, Graphics, Text, TextStyle, Sprite, Texture, RenderTexture } from 'pixi.js';

const SCALE = 3;

// RGB565 color quantization
function toRGB565(r: number, g: number, b: number): [number, number, number] {
  const r5 = (r >> 3) << 3;
  const g6 = (g >> 2) << 2;
  const b5 = (b >> 3) << 3;
  return [r5, g6, b5];
}

function colorToRGB565(color: number): number {
  const r = (color >> 16) & 0xff;
  const g = (color >> 8) & 0xff;
  const b = color & 0xff;
  const [r5, g6, b5] = toRGB565(r, g, b);
  return (r5 << 16) | (g6 << 8) | b5;
}

export class PixiRenderer implements IDisplay {
  private app: Application;
  private stage: Container;
  private graphics: Graphics;
  private spriteCache: Map<string, Texture> = new Map();
  private renderCommands: Array<() => void> = [];
  private initialized = false;

  constructor() {
    this.app = new Application();
    this.stage = new Container();
    this.graphics = new Graphics();
  }

  async init(container: HTMLElement): Promise<HTMLCanvasElement> {
    await this.app.init({
      width: DISPLAY_WIDTH * SCALE,
      height: DISPLAY_HEIGHT * SCALE,
      backgroundColor: 0x000820,
      antialias: false,
      roundPixels: true,
      resolution: 1,
    });

    // Disable texture smoothing globally
    this.app.stage.addChild(this.stage);
    this.stage.scale.set(SCALE);

    container.appendChild(this.app.canvas);
    this.app.canvas.style.imageRendering = 'pixelated';

    this.stage.addChild(this.graphics);
    this.initialized = true;

    // Stop the default ticker - we control frame rate
    this.app.ticker.stop();

    return this.app.canvas;
  }

  drawSprite(id: string, x: number, y: number, w: number, h: number, flipX?: boolean): void {
    this.renderCommands.push(() => {
      const color = this.getSpriteColor(id);
      const quantized = colorToRGB565(color);
      this.graphics.rect(
        flipX ? x + w : x,
        y,
        flipX ? -w : w,
        h
      );
      this.graphics.fill(quantized);

      // Draw fish detail pixels
      this.drawFishPixels(id, x, y, w, h, flipX);
    });
  }

  private drawFishPixels(id: string, x: number, y: number, w: number, h: number, flipX?: boolean): void {
    // Simple pixel art fish representation
    const eyeX = flipX ? x + 3 : x + w - 4;
    const eyeY = y + Math.floor(h / 3);
    this.graphics.rect(eyeX, eyeY, 2, 2);
    this.graphics.fill(0xffffff);

    // Tail
    const tailX = flipX ? x + w - 4 : x + 1;
    const tailY = y + Math.floor(h / 2) - 2;
    this.graphics.rect(tailX, tailY, 3, 4);
    const bodyColor = this.getSpriteColor(id);
    const darkerColor = colorToRGB565(
      ((bodyColor >> 16 & 0xff) * 0.7 << 16) |
      ((bodyColor >> 8 & 0xff) * 0.7 << 8) |
      ((bodyColor & 0xff) * 0.7)
    );
    this.graphics.fill(darkerColor);
  }

  private getSpriteColor(id: string): number {
    switch (id) {
      case 'clown': return 0xff6600;
      case 'blue': return 0x0066ff;
      case 'gold': return 0xffcc00;
      case 'red': return 0xff3333;
      case 'green': return 0x33cc66;
      default: return 0xff8800;
    }
  }

  drawText(text: string, x: number, y: number, color: number, size: number = 6): void {
    this.renderCommands.push(() => {
      const quantized = colorToRGB565(color);
      // Simple bitmap-style text using rectangles for each char
      for (let i = 0; i < text.length; i++) {
        // Use PixiJS Text but at device resolution
        const charX = x + i * (size * 0.7);
        this.drawCharPixels(text[i], charX, y, quantized, size);
      }
    });
  }

  private drawCharPixels(char: string, x: number, y: number, color: number, size: number): void {
    // Minimal pixel font - just draw a small block per char for authentic LCD look
    const s = Math.max(1, Math.floor(size / 6));
    // Simple 3x5 pixel font patterns for common chars
    const patterns: Record<string, number[]> = {
      '0': [0b111, 0b101, 0b101, 0b101, 0b111],
      '1': [0b010, 0b110, 0b010, 0b010, 0b111],
      '2': [0b111, 0b001, 0b111, 0b100, 0b111],
      '3': [0b111, 0b001, 0b111, 0b001, 0b111],
      '4': [0b101, 0b101, 0b111, 0b001, 0b001],
      '5': [0b111, 0b100, 0b111, 0b001, 0b111],
      '6': [0b111, 0b100, 0b111, 0b101, 0b111],
      '7': [0b111, 0b001, 0b010, 0b010, 0b010],
      '8': [0b111, 0b101, 0b111, 0b101, 0b111],
      '9': [0b111, 0b101, 0b111, 0b001, 0b111],
      ':': [0b000, 0b010, 0b000, 0b010, 0b000],
      ' ': [0b000, 0b000, 0b000, 0b000, 0b000],
      '%': [0b101, 0b001, 0b010, 0b100, 0b101],
    };

    const upper = char.toUpperCase();
    // Letters A-Z simple patterns
    const letterPatterns: Record<string, number[]> = {
      'A': [0b010, 0b101, 0b111, 0b101, 0b101],
      'B': [0b110, 0b101, 0b110, 0b101, 0b110],
      'C': [0b111, 0b100, 0b100, 0b100, 0b111],
      'D': [0b110, 0b101, 0b101, 0b101, 0b110],
      'E': [0b111, 0b100, 0b110, 0b100, 0b111],
      'F': [0b111, 0b100, 0b110, 0b100, 0b100],
      'G': [0b111, 0b100, 0b101, 0b101, 0b111],
      'H': [0b101, 0b101, 0b111, 0b101, 0b101],
      'I': [0b111, 0b010, 0b010, 0b010, 0b111],
      'J': [0b001, 0b001, 0b001, 0b101, 0b010],
      'K': [0b101, 0b110, 0b100, 0b110, 0b101],
      'L': [0b100, 0b100, 0b100, 0b100, 0b111],
      'M': [0b101, 0b111, 0b111, 0b101, 0b101],
      'N': [0b101, 0b111, 0b111, 0b111, 0b101],
      'O': [0b111, 0b101, 0b101, 0b101, 0b111],
      'P': [0b111, 0b101, 0b111, 0b100, 0b100],
      'Q': [0b111, 0b101, 0b101, 0b111, 0b001],
      'R': [0b111, 0b101, 0b111, 0b110, 0b101],
      'S': [0b111, 0b100, 0b111, 0b001, 0b111],
      'T': [0b111, 0b010, 0b010, 0b010, 0b010],
      'U': [0b101, 0b101, 0b101, 0b101, 0b111],
      'V': [0b101, 0b101, 0b101, 0b101, 0b010],
      'W': [0b101, 0b101, 0b111, 0b111, 0b101],
      'X': [0b101, 0b101, 0b010, 0b101, 0b101],
      'Y': [0b101, 0b101, 0b010, 0b010, 0b010],
      'Z': [0b111, 0b001, 0b010, 0b100, 0b111],
    };

    const allPatterns = { ...patterns, ...letterPatterns };
    const pattern = allPatterns[upper];
    if (!pattern) {
      // Unknown char - draw a filled block
      this.graphics.rect(x, y, 3 * s, 5 * s);
      this.graphics.fill(color);
      return;
    }

    for (let row = 0; row < 5; row++) {
      for (let col = 0; col < 3; col++) {
        if (pattern[row] & (1 << (2 - col))) {
          this.graphics.rect(x + col * s, y + row * s, s, s);
          this.graphics.fill(color);
        }
      }
    }
  }

  drawRect(x: number, y: number, w: number, h: number, color: number): void {
    this.renderCommands.push(() => {
      const quantized = colorToRGB565(color);
      this.graphics.rect(x, y, w, h);
      this.graphics.fill(quantized);
    });
  }

  drawCircle(x: number, y: number, radius: number, color: number): void {
    this.renderCommands.push(() => {
      const quantized = colorToRGB565(color);
      this.graphics.circle(x, y, radius);
      this.graphics.fill(quantized);
    });
  }

  clear(): void {
    this.renderCommands = [];
    this.graphics.clear();
  }

  present(): void {
    if (!this.initialized) return;
    this.graphics.clear();

    // Draw background - deep ocean gradient (quantized)
    const bgTop = colorToRGB565(0x000830);
    const bgBot = colorToRGB565(0x001040);
    this.graphics.rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    this.graphics.fill(bgTop);

    // Execute all render commands
    for (const cmd of this.renderCommands) {
      cmd();
    }

    this.renderCommands = [];

    // Force render
    this.app.render();
  }

  getApp(): Application {
    return this.app;
  }
}
