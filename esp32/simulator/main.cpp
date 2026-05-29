#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*(addr))
#endif

#include "sprites.h"

namespace {

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 135;
constexpr int kActiveFps = 25;
constexpr int kIdleFps = 12;
constexpr int kActiveFrameTimeMs = 1000 / kActiveFps;
constexpr int kIdleFrameTimeMs = 1000 / kIdleFps;
constexpr uint32_t kActiveDisplayMs = 8000;
constexpr uint32_t kPlantFrameTimeMs = 800;
constexpr uint8_t kActiveBrightness = 220;
constexpr uint8_t kIdleBrightness = 3;
constexpr int kAquariumTop = 12;
constexpr int kAquariumBottom = kDisplayHeight - 20;
constexpr int kAquariumLeft = 2;
constexpr int kAquariumRight = kDisplayWidth - 2;
constexpr uint32_t kFocusBlockMs = 25UL * 60UL * 1000UL;
constexpr uint32_t kRelaxBlockMs = 5UL * 60UL * 1000UL;
constexpr uint32_t kDoubleClickMs = 350;
constexpr uint8_t kMatrixColumns = 40;
constexpr uint8_t kMatrixRows = 10;
constexpr uint8_t kMatrixCellWidth = kDisplayWidth / kMatrixColumns;
constexpr uint8_t kMatrixCellHeight = 8;
constexpr uint8_t kMatrixTop = 34;
constexpr uint8_t kTimeMatrixColumns = 39;
constexpr uint8_t kTimeMatrixRows = 7;

enum FirmwareApp : uint8_t {
  AquariumApp,
  VfdTimeApp,
  FishStatusApp,
  DeviceInfoApp,
};

constexpr uint8_t kFirmwareAppCount = 4;

enum PomodoroMode : uint8_t {
  PomodoroIdle,
  PomodoroSelecting,
  PomodoroFocus,
  PomodoroRelax,
  PomodoroComplete,
};

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

class Framebuffer {
 public:
  Framebuffer() : pixels_(kDisplayWidth * kDisplayHeight, 0) {}

  const std::vector<uint16_t>& pixels() const { return pixels_; }

  void fillScreen(uint16_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
  }

  void drawPixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= kDisplayWidth || y >= kDisplayHeight) {
      return;
    }
    pixels_[y * kDisplayWidth + x] = color;
  }

  void fillRect(int x, int y, int width, int height, uint16_t color) {
    for (int py = y; py < y + height; ++py) {
      for (int px = x; px < x + width; ++px) {
        drawPixel(px, py, color);
      }
    }
  }

  void drawRect(int x, int y, int width, int height, uint16_t color) {
    fillRect(x, y, width, 1, color);
    fillRect(x, y + height - 1, width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(x + width - 1, y, 1, height, color);
  }

  void fillCircle(int cx, int cy, int radius, uint16_t color) {
    for (int y = -radius; y <= radius; ++y) {
      for (int x = -radius; x <= radius; ++x) {
        if (x * x + y * y <= radius * radius) {
          drawPixel(cx + x, cy + y, color);
        }
      }
    }
  }

 private:
  std::vector<uint16_t> pixels_;
};

class Simulator {
 public:
  Simulator() {
    fish_ = {{
      { &clownfish_sheet, "clownfish", 35, 48, 0.45f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 24, 16, 80.0f, 70.0f },
      { &whale_sheet, "whale", 120, 42, 0.3f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 48, 27, 80.0f, 70.0f },
      { &hammerhead_sheet, "hammerhead", 78, 62, 0.4f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 36, 20, 80.0f, 70.0f },
    }};
    setupPlants();
  }

  void feed(uint32_t now) {
    markInteraction(now);
    spawnFood();
  }

  void switchToNextApp(uint32_t now) {
    markInteraction(now);
    currentApp_ = static_cast<FirmwareApp>((static_cast<uint8_t>(currentApp_) + 1) % kFirmwareAppCount);
    std::printf("B: Switch app -> %s\n", currentAppName());
  }

  void handleA(uint32_t now) {
    markInteraction(now);
    if (currentApp_ != VfdTimeApp) {
      feed(now);
      return;
    }

    if (pendingVfdSingleClick_ && now - pendingVfdSingleClickAt_ <= kDoubleClickMs) {
      pendingVfdSingleClick_ = false;
      resetPomodoro();
      return;
    }

    pendingVfdSingleClick_ = true;
    pendingVfdSingleClickAt_ = now;
  }

  void tiltTimerSmaller(uint32_t now) {
    markInteraction(now);
    simulatedTiltTomatoes_ = std::max<uint8_t>(1, simulatedTiltTomatoes_ - 1);
    if (pomodoroMode_ == PomodoroSelecting) {
      selectedTomatoes_ = simulatedTiltTomatoes_;
    }
  }

  void tiltTimerLarger(uint32_t now) {
    markInteraction(now);
    simulatedTiltTomatoes_ = std::min<uint8_t>(3, simulatedTiltTomatoes_ + 1);
    if (pomodoroMode_ == PomodoroSelecting) {
      selectedTomatoes_ = simulatedTiltTomatoes_;
    }
  }

  void shake(uint32_t now) {
    markInteraction(now);
    scareAllFish();
  }

  void setBatteryLevel(int level) {
    batteryLevel_ = std::clamp(level, 0, 100);
  }

  void setCharging(bool charging) { charging_ = charging; }

