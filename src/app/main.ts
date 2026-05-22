import { DisplaySimulator } from '../platform/mac/DisplaySimulator';
import { ButtonSimulator } from '../platform/mac/ButtonSimulator';
import { IMUSimulator } from '../platform/mac/IMUSimulator';
import { AudioSimulator } from '../platform/mac/AudioSimulator';
import { AudioSystem } from '../systems/AudioSystem';
import { World, createWorld, DISPLAY_WIDTH, DISPLAY_HEIGHT, AQUARIUM_BOTTOM, AQUARIUM_LEFT, AQUARIUM_RIGHT, FoodParticle } from '../core/World';
import { updateFishSystem } from '../systems/FishSystem';
import { updateBubbleSystem } from '../systems/BubbleSystem';
import { updateSaveSystem, loadWorld, saveWorld } from '../systems/SaveSystem';
import { drawBatteryBar } from '../ui/BatteryBar';
import { drawHungerBar } from '../ui/HungerBar';
import { drawStatusBar, drawMoodIndicator, drawButtonHints } from '../ui/StatusBar';
import { IDisplay } from '../platform/interfaces/Display';
import { createFish } from '../core/Fish';

// Constants matching M5StickS3
const FPS = 25;
const FRAME_TIME = 1000 / FPS;
const MAX_FOOD = 50;

export class App {
  private display!: DisplaySimulator;
  private buttons!: ButtonSimulator;
  private imu!: IMUSimulator;
  private audioSystem!: AudioSystem;
  private world!: World;
  private lastTime = 0;
  private accumulator = 0;
  private running = false;
  private consoleEl!: HTMLElement;
  private logs: string[] = [];

  private log(msg: string): void {
    const time = new Date().toLocaleTimeString('en-US', { hour12: false });
    const entry = `[${time}] ${msg}`;
    this.logs.push(entry);
    if (this.logs.length > 200) this.logs.shift();
    if (this.consoleEl) {
      this.consoleEl.textContent = this.logs.join('\n');
      this.consoleEl.scrollTop = this.consoleEl.scrollHeight;
    }
  }

  async start(): Promise<void> {
    // Setup DOM
    const container = document.getElementById('app')!;
    container.innerHTML = '';
    container.style.display = 'flex';
    container.style.justifyContent = 'center';
    container.style.alignItems = 'center';
    container.style.height = '100vh';
    container.style.backgroundColor = '#1a1a2e';
    container.style.gap = '0';

    // Wrapper to center both panels together
    const wrapper = document.createElement('div');
    wrapper.style.display = 'flex';
    wrapper.style.alignItems = 'stretch';
    wrapper.style.gap = '0';
    wrapper.style.maxHeight = '90vh';
    container.appendChild(wrapper);

    // LEFT: Device frame
    const leftPanel = document.createElement('div');
    leftPanel.style.display = 'flex';
    leftPanel.style.alignItems = 'center';
    leftPanel.style.justifyContent = 'center';
    leftPanel.style.padding = '20px';
    leftPanel.style.flexShrink = '0';
    wrapper.appendChild(leftPanel);

    const frame = document.createElement('div');
    frame.style.background = '#222';
    frame.style.borderRadius = '20px';
    frame.style.padding = '15px';
    frame.style.boxShadow = '0 8px 32px rgba(0,0,0,0.5)';
    leftPanel.appendChild(frame);

    // RIGHT: Control + Console panels
    const rightPanel = document.createElement('div');
    rightPanel.style.display = 'flex';
    rightPanel.style.flexDirection = 'column';
    rightPanel.style.flex = '1';
    rightPanel.style.minWidth = '320px';
    rightPanel.style.maxWidth = '480px';
    rightPanel.style.padding = '20px 20px 20px 0';
    rightPanel.style.gap = '12px';
    wrapper.appendChild(rightPanel);

    // Control Panel
    const controlPanel = this.createControlPanel();
    rightPanel.appendChild(controlPanel);

    // Console Panel
    const consolePanel = this.createConsolePanel();
    rightPanel.appendChild(consolePanel);

    // Initialize platform
    this.display = new DisplaySimulator(frame);
    const canvas = this.display.getCanvas();
    this.buttons = new ButtonSimulator();
    this.imu = new IMUSimulator(canvas);
    const audio = new AudioSimulator();
    this.audioSystem = new AudioSystem(audio);

    // Create world
    this.world = createWorld();

    // Try loading save
    const loaded = loadWorld(this.world);
    this.log(loaded ? 'Save data loaded from localStorage' : 'No save found, starting fresh');
    this.log(`Fish count: ${this.world.fish.length}`);
    for (const f of this.world.fish) {
      this.log(`  🐟 ${f.species} [${f.stateMachine.getState()}] hunger=${f.hunger.toFixed(0)}%`);
    }

    // Load sprite sheets
    await this.display.loadSpriteSheet('clown', '/assets/fish1.jpeg', 256, 256, {
      frameCount: 4,
      removeBackground: true,
    });
    this.log('Sprite loaded: clown (4 frames, 256x256)');
    this.log('---');
    this.log('Ready. Use buttons or keyboard to interact.');

    // Start game loop
    this.running = true;
    this.lastTime = performance.now();
    this.loop();
  }

