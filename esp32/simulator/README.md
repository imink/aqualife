# AquaLife macOS Simulator

This is a small SDL2 desktop simulator for the M5StickS3 UI. It follows the same idea as `m5stack/lv_m5_emulator`: render the embedded display into a native macOS window and mock hardware inputs while iterating on UI.

## Setup

```sh
brew install sdl2
cmake -S simulator -B simulator/build
cmake --build simulator/build
./simulator/build/aqualife_simulator
```

## Controls

- `B`: switch app (`AquaLife` / `VFD Time` / `Fish Status` / `Device Info`)
- `A` on `AquaLife`: feed
- `A` on `VFD Time`: single click opens the tomato time-slot selector; single click again starts it
- Double-click `A` on `VFD Time`: clear the tomato timer and return to the real clock
- `Left` / `Right`: mock IMU tilt for the VFD tomato selector, choosing smaller/larger timer slots
- `S`: shake/ scare fish
- `C`: toggle charging
- `Up` / `Down`: change mocked battery level
- `1` - `6`: change window scale
- `Esc` or window close: quit

The simulator intentionally mocks M5StickS3-only APIs such as buttons, IMU, power, and the LCD driver. It is for UI and behavior iteration; final hardware-specific behavior still needs testing on the device.