  int batteryLevel() const { return batteryLevel_; }

  bool charging() const { return charging_; }

  uint8_t brightness() const { return currentBrightness_; }

  bool isActiveDisplay(uint32_t now) const { return now - lastInteractionTime_ < kActiveDisplayMs; }

  int currentFrameTimeMs(uint32_t now) const {
    return isActiveDisplay(now) ? kActiveFrameTimeMs : kIdleFrameTimeMs;
  }

  int currentFps(uint32_t now) const { return isActiveDisplay(now) ? kActiveFps : kIdleFps; }

  const char* currentAppName() const {
    switch (currentApp_) {
      case AquariumApp:
        return "AquaLife";
      case VfdTimeApp:
        return "VFD Time";
      case FishStatusApp:
        return "Fish Status";
      case DeviceInfoApp:
        return "Device Info";
    }
    return "Unknown";
  }

  void updateBrightness(uint32_t now) {
    const bool active = isActiveDisplay(now);
    if (displayActive_ != active) {
      displayActive_ = active;
      std::printf("Display %s lightness: %u -> %u\n", active ? "active" : "inactive", currentBrightness_,
                  active ? kActiveBrightness : kIdleBrightness);
    }

    const uint8_t targetBrightness = active ? kActiveBrightness : kIdleBrightness;
    if (currentBrightness_ == targetBrightness) {
      return;
    }

    if (currentBrightness_ < targetBrightness) {
      currentBrightness_ = targetBrightness;
    } else {
      constexpr uint8_t step = 2;
      currentBrightness_ = currentBrightness_ > targetBrightness + step ? currentBrightness_ - step : targetBrightness;
    }
  }

  void update(uint16_t dt, uint32_t now) {
    updateVfdControls(now);
    updatePomodoro(now);

    for (auto& fish : fish_) {
      updateFish(fish, dt);
    }
    updateBubbles(dt);
    updateFood(dt);
    worldTime_ += dt;
  }

  void render(Framebuffer& framebuffer) {
    switch (currentApp_) {
      case AquariumApp:
        renderAquarium(framebuffer);
        break;
      case VfdTimeApp:
        renderVfdTime(framebuffer);
        break;
      case FishStatusApp:
        renderFishStatus(framebuffer);
        break;
      case DeviceInfoApp:
        renderDeviceInfo(framebuffer);
        break;
    }
  }

 private:
  void renderAquarium(Framebuffer& framebuffer) {
    framebuffer.fillScreen(rgb(0, 8, 32));

    for (int y = 0; y < kDisplayHeight; y += 16) {
      const uint8_t shade = 8 + (y * 18) / kDisplayHeight;
      framebuffer.fillRect(0, y, kDisplayWidth, 16, rgb(0, shade, 36));
    }

    framebuffer.fillRect(0, kAquariumBottom - 3, kDisplayWidth, 6, rgb(136, 102, 34));

    for (const auto& plant : plants_) {
      const uint8_t frame = (worldTime_ / kPlantFrameTimeMs + plant.frameOffset) % plant.sheet->frames;
      const int x = std::lround(plant.x + std::sin(worldTime_ * 0.0015f + plant.offset) * 1.5f);
      drawSpriteMasked(framebuffer, *plant.sheet, frame, x, plant.y - plant.height, plant.drawWidth, plant.height, false);
    }

    for (const auto& bubble : bubbles_) {
      if (bubble.alive) {
        framebuffer.fillCircle(static_cast<int>(bubble.x), static_cast<int>(bubble.y), 2, rgb(136, 204, 255));
      }
    }

    for (const auto& item : food_) {
      if (item.alive) {
        framebuffer.fillRect(static_cast<int>(item.x), static_cast<int>(item.y), 2, 2, rgb(255, 170, 0));
      }
    }

    for (const auto& fish : fish_) {
      if (!fish.visible) {
        continue;
      }
      const uint8_t frame = (fish.animationFrame / 2) % fish.sheet->frames;
      drawSpriteMasked(framebuffer, *fish.sheet, frame, static_cast<int>(fish.x), static_cast<int>(fish.y),
                       fish.drawWidth, fish.drawHeight, fish.direction < 0);
    }

    float avgHunger = 0.0f;
    for (const auto& fish : fish_) {
      avgHunger += fish.hunger;
    }
    drawHungerBar(framebuffer, avgHunger / fish_.size());
    drawText(framebuffer, "AquaLife", 4, 3, rgb(210, 210, 210));
    drawBatteryStatus(framebuffer);
  }
  void markInteraction(uint32_t now) {
    lastInteractionTime_ = now;
    if (!displayActive_) {
      displayActive_ = true;
      std::printf("Display active lightness: %u -> %u\n", currentBrightness_, kActiveBrightness);
    }
    currentBrightness_ = kActiveBrightness;
  }

  uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) const {
    return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
  }

  float randomFloat(float min, float max) {
    std::uniform_real_distribution<float> distribution(min, max);
    return distribution(random_);
  }

  uint32_t randomInt(uint32_t max) {
    std::uniform_int_distribution<uint32_t> distribution(0, max - 1);
    return distribution(random_);
  }

  void drawSpriteMasked(Framebuffer& framebuffer, const SpriteSheet& sheet, uint8_t frame, int x, int y, int width,
                        int height, bool flipX) {
    const int sourceX = (frame % sheet.frames) * sheet.frameWidth;

    for (int dy = 0; dy < height; ++dy) {
      const int sy = (dy * sheet.frameHeight) / height;
      for (int dx = 0; dx < width; ++dx) {
        const int sx = (dx * sheet.frameWidth) / width;
        const int drawX = flipX ? (x + width - 1 - dx) : (x + dx);
        const int drawY = y + dy;

        if (drawX < 0 || drawY < 0 || drawX >= kDisplayWidth || drawY >= kDisplayHeight) {
          continue;
        }

        const int sourceIndex = sy * (sheet.frameWidth * sheet.frames) + sourceX + sx;
        if (pgm_read_byte(&sheet.alpha[sourceIndex]) == 0) {
          continue;
        }

        framebuffer.drawPixel(drawX, drawY, pgm_read_word(&sheet.pixels[sourceIndex]));
      }
    }
  }

  void drawText(Framebuffer& framebuffer, const char* text, int x, int y, uint16_t color) {
    int cursorX = x;
    while (*text != '\0') {
      drawChar(framebuffer, *text, cursorX, y, color);
      cursorX += 6;
      ++text;
    }
  }

  void drawChar(Framebuffer& framebuffer, char value, int x, int y, uint16_t color) {
    const uint8_t* glyph = glyphFor(value);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((glyph[row] & (1 << (4 - col))) != 0) {
          framebuffer.drawPixel(x + col, y + row, color);
        }
      }
    }
  }

  const uint8_t* glyphFor(char value) {
    static constexpr uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static constexpr uint8_t percent[7] = {0b11001, 0b11010, 0b00100, 0b01000, 0b10110, 0b00110, 0};
    static constexpr uint8_t digits[10][7] = {
      {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},
      {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
      {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},
      {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110},
      {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},
      {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},
      {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},
      {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},
      {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},
      {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},
    };
    static constexpr uint8_t letters[26][7] = {
      {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},
      {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},
      {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},
      {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},
      {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},
      {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},
      {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110},
      {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},
      {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
      {0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100},
      {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},
      {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},
      {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},
      {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001},
      {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},
      {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},
      {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},
      {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},
      {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},
      {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},
      {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},
      {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},
      {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010},
      {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},
      {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},
      {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},
    };

    if (value >= '0' && value <= '9') {
      return digits[value - '0'];
    }
    if (value >= 'a' && value <= 'z') {
      value = static_cast<char>(value - 'a' + 'A');
    }
    if (value >= 'A' && value <= 'Z') {
      return letters[value - 'A'];
    }
    if (value == '%') {
      return percent;
    }
    return blank;
  }

  void drawBatteryStatus(Framebuffer& framebuffer) {
    constexpr int iconWidth = 20;
    constexpr int iconHeight = 9;
    constexpr int capWidth = 2;
    constexpr int rightMargin = 2;
    constexpr int iconX = kDisplayWidth - iconWidth - capWidth - rightMargin;
    constexpr int iconY = 3;
    const uint16_t outlineColor = rgb(210, 210, 210);
    const uint16_t lowColor = rgb(255, 82, 82);
    const uint16_t chargingColor = rgb(90, 255, 150);
    const uint16_t fillColor = charging_ ? chargingColor : batteryLevel_ <= 20 ? lowColor : rgb(120, 220, 255);

    framebuffer.drawRect(iconX, iconY, iconWidth, iconHeight, outlineColor);
    framebuffer.fillRect(iconX + iconWidth, iconY + 3, capWidth, 3, outlineColor);
    framebuffer.fillRect(iconX + 2, iconY + 2, (batteryLevel_ * (iconWidth - 4)) / 100, iconHeight - 4, fillColor);

    char label[6];
    std::snprintf(label, sizeof(label), "%d%%", batteryLevel_);
    drawText(framebuffer, label, iconX - 29, iconY + 1, outlineColor);
  }

  void drawAppHeader(Framebuffer& framebuffer, const char* title) {
    framebuffer.fillRect(0, 0, kDisplayWidth, 13, rgb(4, 18, 44));
    drawText(framebuffer, title, 4, 3, rgb(230, 240, 255));

    char indexText[8];
    std::snprintf(indexText, sizeof(indexText), "%u/%u", static_cast<unsigned>(static_cast<uint8_t>(currentApp_) + 1),
                  static_cast<unsigned>(kFirmwareAppCount));
    drawText(framebuffer, indexText, kDisplayWidth - 27, 3, rgb(170, 190, 220));
  }

  void drawProgressBar(Framebuffer& framebuffer, int x, int y, int width, int height, float percent, uint16_t color) {
    framebuffer.drawRect(x, y, width, height, rgb(72, 84, 108));
    const int fillWidth = static_cast<int>((std::clamp(percent, 0.0f, 100.0f) / 100.0f) * (width - 2));
    framebuffer.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
  }

  void renderFishStatus(Framebuffer& framebuffer) {
    framebuffer.fillScreen(rgb(8, 12, 24));
    drawAppHeader(framebuffer, "Fish Status");

    constexpr int rowTop = 22;
    constexpr int rowHeight = 33;
    constexpr int barX = 76;
    constexpr int barWidth = 112;

    for (int i = 0; i < static_cast<int>(fish_.size()); i++) {
      const int y = rowTop + i * rowHeight;
      framebuffer.fillRect(4, y - 3, kDisplayWidth - 8, rowHeight - 3, rgb(13, 25, 42));
      drawText(framebuffer, fish_[i].name, 9, y, rgb(230, 240, 255));
      drawText(framebuffer, "Hunger", 9, y + 11, rgb(160, 180, 205));
      drawText(framebuffer, "Happy", 9, y + 21, rgb(160, 180, 205));
      drawProgressBar(framebuffer, barX, y + 10, barWidth, 7, fish_[i].hunger, rgb(96, 220, 130));
      drawProgressBar(framebuffer, barX, y + 20, barWidth, 7, fish_[i].happiness, rgb(255, 198, 82));
    }

    drawBatteryStatus(framebuffer);
  }

  void renderDeviceInfo(Framebuffer& framebuffer) {
    framebuffer.fillScreen(rgb(14, 14, 18));
    drawAppHeader(framebuffer, "Device Info");

    char line[64];
    drawText(framebuffer, "Version sim", 9, 24, rgb(230, 240, 255));
    drawText(framebuffer, "Git desktop", 9, 37, rgb(180, 200, 225));
    std::snprintf(line, sizeof(line), "Battery %d%%", batteryLevel_);
    drawText(framebuffer, line, 9, 50, rgb(180, 200, 225));
    drawText(framebuffer, "IMU keyboard", 9, 63, rgb(90, 255, 150));
    std::snprintf(line, sizeof(line), "Uptime %lu s", static_cast<unsigned long>(SDL_GetTicks() / 1000));
    drawText(framebuffer, line, 9, 76, rgb(180, 200, 225));
    drawText(framebuffer, "B: next app", 9, kDisplayHeight - 15, rgb(130, 150, 180));
    drawBatteryStatus(framebuffer);
  }

  void drawGlowText(Framebuffer& framebuffer, const char* text, int x, int y, uint16_t color) {
    drawText(framebuffer, text, x - 1, y, rgb(3, 58, 56));
    drawText(framebuffer, text, x + 1, y, rgb(3, 58, 56));
    drawText(framebuffer, text, x, y - 1, rgb(3, 58, 56));
    drawText(framebuffer, text, x, y + 1, rgb(3, 58, 56));
    drawText(framebuffer, text, x, y, color);
  }

  void drawVfdPanel(Framebuffer& framebuffer) {
    framebuffer.fillScreen(rgb(1, 8, 13));
    framebuffer.drawRect(2, 2, kDisplayWidth - 4, kDisplayHeight - 4, rgb(8, 54, 62));
    framebuffer.drawRect(4, 4, kDisplayWidth - 8, kDisplayHeight - 8, rgb(1, 22, 31));
    for (int y = 10; y < kDisplayHeight - 8; y += 6) {
      framebuffer.fillRect(6, y, kDisplayWidth - 12, 1, rgb(0, 18, 22));
    }
    framebuffer.fillRect(0, 0, kDisplayWidth, 7, rgb(0, 5, 9));
    framebuffer.fillRect(0, kDisplayHeight - 7, kDisplayWidth, 7, rgb(0, 5, 9));
  }

  void drawMatrixDot(Framebuffer& framebuffer, uint8_t col, uint8_t row, bool active) {
    const int x = col * kMatrixCellWidth + 1;
    const int y = kMatrixTop + row * kMatrixCellHeight;
    if (active) {
      framebuffer.fillRect(x - 1, y - 1, 6, 6, rgb(0, 58, 52));
      framebuffer.fillRect(x, y, 4, 4, rgb(94, 255, 224));
    } else {
      framebuffer.fillRect(x + 1, y + 1, 3, 3, rgb(0, 24, 24));
    }
  }

  void drawMatrixBackground(Framebuffer& framebuffer) {
    for (uint8_t row = 0; row < kMatrixRows; row++) {
      for (uint8_t col = 0; col < kMatrixColumns; col++) {
        drawMatrixDot(framebuffer, col, row, false);
      }
    }
  }

  void drawMatrixDigit(Framebuffer& framebuffer, uint8_t digit, uint8_t gridCol, uint8_t gridRow) {
    constexpr uint8_t patterns[10][7] = {
      {0b11111, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b11111},
      {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},
      {0b11110, 0b00001, 0b00001, 0b11110, 0b10000, 0b10000, 0b11111},
      {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110},
      {0b10010, 0b10010, 0b10010, 0b11111, 0b00010, 0b00010, 0b00010},
      {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110},
      {0b01111, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},
      {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},
      {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},
      {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b11110},
    };

    for (uint8_t row = 0; row < 7; row++) {
      for (uint8_t col = 0; col < 5; col++) {
        drawMatrixDot(framebuffer, gridCol + col, gridRow + row,
                      (patterns[digit][row] & (1 << (4 - col))) != 0);
      }
    }
  }

  void drawMatrixColon(Framebuffer& framebuffer, uint8_t gridCol, uint8_t gridRow) {
    for (uint8_t row = 0; row < 7; row++) {
      drawMatrixDot(framebuffer, gridCol, gridRow + row, row == 2 || row == 4);
    }
  }

  void drawLargeHms(Framebuffer& framebuffer, uint8_t hours, uint8_t minutes, uint8_t seconds) {
    constexpr uint8_t digitWidth = 5;
    constexpr uint8_t colonWidth = 1;
    constexpr uint8_t gap = 1;
    constexpr uint8_t startCol = (kMatrixColumns - kTimeMatrixColumns) / 2;
    constexpr uint8_t startRow = (kMatrixRows - kTimeMatrixRows) / 2;
    const uint8_t values[] = {static_cast<uint8_t>(hours / 10), static_cast<uint8_t>(hours % 10),
                              static_cast<uint8_t>(minutes / 10), static_cast<uint8_t>(minutes % 10),
                              static_cast<uint8_t>(seconds / 10), static_cast<uint8_t>(seconds % 10)};

    drawMatrixBackground(framebuffer);

    uint8_t col = startCol;
    drawMatrixDigit(framebuffer, values[0], col, startRow);
    col += digitWidth + gap;
    drawMatrixDigit(framebuffer, values[1], col, startRow);
    col += digitWidth + gap;
    drawMatrixColon(framebuffer, col, startRow);
    col += colonWidth + gap;
    drawMatrixDigit(framebuffer, values[2], col, startRow);
    col += digitWidth + gap;
    drawMatrixDigit(framebuffer, values[3], col, startRow);
    col += digitWidth + gap;
    drawMatrixColon(framebuffer, col, startRow);
    col += colonWidth + gap;
    drawMatrixDigit(framebuffer, values[4], col, startRow);
    col += digitWidth + gap;
    drawMatrixDigit(framebuffer, values[5], col, startRow);
  }

  uint32_t remainingPhaseMs(uint32_t now) const {
    if (pomodoroMode_ != PomodoroFocus && pomodoroMode_ != PomodoroRelax) {
      return 0;
    }
    const uint32_t elapsed = now - phaseStartedAt_;
    return elapsed >= phaseDurationMs_ ? 0 : phaseDurationMs_ - elapsed;
  }

  void drawLargeDuration(Framebuffer& framebuffer, uint32_t milliseconds) {
    const uint32_t totalSeconds = (milliseconds + 999) / 1000;
    drawLargeHms(framebuffer, static_cast<uint8_t>((totalSeconds / 3600) % 100),
                 static_cast<uint8_t>((totalSeconds / 60) % 60), static_cast<uint8_t>(totalSeconds % 60));
  }

  void drawTomatoSlots(Framebuffer& framebuffer) {
    const int centers[] = {48, 120, 192};
    const char* const labels[] = {"25", "50", "75"};
    for (uint8_t i = 0; i < 3; i++) {
      const bool selected = selectedTomatoes_ == i + 1;
      framebuffer.fillRect(centers[i] - 22, 105, 44, 1, selected ? rgb(94, 255, 224) : rgb(2, 48, 48));
      drawGlowText(framebuffer, labels[i], centers[i] - 8, 108, selected ? rgb(94, 255, 224) : rgb(39, 138, 128));
      drawGlowText(framebuffer, "MIN", centers[i] + 8, 108, rgb(39, 138, 128));
      if (selected) {
        framebuffer.fillRect(centers[i] - 5, 98, 11, 4, rgb(94, 255, 224));
      }
    }
  }

  const char* pomodoroLabel() const {
    switch (pomodoroMode_) {
      case PomodoroSelecting:
        return "SELECT";
      case PomodoroFocus:
        return "FOCUS";
      case PomodoroRelax:
        return "RELAX";
      case PomodoroComplete:
        return "COMPLETE";
      case PomodoroIdle:
        break;
    }
    return "TIMER";
  }

  void renderVfdTime(Framebuffer& framebuffer) {
    const uint32_t now = SDL_GetTicks();
    drawVfdPanel(framebuffer);
    drawGlowText(framebuffer, "STEREO", 10, 13, rgb(39, 138, 128));
    drawGlowText(framebuffer, pomodoroLabel(), 65, 13, pomodoroMode_ == PomodoroIdle ? rgb(39, 138, 128) : rgb(94, 255, 224));
    drawGlowText(framebuffer, "NTP LOCK", 119, 13, rgb(94, 255, 224));

    if (pomodoroMode_ == PomodoroSelecting) {
      drawLargeDuration(framebuffer, selectedTomatoes_ * kFocusBlockMs);
      drawTomatoSlots(framebuffer);
      char label[18];
      std::snprintf(label, sizeof(label), "TILT %u MIN", static_cast<unsigned>(selectedTomatoes_ * 25));
      drawGlowText(framebuffer, label, 10, 116, rgb(39, 138, 128));
      drawGlowText(framebuffer, "A:START", kDisplayWidth - 56, 116, rgb(94, 255, 224));
    } else if (pomodoroMode_ == PomodoroFocus || pomodoroMode_ == PomodoroRelax) {
      drawLargeDuration(framebuffer, remainingPhaseMs(now));
      char label[18];
      std::snprintf(label, sizeof(label), "%u/%u TOMATO", static_cast<unsigned>(completedTomatoes_ + 1),
                    static_cast<unsigned>(selectedTomatoes_));
      drawGlowText(framebuffer, label, 10, 116, rgb(39, 138, 128));
      drawGlowText(framebuffer, "AA:CLR", kDisplayWidth - 50, 116, rgb(39, 138, 128));
    } else if (pomodoroMode_ == PomodoroComplete) {
      drawLargeDuration(framebuffer, 0);
      char label[18];
      std::snprintf(label, sizeof(label), "%u DONE", static_cast<unsigned>(selectedTomatoes_));
      drawGlowText(framebuffer, label, 10, 116, rgb(39, 138, 128));
      drawGlowText(framebuffer, "AA:CLR", kDisplayWidth - 50, 116, rgb(39, 138, 128));
    } else {
      const std::time_t current = std::time(nullptr);
      const std::tm* local = std::localtime(&current);
      if (local != nullptr) {
        drawLargeHms(framebuffer, static_cast<uint8_t>(local->tm_hour), static_cast<uint8_t>(local->tm_min),
                     static_cast<uint8_t>(local->tm_sec));
        char dateText[18];
        std::strftime(dateText, sizeof(dateText), "%b %d %a", local);
        drawGlowText(framebuffer, dateText, 10, 116, rgb(39, 138, 128));
      }
      drawGlowText(framebuffer, "A:SET", kDisplayWidth - 43, 116, rgb(39, 138, 128));
    }

    drawBatteryStatus(framebuffer);
  }

  void handleVfdSingleClick() {
    if (pomodoroMode_ == PomodoroIdle || pomodoroMode_ == PomodoroComplete) {
      selectedTomatoes_ = simulatedTiltTomatoes_;
      pomodoroMode_ = PomodoroSelecting;
      return;
    }
    if (pomodoroMode_ == PomodoroSelecting) {
      completedTomatoes_ = 0;
      startFocusPhase(SDL_GetTicks());
    }
  }

  void updateVfdControls(uint32_t now) {
    if (pendingVfdSingleClick_ && now - pendingVfdSingleClickAt_ > kDoubleClickMs) {
      pendingVfdSingleClick_ = false;
      handleVfdSingleClick();
    }
  }

  void startFocusPhase(uint32_t now) {
    pomodoroMode_ = PomodoroFocus;
    phaseStartedAt_ = now;
    phaseDurationMs_ = kFocusBlockMs;
    std::printf("VFD tomato: focus %u/%u started\n", static_cast<unsigned>(completedTomatoes_ + 1),
                static_cast<unsigned>(selectedTomatoes_));
    std::puts("sound: start chime");
  }

  void startRelaxPhase(uint32_t now) {
    pomodoroMode_ = PomodoroRelax;
    phaseStartedAt_ = now;
    phaseDurationMs_ = kRelaxBlockMs;
    std::puts("VFD tomato: relax started");
    std::puts("sound: relax chime");
  }

  void completePomodoro() {
    pomodoroMode_ = PomodoroComplete;
    phaseStartedAt_ = SDL_GetTicks();
    phaseDurationMs_ = 0;
    std::puts("VFD tomato: complete");
    std::puts("sound: complete chime");
  }

  void resetPomodoro() {
    pomodoroMode_ = PomodoroIdle;
    selectedTomatoes_ = 1;
    completedTomatoes_ = 0;
    phaseStartedAt_ = 0;
    phaseDurationMs_ = 0;
    std::puts("VFD tomato: cleared");
  }

  void updatePomodoro(uint32_t now) {
    if (pomodoroMode_ != PomodoroFocus && pomodoroMode_ != PomodoroRelax) {
      return;
    }
    if (now - phaseStartedAt_ < phaseDurationMs_) {
      return;
    }
    if (pomodoroMode_ == PomodoroFocus) {
      completedTomatoes_++;
      startRelaxPhase(now);
      return;
    }
    if (completedTomatoes_ >= selectedTomatoes_) {
      completePomodoro();
    } else {
      startFocusPhase(now);
    }
  }

  void spawnFood() {
    for (auto& item : food_) {
      if (!item.alive) {
        item.x = randomFloat(kAquariumLeft + 8, kAquariumRight - 8);
        item.y = kAquariumTop + 4;
        item.vy = 0.3f;
        item.alive = true;
        for (auto& fish : fish_) {
          if (fish.visible && fish.state != Scared && fish.state != Hidden) {
            fish.state = SeekFood;
            fish.thinkTimer = 0;
          }
        }
        std::puts("A: Feed");
        return;
      }
    }
  }

  bool hasLiveFood() const {
    for (const auto& item : food_) {
      if (item.alive) {
        return true;
      }
    }
    return false;
  }

  void seekFood(Fish& fish) {
    Food* nearest = nullptr;
    float minDistSq = 1000000.0f;

    for (auto& item : food_) {
      if (!item.alive) {
        continue;
      }
      const float dx = item.x - fish.x;
      const float dy = item.y - fish.y;
      const float distSq = dx * dx + dy * dy;
      if (distSq < minDistSq) {
        minDistSq = distSq;
        nearest = &item;
      }
    }

    if (nearest == nullptr) {
      return;
    }

    const float dx = nearest->x - fish.x;
    const float dy = nearest->y - fish.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 8.0f) {
      nearest->alive = false;
      fish.hunger = std::min(100.0f, fish.hunger + 25.0f);
      fish.happiness = std::min(100.0f, fish.happiness + 10.0f);
      fish.vx *= 0.4f;
      fish.vy *= 0.4f;
      return;
    }

    if (dist > 0.0f) {
      fish.vx = (dx / dist) * 1.0f;
      fish.vy = (dy / dist) * 1.0f;
    }
  }

  void scareAllFish() {
    std::puts("S: Shake detected - fish scared");
    for (auto& fish : fish_) {
      if (!fish.visible) {
        continue;
      }
      fish.state = Scared;
      fish.thinkTimer = 0;
      fish.direction = randomFloat(0, 1) > 0.5f ? 1 : -1;
    }
  }

  void updateFish(Fish& fish, uint16_t dt) {
    fish.thinkTimer += dt;

    if (fish.thinkTimer >= 200) {
      fish.thinkTimer = 0;
      switch (fish.state) {
        case Idle:
          if (randomFloat(0, 1) < 0.2f) {
            fish.vx = randomFloat(-0.5f, 0.5f);
            fish.vy = randomFloat(-0.2f, 0.2f);
          }
          if (hasLiveFood()) {
            fish.state = SeekFood;
          }
          break;
        case Scared:
          fish.vx = fish.direction * 3.5f;
          fish.vy = randomFloat(-1.0f, 1.0f);
          fish.state = Hidden;
          fish.visible = false;
          fish.hideTimer = 3000 + static_cast<uint16_t>(randomInt(3000));
          break;
        case Hidden:
          if (fish.hideTimer > 200) {
            fish.hideTimer -= 200;
          } else {
            const bool fromLeft = randomFloat(0, 1) > 0.5f;
            fish.x = fromLeft ? kAquariumLeft : kAquariumRight - fish.drawWidth;
            fish.y = randomFloat(kAquariumTop + 8, kAquariumBottom - fish.drawHeight - 4);
            fish.direction = fromLeft ? 1 : -1;
            fish.vx = fish.direction * 0.35f;
            fish.vy = 0.0f;
            fish.visible = true;
            fish.state = Idle;
            std::printf("%s returned\n", fish.name);
          }
          break;
        case SeekFood:
          seekFood(fish);
          if (!hasLiveFood()) {
            fish.state = Idle;
          }
          break;
        case Play:
        case Sleep:
          fish.state = Idle;
          break;
      }
    }

    if (!fish.visible) {
      return;
    }

    fish.hunger = std::max(0.0f, fish.hunger - 0.001f * dt);
    fish.happiness = std::max(0.0f, fish.happiness - 0.0005f * dt);

    fish.x += fish.vx * (dt / 40.0f);
    fish.y += fish.vy * (dt / 40.0f);

    if (std::fabs(fish.vx) > 0.05f) {
      fish.direction = fish.vx > 0 ? 1 : -1;
    }

    if (fish.x < kAquariumLeft) {
      fish.x = kAquariumLeft;
      fish.vx = std::fabs(fish.vx);
    }
    if (fish.x > kAquariumRight - fish.drawWidth) {
      fish.x = kAquariumRight - fish.drawWidth;
      fish.vx = -std::fabs(fish.vx);
    }
    if (fish.y < kAquariumTop) {
      fish.y = kAquariumTop;
      fish.vy = std::fabs(fish.vy);
    }
    if (fish.y > kAquariumBottom - fish.drawHeight) {
      fish.y = kAquariumBottom - fish.drawHeight;
      fish.vy = -std::fabs(fish.vy);
    }

    fish.animationTimer += dt;
    if (fish.animationTimer >= 150) {
      fish.animationTimer = 0;
      fish.animationFrame = (fish.animationFrame + 1) % 8;
    }
  }

  void updateBubbles(uint16_t dt) {
    bubbleSpawnTimer_ += dt;
    if (bubbleSpawnTimer_ >= nextBubbleSpawn_) {
      bubbleSpawnTimer_ = 0;
      nextBubbleSpawn_ = 500 + randomInt(1500);

      for (auto& bubble : bubbles_) {
        if (!bubble.alive) {
          bubble.x = randomFloat(kAquariumLeft, kAquariumRight);
          bubble.y = kAquariumBottom - 4;
          bubble.velocity = randomFloat(0.3f, 0.8f);
          bubble.drift = randomFloat(-0.2f, 0.2f);
          bubble.alive = true;
          break;
        }
      }
    }

    for (auto& bubble : bubbles_) {
      if (!bubble.alive) {
        continue;
      }
      bubble.y -= bubble.velocity * (dt / 40.0f);
      bubble.x += bubble.drift * (dt / 40.0f);
      if (bubble.y < kAquariumTop) {
        bubble.alive = false;
      }
    }
  }

  void updateFood(uint16_t dt) {
    for (auto& item : food_) {
      if (!item.alive) {
        continue;
      }
      item.y += item.vy * (dt / 40.0f);
      if (item.y > kAquariumBottom - 5) {
        item.y = kAquariumBottom - 5;
        item.vy = 0;
      }
    }
  }

  void drawHungerBar(Framebuffer& framebuffer, float hunger) {
    constexpr int x = 5;
    constexpr int y = kAquariumBottom + 4;
    drawText(framebuffer, "HGR", x, y, rgb(170, 170, 170));
    framebuffer.fillRect(x + 20, y, 40, 5, rgb(51, 51, 51));
    const int fillWidth = static_cast<int>((hunger / 100.0f) * 38.0f);
    const uint16_t color = hunger > 50.0f ? rgb(0, 204, 0) : hunger > 25.0f ? rgb(204, 204, 0) : rgb(204, 0, 0);
    framebuffer.fillRect(x + 21, y + 1, fillWidth, 3, color);
  }

  void setupPlants() {
    for (int i = 0; i < 5; ++i) {
      const bool amazonSword = i % 2 == 0;
      plants_[i] = {
        amazonSword ? &amazon_sword_sheet : &cabomba_sheet,
        8 + i * 50,
        kAquariumBottom,
        randomFloat(0, 6.28318f),
        static_cast<uint8_t>(i),
        static_cast<uint16_t>(amazonSword ? 30 : 26),
        static_cast<int>(28 + randomInt(5)),
      };
    }
  }

  std::mt19937 random_{std::random_device{}()};
  std::array<Fish, 3> fish_{};
  std::array<Bubble, 20> bubbles_{};
  std::array<Food, 20> food_{};
  std::array<Plant, 5> plants_{};
  uint32_t worldTime_ = 0;
  uint16_t bubbleSpawnTimer_ = 0;
  uint16_t nextBubbleSpawn_ = 800;
  int batteryLevel_ = 87;
  uint32_t lastInteractionTime_ = 0;
  uint8_t currentBrightness_ = kActiveBrightness;
  bool displayActive_ = true;
  bool charging_ = false;
  FirmwareApp currentApp_ = AquariumApp;
  PomodoroMode pomodoroMode_ = PomodoroIdle;
  uint8_t simulatedTiltTomatoes_ = 1;
  uint8_t selectedTomatoes_ = 1;
  uint8_t completedTomatoes_ = 0;
  uint32_t phaseStartedAt_ = 0;
  uint32_t phaseDurationMs_ = 0;
  bool pendingVfdSingleClick_ = false;
  uint32_t pendingVfdSingleClickAt_ = 0;
};

