#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include "sprites.h"

constexpr int DISPLAY_WIDTH = 240;
constexpr int DISPLAY_HEIGHT = 135;
constexpr int ACTIVE_FPS = 25;
constexpr int IDLE_FPS = 12;
constexpr int ACTIVE_FRAME_TIME_MS = 1000 / ACTIVE_FPS;
constexpr int IDLE_FRAME_TIME_MS = 1000 / IDLE_FPS;
constexpr uint32_t ACTIVE_DISPLAY_MS = 8000;
constexpr uint32_t IMU_SAMPLE_MS = 150;
constexpr uint8_t ACTIVE_BRIGHTNESS = 220;
constexpr uint8_t IDLE_BRIGHTNESS = 90;

constexpr int AQUARIUM_TOP = 12;
constexpr int AQUARIUM_BOTTOM = DISPLAY_HEIGHT - 20;
constexpr int AQUARIUM_LEFT = 2;
constexpr int AQUARIUM_RIGHT = DISPLAY_WIDTH - 2;

enum FishState : uint8_t {
  Idle,
  SeekFood,
  Sleep,
  Scared,
  Play,
  Hidden,
};

struct Fish {
  const SpriteSheet* sheet;
  const char* name;
  float x;
  float y;
  float vx;
  float vy;
  int8_t direction;
  uint8_t animationFrame;
  uint16_t animationTimer;
  uint16_t thinkTimer;
  uint16_t hideTimer;
  FishState state;
  bool visible;
  uint16_t drawWidth;
  uint16_t drawHeight;
};

struct Bubble {
  float x;
  float y;
  float velocity;
  float drift;
  bool alive;
};

struct Food {
  float x;
  float y;
  float vy;
  bool alive;
};

struct Plant {
  int x;
  int y;
  float offset;
  int height;
};

Fish fish[] = {
  { &clownfish_sheet, "clownfish", 35, 48, 0.45f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 24, 16 },
  { &whale_sheet, "whale", 120, 42, 0.3f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 48, 27 },
  { &hammerhead_sheet, "hammerhead", 78, 62, 0.4f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 36, 20 },
};

constexpr int FISH_COUNT = sizeof(fish) / sizeof(fish[0]);
Bubble bubbles[20];
Food food[20];
Plant plants[5];
M5Canvas framebuffer(&M5.Display);

uint32_t lastFrame = 0;
uint32_t worldTime = 0;
uint16_t bubbleSpawnTimer = 0;
uint16_t nextBubbleSpawn = 800;
uint32_t lastBatteryRead = 0;
uint32_t lastInteractionTime = 0;
uint32_t lastImuRead = 0;
int32_t batteryLevel = -1;
uint8_t currentBrightness = ACTIVE_BRIGHTNESS;
m5::Power_Class::is_charging_t batteryChargeState = m5::Power_Class::charge_unknown;

