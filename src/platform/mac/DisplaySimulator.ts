import { IDisplay } from '../interfaces/Display';
import { DISPLAY_WIDTH, DISPLAY_HEIGHT } from '../../core/World';

const SCALE = 3;

// RGB565 color quantization
function colorToRGB565(color: number): number {
  const r = (color >> 16) & 0xff;
  const g = (color >> 8) & 0xff;
  const b = color & 0xff;
  const r5 = (r >> 3) << 3;
  const g6 = (g >> 2) << 2;
  const b5 = (b >> 3) << 3;
  return (r5 << 16) | (g6 << 8) | b5;
}

export interface SpriteSheet {
  image: HTMLImageElement;
  frameWidth: number;
  frameHeight: number;
  frameCount: number;
}

export class DisplaySimulator implements IDisplay {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private buffer: HTMLCanvasElement;
  private bufCtx: CanvasRenderingContext2D;
  private spriteSheets: Map<string, SpriteSheet> = new Map();
  // RGB565 post-processing buffer
  private rgb565Buffer: HTMLCanvasElement;
  private rgb565Ctx: CanvasRenderingContext2D;

  constructor(container: HTMLElement) {
    // Display canvas (scaled up)
    this.canvas = document.createElement('canvas');
    this.canvas.width = DISPLAY_WIDTH * SCALE;
    this.canvas.height = DISPLAY_HEIGHT * SCALE;
    this.canvas.style.imageRendering = 'pixelated';
    container.appendChild(this.canvas);
    this.ctx = this.canvas.getContext('2d')!;
    this.ctx.imageSmoothingEnabled = false;

    // Internal buffer at device resolution
    this.buffer = document.createElement('canvas');
    this.buffer.width = DISPLAY_WIDTH;
    this.buffer.height = DISPLAY_HEIGHT;
    this.bufCtx = this.buffer.getContext('2d')!;
    this.bufCtx.imageSmoothingEnabled = false;

    // RGB565 post-process buffer
    this.rgb565Buffer = document.createElement('canvas');
    this.rgb565Buffer.width = DISPLAY_WIDTH;
    this.rgb565Buffer.height = DISPLAY_HEIGHT;
    this.rgb565Ctx = this.rgb565Buffer.getContext('2d')!;
  }

  getCanvas(): HTMLCanvasElement {
    return this.canvas;
  }

