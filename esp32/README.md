# AquaLife ESP32 / M5StickS3 Firmware

This folder contains the hardware firmware version of the AquaLife aquarium.

For Chinese documentation, see [README.zh-CN.md](README.zh-CN.md).

## Requirements

- M5StickS3 / ESP32-S3 device
- PlatformIO
- USB cable

## VS Code workspace

Open [../aqualife.code-workspace](../aqualife.code-workspace) in VS Code when developing the full project.

The workspace includes both the repository root and [esp32](.) as separate folders. This lets the web app stay available while PlatformIO detects [platformio.ini](platformio.ini) as a top-level workspace project.

If you open only the repository root folder, PlatformIO may not automatically detect the nested ESP32 project.

## Install firmware

These steps are for players who want to install AquaLife on a M5StickS3.

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the PlatformIO extension.
2. Connect the M5StickS3 to your computer with a USB cable.
3. Open this repository in VS Code.
4. Open the PlatformIO panel.
5. Select `m5sticks3 > General > Upload` to flash the firmware.
6. Select `m5sticks3 > General > Upload and Monitor` if you also want to watch serial logs after flashing.

If the upload port changes after flashing, unplug and reconnect the device, then run `Upload and Monitor` again.

## Download firmware releases

Released M5StickS3 firmware is published as GitHub Release assets.

1. Open the repository on GitHub.
2. Select the `Releases` page.
3. Open the latest release, such as `v1.0.0`.
4. Download the `aqualife-m5sticks3-<version>-...` assets.

Each release contains:

- `aqualife-m5sticks3-<version>-firmware.bin`
- `aqualife-m5sticks3-<version>-firmware.elf`
- `aqualife-m5sticks3-<version>-bootloader.bin`
- `aqualife-m5sticks3-<version>-partitions.bin`
- `aqualife-m5sticks3-<version>-build_info.h`
- `SHA256SUMS.txt`

Use PlatformIO or another ESP32 flashing tool to install the downloaded binaries.

## Play

- Button A: feed the fish.
- Button B: switch to the next app screen.
- Shake the device briefly: scare the fish; they hide for a few seconds, then return.
- After several seconds without interaction, the display dims to save power.

The aquarium keeps core fish status in device storage, so hunger and happiness survive a restart. Temporary states such as scared or hidden are not restored after reboot.

## Firmware info

The firmware prints its version, git commit, and UTC build time at boot, for example:

```txt
AquaLife ESP32 firmware v1.0.0 (abc1234)
Build time: 2026-05-27T00:00:00Z
```

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

GitHub Releases contain the published firmware binaries. Creating a `v*` tag, such as `v1.0.0`, runs the release workflow and uploads the firmware assets to that release.

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
- Button B: switch to the next app screen
- Shake: fish dart away, hide, then return one by one
- State persistence: fish hunger and happiness are saved to NVS on key events
- Rendering: RGB565
