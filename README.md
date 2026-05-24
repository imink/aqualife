# AquaLife - M5StickS3 Aquarium Pet Simulator

A virtual aquarium pet application that simulates an M5StickS3 ESP32-S3 device running on macOS.

## Architecture

```
src/
├── core/          # Pure logic (no Pixi imports)
│   ├── Fish.ts        # Fish entity + serialization
│   ├── Bubble.ts      # Bubble entity
│   ├── Pet.ts         # Pet stats system
│   ├── World.ts       # World state container
│   └── StateMachine.ts # Generic state machine
├── platform/      # Hardware abstraction
│   ├── interfaces/    # Abstract interfaces (Display, Buttons, IMU, Audio)
│   ├── mac/           # macOS simulator implementations
│   └── stick/         # Future M5StickS3 implementations
├── renderer/      # PixiJS renderer (only module allowed to import Pixi)
├── systems/       # Game systems (no Pixi imports)
│   ├── FishSystem.ts  # Fish AI + movement (5Hz decision, per-frame movement)
│   ├── BubbleSystem.ts # Bubble spawning + physics
│   ├── SaveSystem.ts  # Auto-save every 30s to localStorage
│   └── AudioSystem.ts # Sound effect triggers
├── ui/            # UI drawing helpers
│   ├── BatteryBar.ts
│   ├── HungerBar.ts
│   └── StatusBar.ts
└── app/
    └── main.ts    # Application bootstrap + game loop
esp32/
├── platformio.ini # PlatformIO firmware config
├── include/
│   └── sprites.h  # Generated RGB565 sprite data
└── src/
    └── main.cpp   # M5StickS3 firmware
```

## Device Simulation

- **Display**: 240×135 landscape internal resolution, scaled 3× to 720×405
- **Colors**: RGB565 quantization (simulates real LCD)
- **Frame Rate**: Locked to 25 FPS (no requestAnimationFrame)
- **Sprites**: 16×16 max, 8 animation frames max
- **Rendering**: Nearest-neighbor, no anti-aliasing

## Controls

| Key | M5 Button | Action |
|-----|-----------|--------|
| A | Button A | Feed fish |
| S | Button B | Play with fish |
| Space | A+B | Feed + Play |
| Mouse drag | IMU | Tilt simulation (scare fish) |

## Running

```bash
npm install
npm run dev
```

Open http://localhost:3000

## Building

```bash
npm run build
```

## ESP32 / M5StickS3 firmware

Generate sprite data:

```bash
npm run esp32:assets
```

Build with PlatformIO:

```bash
npm run esp32:build
```

Upload to device:

```bash
npm run esp32:upload
```

Monitor logs:

```bash
npm run esp32:monitor
```

The ESP32 version lives in `esp32/` and uses the same fish sprite rules:

- 4 horizontal frames
- each frame `32x32`
- total `128x32`
- transparent PNG
- converted to RGB565 + alpha mask in `esp32/include/sprites.h`

Use `public/assets/` as the single canonical sprite folder. The Web simulator loads files from there at `/assets/...`, and the ESP32 converter reads the same files to generate `esp32/include/sprites.h`.

## Constraints

- `core/` and `systems/` NEVER import PixiJS directly
- All rendering goes through the `IDisplay` interface
- Architecture supports future migration to real M5StickS3 hardware