  async loadSpriteSheet(id: string, url: string, frameWidth: number, frameHeight: number, opts?: { frameCount?: number; removeBackground?: boolean }): Promise<void> {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => {
        const frameCount = opts?.frameCount ?? Math.floor(img.width / frameWidth);

        if (opts?.removeBackground) {
          // Process image to remove checkered/light background
          const proc = document.createElement('canvas');
          proc.width = img.width;
          proc.height = img.height;
          const pCtx = proc.getContext('2d')!;
          pCtx.drawImage(img, 0, 0);
          const imageData = pCtx.getImageData(0, 0, proc.width, proc.height);
          const data = imageData.data;

          for (let i = 0; i < data.length; i += 4) {
            const r = data[i], g = data[i + 1], b = data[i + 2];
            // Remove checkered background more aggressively
            // Checkered pixels are gray (~191-192) and white (~255)
            // Also remove near-white and light gray pixels
            const isGray = Math.abs(r - g) < 20 && Math.abs(g - b) < 20;
            if (isGray && r > 150) {
              data[i + 3] = 0; // Make transparent
            }
          }
          pCtx.putImageData(imageData, 0, 0);

          // Create a cropped sprite sheet canvas (trim vertical whitespace)
          // Find the bounding box of non-transparent pixels
          let minY = proc.height, maxY = 0;
          for (let y = 0; y < proc.height; y++) {
            for (let x = 0; x < proc.width; x++) {
              const idx = (y * proc.width + x) * 4;
              if (data[idx + 3] > 0) {
                minY = Math.min(minY, y);
                maxY = Math.max(maxY, y);
              }
            }
          }

          const cropH = maxY - minY + 1;
          const cropCanvas = document.createElement('canvas');
          cropCanvas.width = proc.width;
          cropCanvas.height = cropH;
          const cropCtx = cropCanvas.getContext('2d')!;
          cropCtx.drawImage(proc, 0, minY, proc.width, cropH, 0, 0, proc.width, cropH);

          // Convert to an image element for drawing
          const processedImg = new Image();
          processedImg.onload = () => {
            const actualFrameW = Math.floor(processedImg.width / frameCount);
            const actualFrameH = processedImg.height;
            this.spriteSheets.set(id, { image: processedImg, frameWidth: actualFrameW, frameHeight: actualFrameH, frameCount });
            resolve();
          };
          processedImg.src = cropCanvas.toDataURL('image/png');
        } else {
          this.spriteSheets.set(id, { image: img, frameWidth, frameHeight, frameCount });
          resolve();
        }
      };
      img.onerror = reject;
      img.src = url;
    });
  }

  hasSpriteSheet(id: string): boolean {
    return this.spriteSheets.has(id);
  }

  drawSpriteFrame(id: string, frame: number, x: number, y: number, w: number, h: number, flipX?: boolean): void {
    const sheet = this.spriteSheets.get(id);
    if (!sheet) {
      // Fallback to colored rectangle
      this.drawSprite(id, x, y, w, h, flipX);
      return;
    }

    const srcX = (frame % sheet.frameCount) * sheet.frameWidth;
    const srcY = 0;

    this.bufCtx.save();
    this.bufCtx.imageSmoothingEnabled = false;
    if (flipX) {
      this.bufCtx.translate(x + w, y);
      this.bufCtx.scale(-1, 1);
      this.bufCtx.drawImage(sheet.image, srcX, srcY, sheet.frameWidth, sheet.frameHeight, 0, 0, w, h);
    } else {
      this.bufCtx.drawImage(sheet.image, srcX, srcY, sheet.frameWidth, sheet.frameHeight, x, y, w, h);
    }
    this.bufCtx.restore();
  }

  drawSprite(id: string, x: number, y: number, w: number, h: number, flipX?: boolean): void {
    // Procedural pixel-art fish for species without sprite sheets
    const color = this.getSpriteColor(id);
    const quantized = colorToRGB565(color);
    const hex = `#${quantized.toString(16).padStart(6, '0')}`;
    const darker = colorToRGB565(
      (((color >> 16 & 0xff) * 0.6) << 16) |
      (((color >> 8 & 0xff) * 0.6) << 8) |
      ((color & 0xff) * 0.6)
    );
    const darkerHex = `#${darker.toString(16).padStart(6, '0')}`;

    this.bufCtx.save();
    if (flipX) {
      this.bufCtx.translate(x + w, y);
      this.bufCtx.scale(-1, 1);
      x = 0; y = 0;
    }

    // Fish body (ellipse shape via pixel rows)
    const cx = x + Math.floor(w * 0.45);
    const cy = y + Math.floor(h / 2);
    const rx = Math.floor(w * 0.4);
    const ry = Math.floor(h * 0.4);

    for (let dy = -ry; dy <= ry; dy++) {
      const rowWidth = Math.floor(rx * Math.sqrt(1 - (dy * dy) / (ry * ry)));
      this.bufCtx.fillStyle = hex;
      this.bufCtx.fillRect(cx - rowWidth, cy + dy, rowWidth * 2, 1);
    }

    // Tail fin (triangle shape)
    const tailX = x + 1;
    for (let dy = -ry; dy <= ry; dy++) {
      const tailW = Math.floor(3 * (1 - Math.abs(dy) / ry));
      this.bufCtx.fillStyle = darkerHex;
      this.bufCtx.fillRect(tailX, cy + dy, tailW, 1);
    }

    // Eye
    this.bufCtx.fillStyle = '#ffffff';
    this.bufCtx.fillRect(cx + rx - 3, cy - 2, 2, 2);
    this.bufCtx.fillStyle = '#000000';
    this.bufCtx.fillRect(cx + rx - 2, cy - 1, 1, 1);

    // Dorsal fin
    this.bufCtx.fillStyle = darkerHex;
    this.bufCtx.fillRect(cx - 2, cy - ry - 2, 4, 2);

    this.bufCtx.restore();
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
    const quantized = colorToRGB565(color);
    const hex = `#${quantized.toString(16).padStart(6, '0')}`;
    this.bufCtx.fillStyle = hex;

    // Draw using pixel font patterns
    const s = Math.max(1, Math.floor(size / 6));
    for (let i = 0; i < text.length; i++) {
      this.drawChar(text[i], x + i * (4 * s), y, s);
    }
  }

  private drawChar(char: string, x: number, y: number, s: number): void {
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
      'A': [0b010, 0b101, 0b111, 0b101, 0b101],
      'B': [0b110, 0b101, 0b110, 0b101, 0b110],
      'C': [0b111, 0b100, 0b100, 0b100, 0b111],
      'D': [0b110, 0b101, 0b101, 0b101, 0b110],
      'E': [0b111, 0b100, 0b110, 0b100, 0b111],
      'F': [0b111, 0b100, 0b110, 0b100, 0b100],
      'G': [0b111, 0b100, 0b101, 0b101, 0b111],
      'H': [0b101, 0b101, 0b111, 0b101, 0b101],
      'I': [0b111, 0b010, 0b010, 0b010, 0b111],
      'L': [0b100, 0b100, 0b100, 0b100, 0b111],
      'M': [0b101, 0b111, 0b111, 0b101, 0b101],
      'N': [0b101, 0b111, 0b111, 0b111, 0b101],
      'O': [0b111, 0b101, 0b101, 0b101, 0b111],
      'P': [0b111, 0b101, 0b111, 0b100, 0b100],
      'R': [0b111, 0b101, 0b111, 0b110, 0b101],
      'S': [0b111, 0b100, 0b111, 0b001, 0b111],
      'T': [0b111, 0b010, 0b010, 0b010, 0b010],
      'U': [0b101, 0b101, 0b101, 0b101, 0b111],
      'V': [0b101, 0b101, 0b101, 0b101, 0b010],
      'W': [0b101, 0b101, 0b111, 0b111, 0b101],
      'Y': [0b101, 0b101, 0b010, 0b010, 0b010],
    };

    const upper = char.toUpperCase();
    const pattern = patterns[upper];
    if (!pattern) {
      this.bufCtx.fillRect(x, y, 3 * s, 5 * s);
      return;
    }

    for (let row = 0; row < 5; row++) {
      for (let col = 0; col < 3; col++) {
        if (pattern[row] & (1 << (2 - col))) {
          this.bufCtx.fillRect(x + col * s, y + row * s, s, s);
        }
      }
    }
  }

  drawRect(x: number, y: number, w: number, h: number, color: number): void {
    const quantized = colorToRGB565(color);
    this.bufCtx.fillStyle = `#${quantized.toString(16).padStart(6, '0')}`;
    this.bufCtx.fillRect(x, y, w, h);
  }

  drawCircle(x: number, y: number, radius: number, color: number): void {
    const quantized = colorToRGB565(color);
    this.bufCtx.fillStyle = `#${quantized.toString(16).padStart(6, '0')}`;
    this.bufCtx.beginPath();
    this.bufCtx.arc(x, y, radius, 0, Math.PI * 2);
    this.bufCtx.fill();
  }

  clear(): void {
    this.bufCtx.fillStyle = '#000820';
    this.bufCtx.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  }

  present(): void {
    // Scale buffer up to display canvas with nearest-neighbor
    this.ctx.imageSmoothingEnabled = false;
    this.ctx.drawImage(this.buffer, 0, 0, DISPLAY_WIDTH * SCALE, DISPLAY_HEIGHT * SCALE);
  }
}
