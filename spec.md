# M5StickS3 Aquarium Pet Simulator

Version: v1.0
Target Platform (Phase 1): macOS
Target Platform (Phase 2): M5StickS3 ESP32-S3
Language: TypeScript
Framework: Vite + PixiJS + TypeScript
Goal: Build a Marine Aquarium–style virtual pet aquarium while preserving rendering behavior, frame rate, sprite constraints, and interaction patterns identical to M5StickS3.

---

# Primary Goal

Create a virtual aquarium application that runs on macOS but behaves like a virtual M5StickS3 device.

The simulator must mimic:

- M5StickS3 screen size
- rendering limitations
- frame rate limitations
- sprite constraints
- color format limitations
- button interaction
- IMU behavior
- audio behavior
- performance limitations

The application should NOT behave like a desktop game.

The application should behave like:

"A M5StickS3 device running a fish pet application"

---

# Technical Stack

Required:

- Vite
- TypeScript
- PixiJS
- Zustand
- ESLint
- Prettier

Optional:

- Howler.js
- localforage

Package versions:

```json
{
    "pixi.js":"latest",
    "zustand":"latest",
    "typescript":"latest"
}
```

---

# Project Structure

```txt
project/

src/

    core/

        Fish.ts
        Bubble.ts
        Pet.ts
        World.ts
        StateMachine.ts

    platform/

        interfaces/

            Display.ts
            Buttons.ts
            Audio.ts
            IMU.ts

        mac/

            DisplaySimulator.ts
            ButtonSimulator.ts
            IMUSimulator.ts
            AudioSimulator.ts

        stick/

            (future)

    renderer/

        PixiRenderer.ts

    assets/

        fish/
        ui/
        background/

    systems/

        FishSystem.ts
        BubbleSystem.ts
        SaveSystem.ts
        AudioSystem.ts

    ui/

        BatteryBar.ts
        HungerBar.ts
        StatusBar.ts

    app/

        main.ts
```

---

# Device Simulation Requirements

Simulate actual M5StickS3 behavior.

---

## Virtual Display

Real device:

```txt
width = 135
height = 240
```

Internal render resolution MUST be:

```ts
const DISPLAY_WIDTH=135
const DISPLAY_HEIGHT=240
```

Desktop scaling:

```ts
scale=3
```

Actual desktop display:

```txt
405 x 720
```

Important:

Disable smoothing.

```ts
imageSmoothingEnabled=false
```

No anti-aliasing.

No texture filtering.

Nearest neighbor only.

---

# Frame Rate Rules

DO NOT use unrestricted rendering.

Forbidden:

```ts
requestAnimationFrame(update)
```

Required:

Logic FPS:

```ts
25 fps
```

Render FPS:

```ts
25 fps
```

Implementation:

```ts
const FPS=25

const FRAME_TIME=1000/FPS
```

Recommended game loop:

```ts
accumulator+=delta

while(accumulator>FRAME_TIME){

    world.update()

    accumulator-=FRAME_TIME
}

renderer.render()
```

---

# Sprite Rules

Must simulate ESP32 limitations.

Maximum sprite sizes:

Fish:

```txt
16x16
24x24
32x32
```

UI:

```txt
8x8
16x16
```

Maximum animation frames:

```txt
8
```

Maximum fish count:

```txt
15
```

Maximum bubble count:

```txt
20
```

Maximum plant count:

```txt
6
```

Maximum particles:

```txt
50
```

---

# Color Simulation

Desktop renderer must simulate RGB565.

Do NOT render full RGBA8888.

Required:

```ts
RGB888

↓

RGB565

↓

RGB888
```

Rendering pipeline:

```txt
Input texture

↓

convert to RGB565

↓

convert back to display format

↓

render
```

Purpose:

simulate real device color quality

---

# Input Mapping

Map keyboard to M5 buttons.

Button A:

```txt
Keyboard: A
```

Button B:

```txt
Keyboard: S
```

Button A+B:

```txt
Space
```

Button interface:

```ts
BtnA.wasPressed()

BtnA.pressed()

BtnA.released()
```

---

# IMU Simulation

M5Stick has accelerometer.