void disableWireless() {
  WiFi.mode(WIFI_OFF);
  btStop();
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

float randFloat(float min, float max) {
  return min + (static_cast<float>(esp_random() % 10000) / 10000.0f) * (max - min);
}

bool isActiveDisplay(uint32_t now) {
  return now - lastInteractionTime < ACTIVE_DISPLAY_MS;
}

int currentFrameTimeMs(uint32_t now) {
  return isActiveDisplay(now) ? ACTIVE_FRAME_TIME_MS : IDLE_FRAME_TIME_MS;
}

void markInteraction() {
  lastInteractionTime = millis();
  if (currentBrightness != ACTIVE_BRIGHTNESS) {
    currentBrightness = ACTIVE_BRIGHTNESS;
    M5.Display.setBrightness(currentBrightness);
  }
}

void updateDisplayBrightness(uint32_t now) {
  const uint8_t targetBrightness = isActiveDisplay(now) ? ACTIVE_BRIGHTNESS : IDLE_BRIGHTNESS;
  if (currentBrightness == targetBrightness) {
    return;
  }

  if (currentBrightness < targetBrightness) {
    currentBrightness = targetBrightness;
  } else {
    constexpr uint8_t step = 2;
    currentBrightness = currentBrightness > targetBrightness + step ? currentBrightness - step : targetBrightness;
  }

  M5.Display.setBrightness(currentBrightness);
}

void drawSpriteMasked(const SpriteSheet& sheet, uint8_t frame, int x, int y, int w, int h, bool flipX) {
  const int srcX = (frame % sheet.frames) * sheet.frameWidth;

  for (int dy = 0; dy < h; dy++) {
    const int sy = (dy * sheet.frameHeight) / h;
    for (int dx = 0; dx < w; dx++) {
      const int sx = (dx * sheet.frameWidth) / w;
      const int drawX = flipX ? (x + w - 1 - dx) : (x + dx);
      const int drawY = y + dy;

      if (drawX < 0 || drawY < 0 || drawX >= DISPLAY_WIDTH || drawY >= DISPLAY_HEIGHT) {
        continue;
      }

      const int sourceIndex = sy * (sheet.frameWidth * sheet.frames) + srcX + sx;
      if (pgm_read_byte(&sheet.alpha[sourceIndex]) == 0) {
        continue;
      }

      const uint16_t color = pgm_read_word(&sheet.pixels[sourceIndex]);
      framebuffer.drawPixel(drawX, drawY, color);
    }
  }
}

void drawPixelText(const char* text, int x, int y, uint16_t color) {
  framebuffer.setTextColor(color);
  framebuffer.setTextSize(1);
  framebuffer.setCursor(x, y);
  framebuffer.print(text);
}

void updateBatteryStatus() {
  const uint32_t now = millis();
  if (now - lastBatteryRead < 1000 && batteryLevel >= 0) {
    return;
  }

  lastBatteryRead = now;
  batteryLevel = M5.Power.getBatteryLevel();
  if (batteryLevel > 100) {
    batteryLevel = 100;
  }
  batteryChargeState = M5.Power.isCharging();
}

void drawBatteryStatus() {
  updateBatteryStatus();

  constexpr int iconWidth = 20;
  constexpr int iconHeight = 9;
  constexpr int capWidth = 2;
  constexpr int rightMargin = 2;
  constexpr int iconX = DISPLAY_WIDTH - iconWidth - capWidth - rightMargin;
  constexpr int iconY = 3;
  const uint16_t outlineColor = rgb(210, 210, 210);
  const uint16_t lowColor = rgb(255, 82, 82);
  const uint16_t chargingColor = rgb(90, 255, 150);
  const uint16_t fillColor = batteryChargeState == m5::Power_Class::is_charging
                               ? chargingColor
                               : batteryLevel <= 20 ? lowColor : rgb(120, 220, 255);

  framebuffer.drawRect(iconX, iconY, iconWidth, iconHeight, outlineColor);
  framebuffer.fillRect(iconX + iconWidth, iconY + 3, capWidth, 3, outlineColor);

  if (batteryLevel >= 0) {
    const int fillWidth = (batteryLevel * (iconWidth - 4)) / 100;
    framebuffer.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4, fillColor);

    char label[6];
    snprintf(label, sizeof(label), "%ld%%", static_cast<long>(batteryLevel));
    drawPixelText(label, iconX - 29, iconY + 1, outlineColor);
  } else {
    drawPixelText("--%", iconX - 23, iconY + 1, lowColor);
  }
}

void spawnFood() {
  for (auto& item : food) {
    if (!item.alive) {
      item.x = randFloat(AQUARIUM_LEFT + 8, AQUARIUM_RIGHT - 8);
      item.y = AQUARIUM_TOP + 4;
      item.vy = 0.3f;
      item.alive = true;
      Serial.println("BTN_A: Feed");
      return;
    }
  }
}

void scareAllFish() {
  Serial.println("IMU: Shake detected - fish scared");
  for (auto& f : fish) {
    if (!f.visible) {
      continue;
    }
    f.state = Scared;
    f.thinkTimer = 0;
    f.direction = randFloat(0, 1) > 0.5f ? 1 : -1;
  }
}

