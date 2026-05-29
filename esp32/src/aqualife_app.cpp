#include "aqualife_app.h"

#include <Preferences.h>

#include "app_runtime.h"

namespace {

constexpr uint32_t PLANT_FRAME_TIME_MS = 800;
constexpr bool PERSISTENCE_ENABLED = true;
constexpr uint32_t AQUARIUM_STATE_VERSION = 1;

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
  float hunger;
  float happiness;
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
  const SpriteSheet* sheet;
  int x;
  int y;
  float offset;
  uint8_t frameOffset;
  uint16_t drawWidth;
  int height;
};

Fish fish[] = {
  { &clownfish_sheet, "clownfish", 35, 48, 0.45f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 24, 16, 80.0f, 70.0f },
  { &whale_sheet, "whale", 120, 42, 0.3f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 48, 27, 80.0f, 70.0f },
  { &hammerhead_sheet, "hammerhead", 78, 62, 0.4f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 36, 20, 80.0f, 70.0f },
};

constexpr int FISH_COUNT = sizeof(fish) / sizeof(fish[0]);
Bubble bubbles[20];
Food food[20];
Plant plants[5];
Preferences preferences;
uint32_t worldTime = 0;
uint16_t bubbleSpawnTimer = 0;
uint16_t nextBubbleSpawn = 800;

void loadAquariumState() {
  if (!PERSISTENCE_ENABLED) {
    return;
  }

  if (!preferences.begin("aqualife", true)) {
    LOG_PRINTLN("NVS: failed to open aquarium state");
    return;
  }

  const uint32_t version = preferences.getUInt("version", 0);
  if (version == AQUARIUM_STATE_VERSION) {
    for (int i = 0; i < FISH_COUNT; i++) {
      char key[8];

      snprintf(key, sizeof(key), "hun%d", i);
      fish[i].hunger = clampPercent(preferences.getFloat(key, fish[i].hunger));

      snprintf(key, sizeof(key), "hap%d", i);
      fish[i].happiness = clampPercent(preferences.getFloat(key, fish[i].happiness));
    }

    LOG_PRINTF("NVS: aquarium state loaded, last event=%s\n", preferences.getString("event", "none").c_str());
  } else {
    LOG_PRINTF("NVS: no compatible aquarium state, version=%lu\n", static_cast<unsigned long>(version));
  }

  preferences.end();
}

void saveAquariumState(const char* eventName) {
  if (!PERSISTENCE_ENABLED) {
    return;
  }

  if (!preferences.begin("aqualife", false)) {
    LOG_PRINTLN("NVS: failed to save aquarium state");
    return;
  }

  preferences.putUInt("version", AQUARIUM_STATE_VERSION);
  preferences.putString("event", eventName);
  preferences.putULong("saved_ms", millis());

  for (int i = 0; i < FISH_COUNT; i++) {
    char key[8];

    snprintf(key, sizeof(key), "hun%d", i);
    preferences.putFloat(key, clampPercent(fish[i].hunger));

    snprintf(key, sizeof(key), "hap%d", i);
    preferences.putFloat(key, clampPercent(fish[i].happiness));
  }

  preferences.end();
  LOG_PRINTF("NVS: aquarium state saved, event=%s\n", eventName);
}

bool hasLiveFood() {
  for (const auto& item : food) {
    if (item.alive) {
      return true;
    }
  }
  return false;
}

void seekFood(Fish& f) {
  Food* nearest = nullptr;
  float minDistSq = 1000000.0f;

  for (auto& item : food) {
    if (!item.alive) {
      continue;
    }
    const float dx = item.x - f.x;
    const float dy = item.y - f.y;
    const float distSq = dx * dx + dy * dy;
    if (distSq < minDistSq) {
      minDistSq = distSq;
      nearest = &item;
    }
  }

  if (nearest == nullptr) {
    return;
  }

  const float dx = nearest->x - f.x;
  const float dy = nearest->y - f.y;
  const float dist = sqrtf(dx * dx + dy * dy);
  if (dist < 8.0f) {
    nearest->alive = false;
    f.hunger = min(100.0f, f.hunger + 25.0f);
    f.happiness = min(100.0f, f.happiness + 10.0f);
    f.vx *= 0.4f;
    f.vy *= 0.4f;
    saveAquariumState("eat");
    return;
  }

  if (dist > 0.0f) {
    f.vx = (dx / dist) * 1.0f;
    f.vy = (dy / dist) * 1.0f;
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
        if (hasLiveFood()) {
          f.state = SeekFood;
        }
        break;

      case Scared:
        f.vx = f.direction * 3.5f;
        f.vy = randFloat(-1.0f, 1.0f);
        f.state = Hidden;
        f.visible = false;
        f.hideTimer = 3000 + static_cast<uint16_t>(esp_random() % 3000);
        LOG_PRINTF("%s hidden for %u ms\n", f.name, f.hideTimer);
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
          LOG_PRINTF("%s returned\\n", f.name);
        }
        break;

      case SeekFood:
        seekFood(f);
        if (!hasLiveFood()) {
          f.state = Idle;
        }
        break;
      case Play:
      case Sleep:
        f.state = Idle;
        break;
    }
  }

  if (!f.visible) {
    return;
  }

  f.hunger = max(0.0f, f.hunger - 0.001f * dt);
  f.happiness = max(0.0f, f.happiness - 0.0005f * dt);

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