  private loop = (): void => {
    if (!this.running) return;

    const now = performance.now();
    const delta = now - this.lastTime;
    this.lastTime = now;
    this.accumulator += delta;

    // Fixed timestep game loop at 25 FPS
    while (this.accumulator >= FRAME_TIME) {
      this.update(FRAME_TIME);
      this.accumulator -= FRAME_TIME;
    }

    this.render();

    // Schedule next frame - use setTimeout to enforce FPS cap
    setTimeout(this.loop, FRAME_TIME);
  };

  private update(dt: number): void {
    // Update platform inputs
    this.buttons.update();
    this.imu.update();

    const accel = this.imu.getAccel();

    // Handle button inputs
    if (this.buttons.btnA.wasPressed()) {
      this.feed();
      this.log('KEY_A: Feed');
    }
    if (this.buttons.btnB.wasPressed()) {
      this.play();
      this.log('KEY_S: Play');
    }
    if (this.buttons.space.wasPressed()) {
      this.feed();
      this.play();
      this.log('KEY_SPACE: Feed + Play');
    }

    // Update systems
    updateFishSystem(this.world, dt, accel.x);
    updateBubbleSystem(this.world, dt);
    updateSaveSystem(this.world, dt);

    // Update food particles
    this.updateFood(dt);

    // Update world time
    this.world.time += dt;
  }

  private feed(): void {
    if (this.world.food.length >= MAX_FOOD) return;

    // Drop food from top
    const x = AQUARIUM_LEFT + Math.random() * (AQUARIUM_RIGHT - AQUARIUM_LEFT);
    const food: FoodParticle = {
      x,
      y: 20,
      vy: 0.3,
      alive: true,
    };
    this.world.food.push(food);
    this.audioSystem.playFeed();
  }

  private play(): void {
    // Trigger play state on nearest fish
    for (const fish of this.world.fish) {
      if (fish.stateMachine.getState() === 'Idle') {
        fish.stateMachine.transition('Play');
        fish.happiness = Math.min(100, fish.happiness + 15);
        break;
      }
    }
    this.audioSystem.playInteract();
  }

  private updateFood(dt: number): void {
    for (const food of this.world.food) {
      if (!food.alive) continue;
      food.y += food.vy * (dt / 40);
      if (food.y > AQUARIUM_BOTTOM - 5) {
        food.y = AQUARIUM_BOTTOM - 5;
        food.vy = 0;
      }
    }
    // Remove eaten food
    this.world.food = this.world.food.filter(f => f.alive);
  }