void updateFish(Fish& f, uint16_t dt) {
  f.thinkTimer += dt;

  if (f.thinkTimer >= 200) {
    f.thinkTimer = 0;

    switch (f.state) {
      case Idle:
        if (randFloat(0, 1) < 0.2f) {
          f.vx = randFloat(-0.5f, 0.5f);
          f.vy = randFloat(-0.2f, 0.2f);
        }
        break;

      case Scared:
        f.vx = f.direction * 3.5f;
        f.vy = randFloat(-1.0f, 1.0f);
        f.state = Hidden;
        f.visible = false;
        f.hideTimer = 3000 + static_cast<uint16_t>(esp_random() % 3000);
        break;

      case Hidden:
        if (f.hideTimer > 200) {
          f.hideTimer -= 200;
        } else {
          const bool fromLeft = randFloat(0, 1) > 0.5f;
          f.x = fromLeft ? AQUARIUM_LEFT : AQUARIUM_RIGHT - f.drawWidth;
          f.y = randFloat(AQUARIUM_TOP + 8, AQUARIUM_BOTTOM - f.drawHeight - 4);
          f.direction = fromLeft ? 1 : -1;
          f.vx = f.direction * 0.35f;
          f.vy = 0.0f;
          f.visible = true;
          f.state = Idle;
          Serial.printf("%s returned\\n", f.name);
        }
        break;

      case Play:
      case SeekFood:
      case Sleep:
        f.state = Idle;
        break;
    }
  }

  if (!f.visible) {
    return;
  }

  f.x += f.vx * (dt / 40.0f);
  f.y += f.vy * (dt / 40.0f);

  if (fabs(f.vx) > 0.05f) {
    f.direction = f.vx > 0 ? 1 : -1;
  }

  if (f.x < AQUARIUM_LEFT) {
    f.x = AQUARIUM_LEFT;
    f.vx = fabs(f.vx);
  }
  if (f.x > AQUARIUM_RIGHT - f.drawWidth) {
    f.x = AQUARIUM_RIGHT - f.drawWidth;
    f.vx = -fabs(f.vx);
  }
  if (f.y < AQUARIUM_TOP) {
    f.y = AQUARIUM_TOP;
    f.vy = fabs(f.vy);
  }
  if (f.y > AQUARIUM_BOTTOM - f.drawHeight) {
    f.y = AQUARIUM_BOTTOM - f.drawHeight;
    f.vy = -fabs(f.vy);
  }

  f.animationTimer += dt;
  if (f.animationTimer >= 150) {
    f.animationTimer = 0;
    f.animationFrame = (f.animationFrame + 1) % 8;
  }
}

void updateBubbles(uint16_t dt) {
  bubbleSpawnTimer += dt;
  if (bubbleSpawnTimer >= nextBubbleSpawn) {
    bubbleSpawnTimer = 0;
    nextBubbleSpawn = 500 + (esp_random() % 1500);

    for (auto& b : bubbles) {
      if (!b.alive) {
        b.x = randFloat(AQUARIUM_LEFT, AQUARIUM_RIGHT);
        b.y = AQUARIUM_BOTTOM - 4;
        b.velocity = randFloat(0.3f, 0.8f);
        b.drift = randFloat(-0.2f, 0.2f);
        b.alive = true;
        break;
      }
    }
  }

  for (auto& b : bubbles) {
    if (!b.alive) {
      continue;
    }
    b.y -= b.velocity * (dt / 40.0f);
    b.x += b.drift * (dt / 40.0f);
    if (b.y < AQUARIUM_TOP) {
      b.alive = false;
    }
  }
}

void updateFood(uint16_t dt) {
  for (auto& item : food) {
    if (!item.alive) {
      continue;
    }
    item.y += item.vy * (dt / 40.0f);
    if (item.y > AQUARIUM_BOTTOM - 5) {
      item.y = AQUARIUM_BOTTOM - 5;
      item.vy = 0;
    }
  }
}

void updateControls() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    markInteraction();
    spawnFood();
  }
  if (M5.BtnB.wasPressed()) {
    markInteraction();
    Serial.println("BTN_B: Play");
    for (auto& f : fish) {
      if (f.visible && f.state == Idle) {
        f.state = Play;
        break;
      }
    }
  }

  const uint32_t now = millis();
  if (now - lastImuRead < IMU_SAMPLE_MS) {
    return;
  }
  lastImuRead = now;

  auto imu = M5.Imu.getImuData();
  const float shake = fabs(imu.accel.x) + fabs(imu.accel.y) + fabs(imu.accel.z - 1.0f);
  if (shake > 2.8f) {
    markInteraction();
    scareAllFish();
  }
}

