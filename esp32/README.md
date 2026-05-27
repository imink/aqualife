# AquaLife ESP32 / M5StickS3 Firmware

This folder contains the hardware firmware version of the AquaLife aquarium.

## Requirements

- M5StickS3 / ESP32-S3 device
- PlatformIO
- USB cable

## Build assets

From the project root:

```bash
npm run esp32:assets
```

This converts PNG sprite sheets in `../public/assets/` into `esp32/include/sprites.h` as RGB565 pixel data plus alpha masks.

In the VS Code PlatformIO panel, this is also available as the custom task:

```txt
m5sticks3 > Custom > Build Assets
```

`Build` and `Upload` also run asset conversion automatically before compiling/uploading.

## Build firmware

```bash
npm run esp32:build
```

## Upload firmware

```bash
npm run esp32:upload
```

## Monitor serial logs

```bash
npm run esp32:monitor
```

## Sprite format

Use the same project sprite rules:

- horizontal sprite sheet
- 4 frames
- each frame `32x32`
- total `128x32`
- transparent PNG
- fish facing right
- tail/fins animate, body mostly stable

The converter currently includes:

- `public/assets/clownfish_sprite_sheet.png`
- `public/assets/whale_sheet.png`
- `public/assets/hammerhead_shark_sprite.png`
- `public/assets/amazon_sword_sprite_sheet.png`
- `public/assets/cabomba_sprite_sheet.png`

## Runtime behavior

- Display: `240x135` landscape
- FPS: fixed `25`
- Rendering uses a 16-bit offscreen framebuffer (`M5Canvas`) and pushes one complete frame to the LCD to avoid flicker
- Animation: 4-frame sprite sheets
- Button A: feed
- Button B: play
- Shake: fish dart away, hide, then return one by one
- Rendering: RGB565