Mouse movement simulates IMU.

Example:

```txt
Mouse drag left

↓

negative X acceleration
```

Mouse drag right:

```txt
positive X acceleration
```

Mouse shake:

```txt
high acceleration spike
```

Interface:

```ts
IMU.getAccel()

returns:

{
    x:number,
    y:number,
    z:number
}
```

---

# Audio Simulation

Use WebAudio.

Interface:

```ts
Speaker.play()

Speaker.tone()

Speaker.stop()
```

Effects:

- bubble pop
- feeding sound
- fish interaction sound

No background music.

---

# Fish Specification

Fish are sprite-based.

No skeletal animation.

Fish structure:

```ts
type Fish={

    id:string

    species:string

    x:number
    y:number

    vx:number
    vy:number

    direction:number

    hunger:number

    happiness:number

    energy:number

    age:number

    animationFrame:number

}
```

---

# Fish Behaviors

Required:

Idle swim

Random direction changes

Bounce from boundaries

Food chasing

Sleep state

Scared state

Interaction state

---

# Fish AI

Update frequency:

```txt
5 Hz
```

NOT every frame.

Fish decision loop:

```txt
Think

↓

Choose action

↓

Execute movement
```

States:

```txt
Idle

SeekFood

Sleep

Scared

Play
```

State machine:

```ts
FishStateMachine
```

---

# Bubble System

Bubble properties:

```ts
{

    x:number
    y:number

    velocity:number

}
```

Behavior:

```txt
Move upward

Random drift

Disappear at top
```

Spawn:

```txt
every 0.5–2 seconds
```

---

# Plant Animation

Plant movement:

```ts
angle=
sin(
time*.03+offset
)*10
```

Do not use physics.

Simple procedural movement only.

---

# Virtual Pet System

Fish must act as pets.

Attributes:

```ts
Hunger

Happiness

Energy

Age

Friendship
```

Behavior:

Not feeding:

```txt
Hunger↓

Happiness↓

```

Feed:

```txt
Hunger↑
Friendship↑
```

Shake device:

```txt
Fish scared
```

---

# Save System

Storage:

```txt
localStorage
```

Save interval:

```txt
30 seconds
```

Save:

```ts
fish state

pet state

settings
```

Auto-load on startup.

---

# UI Layout

Top:

```txt
Battery

Time
```

Middle:

```txt
Aquarium
```

Bottom:

```txt
Hunger

Mood
```

Bottom buttons:

```txt
A Feed

B Play
```

Approximate layout:

```txt
+-------------------+
|Battery Time       |
|                   |
|   🫧              |
|       🐠          |
|           🐟      |
|                   |
| Hunger ████       |
| Mood 😊           |
|                   |
| A Feed   B Play   |
+-------------------+
```

---

# Performance Requirements

Mac simulator must emulate ESP constraints.

CPU target:

```txt
<20%
```

Memory target:

```txt
<100MB
```

Startup:

```txt
<2 sec
```

Stable FPS:

```txt
25 fps
```

No frame drops.

---

# Future Migration Requirements

The following modules must NEVER import Pixi directly:

```txt
core/
systems/
```

Only renderer can import Pixi.

Forbidden:

```ts
import {Sprite} from "pixi.js"
```

inside:

```txt
core/*
```

Allowed:

```txt
renderer/*
```

Reason:

Future M5 migration.

---

# Future ESP32 Rendering Interface

```ts
interface Display{

    drawSprite()

    drawText()

    drawRect()

    clear()

}
```

Desktop implementation:

```ts
DisplaySimulator
```

Future ESP implementation:

```cpp
M5Display
```

---

# Deliverables

Required deliverables:

- complete project source
- README.md
- architecture diagram
- simulator implementation
- placeholder fish sprites
- save/load implementation
- fish state machine
- virtual M5 APIs
- working demo

---

# Definition of Done

Done means:

Application launches on macOS.

Virtual M5 device appears.

Fish swim.

Buttons work.

Food works.

Pet state updates.

Save/load works.

FPS locked to 25.

RGB565 simulation enabled.

No desktop-specific behavior exists.

Architecture allows direct future migration to M5StickS3.