  private render(): void {
    const display = this.display as IDisplay;
    display.clear();

    // Draw background gradient strips
    for (let y = 0; y < DISPLAY_HEIGHT; y += 20) {
      const shade = Math.floor(0x08 + (y / DISPLAY_HEIGHT) * 0x18);
      display.drawRect(0, y, DISPLAY_WIDTH, 20, (shade << 8) | 0x000020);
    }

    // Draw sand bottom
    display.drawRect(0, AQUARIUM_BOTTOM - 3, DISPLAY_WIDTH, 6, 0x886622);

    // Draw plants
    for (const plant of this.world.plants) {
      const angle = Math.sin(this.world.time * 0.001 * 0.03 + plant.offset) * 10;
      const tipX = plant.x + Math.sin(angle * Math.PI / 180) * 5;
      // Draw plant as vertical segments
      for (let i = 0; i < plant.height; i += 3) {
        const t = i / plant.height;
        const px = plant.x + (tipX - plant.x) * t;
        const py = plant.y - i;
        const green = 0x005500 + Math.floor(t * 0x44) * 0x100;
        display.drawRect(px, py, 2, 3, green);
      }
    }

    // Draw bubbles
    for (const bubble of this.world.bubbles) {
      display.drawCircle(bubble.x, bubble.y, bubble.size, 0x88ccff);
    }

    // Draw food
    for (const food of this.world.food) {
      if (food.alive) {
        display.drawRect(food.x, food.y, 2, 2, 0xffaa00);
      }
    }

    // Draw fish
    for (const fish of this.world.fish) {
      if (!fish.visible) continue;

      const flipX = fish.direction < 0;
      // Bob animation
      const bobY = Math.sin(fish.animationFrame * 0.8) * 1;
      const x = Math.floor(fish.x);
      const y = Math.floor(fish.y + bobY);

      // Use sprite sheet if available, otherwise fallback
      if (this.display.hasSpriteSheet(fish.species)) {
        // 4 frames, cycle through based on animationFrame
        const frame = Math.floor(fish.animationFrame / 2) % 4;
        this.display.drawSpriteFrame(fish.species, frame, x, y, 24, 20, flipX);
      } else {
        (display as IDisplay).drawSprite(fish.species, x, y, 20, 14, flipX);
      }
    }

    // Draw UI
    const now = new Date();
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`;
    drawStatusBar(display, timeStr);
    drawBatteryBar(display);

    // Average fish stats for display
    const avgHunger = this.world.fish.reduce((s, f) => s + f.hunger, 0) / (this.world.fish.length || 1);
    const avgHappy = this.world.fish.reduce((s, f) => s + f.happiness, 0) / (this.world.fish.length || 1);

    const uiY = AQUARIUM_BOTTOM + 4;
    drawHungerBar(display, avgHunger, 5, uiY);
    drawMoodIndicator(display, avgHappy, 100, uiY);
    drawButtonHints(display, DISPLAY_HEIGHT - 10);

    display.present();
  }

  private createControlPanel(): HTMLElement {
    const panel = document.createElement('div');
    panel.style.background = '#252535';
    panel.style.borderRadius = '10px';
    panel.style.padding = '16px';
    panel.style.border = '1px solid #333';

    const title = document.createElement('div');
    title.textContent = '🎮 Controls';
    title.style.color = '#aaa';
    title.style.fontSize = '12px';
    title.style.fontFamily = 'monospace';
    title.style.marginBottom = '12px';
    title.style.textTransform = 'uppercase';
    title.style.letterSpacing = '1px';
    panel.appendChild(title);

    // Button row
    const btnRow = document.createElement('div');
    btnRow.style.display = 'flex';
    btnRow.style.gap = '8px';
    btnRow.style.marginBottom = '12px';
    panel.appendChild(btnRow);

    const btnA = this.createButton('A — Feed', () => { this.feed(); this.log('BTN_A: Feed'); });
    const btnB = this.createButton('B — Play', () => { this.play(); this.log('BTN_B: Play'); });
    const btnAB = this.createButton('A+B', () => { this.feed(); this.play(); this.log('BTN_A+B: Feed + Play'); });
    btnRow.appendChild(btnA);
    btnRow.appendChild(btnB);
    btnRow.appendChild(btnAB);

    // IMU section
    const imuTitle = document.createElement('div');
    imuTitle.textContent = 'IMU Simulation';
    imuTitle.style.color = '#888';
    imuTitle.style.fontSize = '11px';
    imuTitle.style.fontFamily = 'monospace';
    imuTitle.style.marginBottom = '8px';
    panel.appendChild(imuTitle);

    const imuRow = document.createElement('div');
    imuRow.style.display = 'flex';
    imuRow.style.gap = '8px';
    imuRow.style.marginBottom = '12px';
    panel.appendChild(imuRow);

    const shakeBtn = this.createButton('🫨 Shake', () => {
      // Simulate a shake - scare all visible fish
      for (const fish of this.world.fish) {
        if (fish.visible) {
          fish.stateMachine.force('Scared');
        }
      }
      this.log('IMU: Shake detected — fish scared!');
    });
    const tiltLBtn = this.createButton('← Tilt L', () => {
      this.log('IMU: Tilt left (accel.x = -1.5)');
    });
    const tiltRBtn = this.createButton('→ Tilt R', () => {
      this.log('IMU: Tilt right (accel.x = 1.5)');
    });
    imuRow.appendChild(tiltLBtn);
    imuRow.appendChild(shakeBtn);
    imuRow.appendChild(tiltRBtn);

    // Info section
    const infoDiv = document.createElement('div');
    infoDiv.style.color = '#666';
    infoDiv.style.fontSize = '10px';
    infoDiv.style.fontFamily = 'monospace';
    infoDiv.style.marginTop = '8px';
    infoDiv.style.lineHeight = '1.6';
    infoDiv.innerHTML = `
      <div style="color:#888;margin-bottom:4px;">Keyboard Shortcuts:</div>
      <div><span style="color:#6cf;">A</span> = Feed &nbsp; <span style="color:#6cf;">S</span> = Play &nbsp; <span style="color:#6cf;">Space</span> = Both</div>
      <div><span style="color:#6cf;">Mouse drag</span> on device = IMU tilt</div>
    `;
    panel.appendChild(infoDiv);

    return panel;
  }

  private createConsolePanel(): HTMLElement {
    const panel = document.createElement('div');
    panel.style.background = '#1a1a24';
    panel.style.borderRadius = '10px';
    panel.style.padding = '12px';
    panel.style.border = '1px solid #333';
    panel.style.flex = '1';
    panel.style.display = 'flex';
    panel.style.flexDirection = 'column';
    panel.style.minHeight = '0';

    const header = document.createElement('div');
    header.style.display = 'flex';
    header.style.justifyContent = 'space-between';
    header.style.alignItems = 'center';
    header.style.marginBottom = '8px';
    panel.appendChild(header);

    const title = document.createElement('div');
    title.textContent = '📋 Console';
    title.style.color = '#aaa';
    title.style.fontSize = '12px';
    title.style.fontFamily = 'monospace';
    title.style.textTransform = 'uppercase';
    title.style.letterSpacing = '1px';
    header.appendChild(title);

    const clearBtn = document.createElement('button');
    clearBtn.textContent = 'Clear';
    clearBtn.style.background = '#333';
    clearBtn.style.border = 'none';
    clearBtn.style.color = '#888';
    clearBtn.style.fontSize = '10px';
    clearBtn.style.padding = '2px 8px';
    clearBtn.style.borderRadius = '4px';
    clearBtn.style.cursor = 'pointer';
    clearBtn.onclick = () => { this.logs = []; this.consoleEl.textContent = ''; };
    header.appendChild(clearBtn);

    this.consoleEl = document.createElement('pre');
    this.consoleEl.style.flex = '1';
    this.consoleEl.style.margin = '0';
    this.consoleEl.style.padding = '8px';
    this.consoleEl.style.background = '#111118';
    this.consoleEl.style.borderRadius = '6px';
    this.consoleEl.style.color = '#4f8';
    this.consoleEl.style.fontSize = '10px';
    this.consoleEl.style.fontFamily = "'JetBrains Mono', 'Fira Code', monospace";
    this.consoleEl.style.overflowY = 'auto';
    this.consoleEl.style.whiteSpace = 'pre-wrap';
    this.consoleEl.style.wordBreak = 'break-all';
    this.consoleEl.style.lineHeight = '1.5';
    panel.appendChild(this.consoleEl);

    this.log('AquaLife M5StickS3 Simulator v1.0');
    this.log('Display: 135×240 @ 3x scale');
    this.log('FPS: 25 (locked)');
    this.log('Color mode: RGB565');
    this.log('---');

    return panel;
  }

  private createButton(label: string, onClick: () => void): HTMLElement {
    const btn = document.createElement('button');
    btn.textContent = label;
    btn.style.background = '#333';
    btn.style.border = '1px solid #555';
    btn.style.color = '#ddd';
    btn.style.fontSize = '11px';
    btn.style.fontFamily = 'monospace';
    btn.style.padding = '6px 12px';
    btn.style.borderRadius = '6px';
    btn.style.cursor = 'pointer';
    btn.style.transition = 'all 0.1s';
    btn.onmousedown = () => { btn.style.background = '#555'; btn.style.transform = 'scale(0.95)'; };
    btn.onmouseup = () => { btn.style.background = '#333'; btn.style.transform = 'scale(1)'; };
    btn.onmouseleave = () => { btn.style.background = '#333'; btn.style.transform = 'scale(1)'; };
    btn.onclick = onClick;
    return btn;
  }

  stop(): void {
    this.running = false;
    saveWorld(this.world);
  }
}