void drawHungerBar(float hunger) {
  constexpr int x = 5;
  constexpr int y = AQUARIUM_BOTTOM + 4;
  drawPixelText("HGR", x, y, rgb(170, 170, 170));
  framebuffer.fillRect(x + 20, y, 40, 5, rgb(51, 51, 51));
  const int fillWidth = static_cast<int>((hunger / 100.0f) * 38.0f);
  const uint16_t color = hunger > 50.0f ? rgb(0, 204, 0) : hunger > 25.0f ? rgb(204, 204, 0) : rgb(204, 0, 0);
  framebuffer.fillRect(x + 21, y + 1, fillWidth, 3, color);
}

void drawImuDebug() {
  if (!IMU_DEBUG_ENABLED) {
    return;
  }

  char status[48];
  snprintf(status, sizeof(status), "IMU %s %s", imuEnabled ? "ON" : "OFF", imuUpdated ? "UPD" : "WAIT");
  drawPixelText(status, 4, 13, imuEnabled ? rgb(90, 255, 150) : rgb(255, 82, 82));

  char values[64];
  snprintf(values, sizeof(values), "A %.1f %.1f %.1f S %.1f T %lu", imuAccelX, imuAccelY, imuAccelZ, imuShakeStrength,
           static_cast<unsigned long>(shakeDuration / 1000));
  drawPixelText(values, 4, 22, rgb(210, 210, 210));
}

void setupPlants() {
  for (int i = 0; i < 5; i++) {
    const bool amazonSword = i % 2 == 0;
    plants[i] = {
      amazonSword ? &amazon_sword_sheet : &cabomba_sheet,
      8 + i * 50,
      AQUARIUM_BOTTOM,
      randFloat(0, 6.28318f),
      static_cast<uint8_t>(i),
      static_cast<uint16_t>(amazonSword ? 30 : 26),
      static_cast<int>(28 + (esp_random() % 5)),
    };
  }
}

}  // namespace

int aquaLifeFishCount() {
  return FISH_COUNT;
}

const char* aquaLifeFishName(int index) {
  return fish[index].name;
}

float aquaLifeFishHunger(int index) {
  return fish[index].hunger;
}

float aquaLifeFishHappiness(int index) {
  return fish[index].happiness;
}

void setupAquaLifeApp() {
  setupPlants();
  loadAquariumState();
}

void feedAquaLifeApp() {
  for (auto& item : food) {
    if (!item.alive) {
      item.x = randFloat(AQUARIUM_LEFT + 8, AQUARIUM_RIGHT - 8);
      item.y = AQUARIUM_TOP + 4;
      item.vy = 0.3f;
      item.alive = true;
      for (auto& f : fish) {
        if (f.visible && f.state != Scared && f.state != Hidden) {
          f.state = SeekFood;
          f.thinkTimer = 0;
        }
      }
      LOG_PRINTLN("BTN_A: Feed");
      saveAquariumState("feed");
      return;
    }
  }
}

void scareAquaLifeFish() {
  LOG_PRINTLN("IMU: Shake detected - fish scared");
  for (auto& f : fish) {
    if (!f.visible) {
      continue;
    }
    f.state = Scared;
    f.thinkTimer = 0;
    f.direction = randFloat(0, 1) > 0.5f ? 1 : -1;
    LOG_PRINTF("%s scared\n", f.name);
  }
}

void updateAquaLifeApp(uint16_t dt) {
  for (auto& f : fish) {
    updateFish(f, dt);
  }
  updateBubbles(dt);
  updateFood(dt);
  worldTime += dt;
}

void renderAquaLifeApp() {
  framebuffer.fillScreen(rgb(0, 8, 32));

  for (int y = 0; y < DISPLAY_HEIGHT; y += 16) {
    const uint8_t shade = 8 + (y * 18) / DISPLAY_HEIGHT;
    framebuffer.fillRect(0, y, DISPLAY_WIDTH, 16, rgb(0, shade, 36));
  }

  framebuffer.fillRect(0, AQUARIUM_BOTTOM - 3, DISPLAY_WIDTH, 6, rgb(136, 102, 34));

  for (const auto& plant : plants) {
    const uint8_t frame = (worldTime / PLANT_FRAME_TIME_MS + plant.frameOffset) % plant.sheet->frames;
    const int x = roundf(plant.x + sinf(worldTime * 0.0015f + plant.offset) * 1.5f);
    drawSpriteMasked(*plant.sheet, frame, x, plant.y - plant.height, plant.drawWidth, plant.height, false);
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

  float avgHunger = 0.0f;
  for (const auto& f : fish) {
    avgHunger += f.hunger;
  }
  drawHungerBar(avgHunger / FISH_COUNT);
  drawPixelText("AquaLife", 4, 3, rgb(210, 210, 210));
  drawImuDebug();
  drawBatteryStatus();

  framebuffer.pushSprite(0, 0);
}