void updateWorld(uint16_t dt) {
  updateControls();
  for (auto& f : fish) {
    updateFish(f, dt);
  }
  updateBubbles(dt);
  updateFood(dt);
  worldTime += dt;
}

void renderWorld() {
  framebuffer.fillScreen(rgb(0, 8, 32));

  for (int y = 0; y < DISPLAY_HEIGHT; y += 16) {
    const uint8_t shade = 8 + (y * 18) / DISPLAY_HEIGHT;
    framebuffer.fillRect(0, y, DISPLAY_WIDTH, 16, rgb(0, shade, 36));
  }

  framebuffer.fillRect(0, AQUARIUM_BOTTOM - 3, DISPLAY_WIDTH, 6, rgb(136, 102, 34));

  for (const auto& plant : plants) {
    const float sway = sinf(worldTime * 0.0015f + plant.offset) * 6.0f;
    const float tipX = plant.x + sway;
    for (int i = 0; i < plant.height; i += 3) {
      const float t = static_cast<float>(i) / plant.height;
      const float bend = t * t;
      const int px = roundf(plant.x + (tipX - plant.x) * bend);
      const int py = plant.y - i;
      framebuffer.fillRect(px, py, 2, 3, rgb(0, 80 + static_cast<uint8_t>(t * 70), 0));
    }
  }

  for (const auto& b : bubbles) {
    if (b.alive) {
      framebuffer.fillCircle(static_cast<int>(b.x), static_cast<int>(b.y), 2, rgb(136, 204, 255));
    }
  }

  for (const auto& item : food) {
    if (item.alive) {
      framebuffer.fillRect(static_cast<int>(item.x), static_cast<int>(item.y), 2, 2, rgb(255, 170, 0));
    }
  }

  for (const auto& f : fish) {
    if (!f.visible) {
      continue;
    }
    const uint8_t frame = (f.animationFrame / 2) % f.sheet->frames;
    drawSpriteMasked(*f.sheet, frame, static_cast<int>(f.x), static_cast<int>(f.y), f.drawWidth, f.drawHeight, f.direction < 0);
  }

  drawPixelText("A FEED", 5, DISPLAY_HEIGHT - 10, rgb(110, 110, 150));
  drawPixelText("B PLAY", DISPLAY_WIDTH - 46, DISPLAY_HEIGHT - 10, rgb(110, 110, 150));
  drawPixelText("AquaLife", 4, 3, rgb(210, 210, 210));
  drawBatteryStatus();

  framebuffer.pushSprite(0, 0);
}

void setupPlants() {
  for (int i = 0; i < 5; i++) {
    plants[i] = {
      15 + i * 55,
      AQUARIUM_BOTTOM,
      randFloat(0, 6.28318f),
      static_cast<int>(12 + (esp_random() % 16)),
    };
  }
}

void setup() {
  disableWireless();

  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);

  M5.Display.setRotation(1);
  M5.Display.setColorDepth(16);
  M5.Display.setTextFont(1);
  M5.Display.setBrightness(currentBrightness);
  M5.Display.fillScreen(TFT_BLACK);

  framebuffer.setColorDepth(16);
  if (!framebuffer.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
    Serial.println("Failed to allocate framebuffer");
    while (true) {
      delay(1000);
    }
  }
  framebuffer.setTextFont(1);

  setupPlants();
  lastFrame = millis();
  lastInteractionTime = lastFrame;
  lastImuRead = lastFrame;

  Serial.println("AquaLife ESP32 firmware");
  Serial.println("Display: 240x135 landscape");
  Serial.println("FPS: 25 active / 12 idle");
}

void loop() {
  const uint32_t now = millis();
  updateDisplayBrightness(now);

  if (now - lastFrame < static_cast<uint32_t>(currentFrameTimeMs(now))) {
    delay(1);
    return;
  }

  const uint16_t dt = now - lastFrame;
  lastFrame = now;

  updateWorld(dt);
  renderWorld();
}