uint32_t expand565(uint16_t color, uint8_t brightness) {
  const uint8_t red = static_cast<uint8_t>((((color >> 11) & 0x1f) * 255 / 31) * brightness / 255);
  const uint8_t green = static_cast<uint8_t>((((color >> 5) & 0x3f) * 255 / 63) * brightness / 255);
  const uint8_t blue = static_cast<uint8_t>(((color & 0x1f) * 255 / 31) * brightness / 255);
  return 0xff000000u | (red << 16) | (green << 8) | blue;
}

void copyFramebufferToTexture(const Framebuffer& framebuffer, SDL_Texture* texture, uint8_t brightness) {
  void* texturePixels = nullptr;
  int pitch = 0;
  if (SDL_LockTexture(texture, nullptr, &texturePixels, &pitch) != 0) {
    return;
  }

  auto* output = static_cast<uint32_t*>(texturePixels);
  const int stride = pitch / static_cast<int>(sizeof(uint32_t));
  const auto& input = framebuffer.pixels();
  for (int y = 0; y < kDisplayHeight; ++y) {
    for (int x = 0; x < kDisplayWidth; ++x) {
      output[y * stride + x] = expand565(input[y * kDisplayWidth + x], brightness);
    }
  }

  SDL_UnlockTexture(texture);
}

