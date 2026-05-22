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
```

## Device Simulation

- **Display**: 135×240 internal resolution, scaled 3× to 405×720
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

## Constraints

- `core/` and `systems/` NEVER import PixiJS directly
- All rendering goes through the `IDisplay` interface
- Architecture supports future migration to real M5StickS3 hardware
