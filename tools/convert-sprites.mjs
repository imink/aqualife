import { createCanvas, loadImage } from 'canvas';
import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const root = resolve(__dirname, '..');

const sprites = [
  {
    id: 'clownfish',
    path: resolve(root, 'public/assets/clownfish_sprite_sheet.png'),
    frameWidth: 32,
    frameHeight: 32,
    frames: 4,
  },
  {
    id: 'whale',
    path: resolve(root, 'public/assets/whale_sheet.png'),
    frameWidth: 32,
    frameHeight: 32,
    frames: 4,
  },
  {
    id: 'hammerhead',
    path: resolve(root, 'public/assets/hammerhead_shark_sprite.png'),
    frameWidth: 32,
    frameHeight: 32,
    frames: 4,
  },
];

function rgb888To565(r, g, b) {
  return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

function formatArray(values, radix = 16) {
  const lines = [];
  for (let i = 0; i < values.length; i += 12) {
    const chunk = values.slice(i, i + 12);
    lines.push(
      `  ${chunk
        .map((value) =>
          radix === 16 ? `0x${value.toString(16).padStart(4, '0')}` : `${value}`,
        )
        .join(', ')}`,
    );
  }
  return lines.join(',\n');
}

async function convertSprite(sprite) {
  const image = await loadImage(sprite.path);
  const expectedWidth = sprite.frameWidth * sprite.frames;
  if (image.width !== expectedWidth || image.height !== sprite.frameHeight) {
    throw new Error(
      `${sprite.id}: expected ${expectedWidth}x${sprite.frameHeight}, got ${image.width}x${image.height}`,
    );
  }

  const canvas = createCanvas(image.width, image.height);
  const ctx = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(image, 0, 0);

  const imageData = ctx.getImageData(0, 0, image.width, image.height);
  const pixels = [];
  const alpha = [];

  for (let y = 0; y < image.height; y++) {
    for (let x = 0; x < image.width; x++) {
      const index = (y * image.width + x) * 4;
      const r = imageData.data[index];
      const g = imageData.data[index + 1];
      const b = imageData.data[index + 2];
      const a = imageData.data[index + 3];
      pixels.push(rgb888To565(r, g, b));
      alpha.push(a > 16 ? 1 : 0);
    }
  }

  return { ...sprite, pixels, alpha };
}

const converted = await Promise.all(sprites.map(convertSprite));

const header = `#pragma once

#include <Arduino.h>

struct SpriteSheet {
  const uint16_t* pixels;
  const uint8_t* alpha;
  uint8_t frameWidth;
  uint8_t frameHeight;
  uint8_t frames;
};

constexpr uint8_t SPRITE_FRAME_SIZE = 32;

${converted
  .map(
    (sprite) => `constexpr uint8_t ${sprite.id.toUpperCase()}_FRAME_WIDTH = ${sprite.frameWidth};
constexpr uint8_t ${sprite.id.toUpperCase()}_FRAME_HEIGHT = ${sprite.frameHeight};
constexpr uint8_t ${sprite.id.toUpperCase()}_FRAMES = ${sprite.frames};

const uint16_t ${sprite.id}_pixels[] PROGMEM = {
${formatArray(sprite.pixels)}
};

const uint8_t ${sprite.id}_alpha[] PROGMEM = {
${formatArray(sprite.alpha, 10)}
};

constexpr SpriteSheet ${sprite.id}_sheet = {
  ${sprite.id}_pixels,
  ${sprite.id}_alpha,
  ${sprite.id.toUpperCase()}_FRAME_WIDTH,
  ${sprite.id.toUpperCase()}_FRAME_HEIGHT,
  ${sprite.id.toUpperCase()}_FRAMES,
};`,
  )
  .join('\n\n')}
`;

const outPath = resolve(root, 'esp32/include/sprites.h');
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, header);
console.log(`Wrote ${outPath}`);