void updateWindowTitle(SDL_Window* window, const Simulator& simulator) {
  char title[128];
  const uint32_t now = SDL_GetTicks();
  std::snprintf(title, sizeof(title), "AquaLife Simulator - %s - Battery %d%%%s - %d FPS - Brightness %u",
                simulator.currentAppName(), simulator.batteryLevel(), simulator.charging() ? " charging" : "",
                simulator.currentFps(now), simulator.brightness());
  SDL_SetWindowTitle(window, title);
}

}  // namespace

int main() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  int scale = 4;
  SDL_Window* window = SDL_CreateWindow("AquaLife Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kDisplayWidth * scale, kDisplayHeight * scale, SDL_WINDOW_RESIZABLE);
  if (window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == nullptr) {
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(renderer, kDisplayWidth, kDisplayHeight);

  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                           kDisplayWidth, kDisplayHeight);
  if (texture == nullptr) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  Simulator simulator;
  Framebuffer framebuffer;
  uint32_t lastFrame = SDL_GetTicks();
  bool running = true;
  const char* frameLimitText = std::getenv("AQUALIFE_SIM_FRAMES");
  const int frameLimit = frameLimitText == nullptr ? 0 : std::atoi(frameLimitText);
  int renderedFrames = 0;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        const SDL_Keycode key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE) {
          running = false;
        } else if (key == SDLK_a) {
          simulator.handleA(SDL_GetTicks());
        } else if (key == SDLK_b) {
          simulator.switchToNextApp(SDL_GetTicks());
        } else if (key == SDLK_s) {
          simulator.shake(SDL_GetTicks());
        } else if (key == SDLK_c) {
          simulator.setCharging(!simulator.charging());
        } else if (key == SDLK_LEFT) {
          simulator.tiltTimerSmaller(SDL_GetTicks());
        } else if (key == SDLK_RIGHT) {
          simulator.tiltTimerLarger(SDL_GetTicks());
        } else if (key == SDLK_UP) {
          simulator.setBatteryLevel(simulator.batteryLevel() + 5);
        } else if (key == SDLK_DOWN) {
          simulator.setBatteryLevel(simulator.batteryLevel() - 5);
        } else if (key >= SDLK_1 && key <= SDLK_6) {
          scale = static_cast<int>(key - SDLK_0);
          SDL_SetWindowSize(window, kDisplayWidth * scale, kDisplayHeight * scale);
        }
      }
    }

    const uint32_t now = SDL_GetTicks();
    simulator.updateBrightness(now);
    if (now - lastFrame < static_cast<uint32_t>(simulator.currentFrameTimeMs(now))) {
      SDL_Delay(1);
      continue;
    }

    const auto dt = static_cast<uint16_t>(now - lastFrame);
    lastFrame = now;

    simulator.update(dt, now);
    simulator.render(framebuffer);
    copyFramebufferToTexture(framebuffer, texture, simulator.brightness());
    updateWindowTitle(window, simulator);

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    ++renderedFrames;
    if (frameLimit > 0 && renderedFrames >= frameLimit) {
      running = false;
    }
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
