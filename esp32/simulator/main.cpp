#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
constexpr int kFps = 25;
constexpr int kFrameTimeMs = 1000 / kFps;
constexpr int kAquariumTop = 12;
constexpr int kAquariumBottom = kDisplayHeight - 20;
constexpr int kAquariumLeft = 2;
constexpr int kAquariumRight = kDisplayWidth - 2;

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
      { &whale_sheet, "whale", 120, 42, 0.3f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 48, 27 },
      { &hammerhead_sheet, "hammerhead", 70, 62, 0.4f, 0.0f, 1, 0, 0, 0, 0, Idle, true, 34, 18 },
    }};
    setupPlants();
  }

  void feed() { spawnFood(); }

  void play() {
    std::puts("B: Play");
    for (auto& fish : fish_) {
      if (fish.visible && fish.state == Idle) {
        fish.state = Play;
        break;
      }
    }
  }

  void shake() { scareAllFish(); }

  void setBatteryLevel(int level) {
    batteryLevel_ = std::clamp(level, 0, 100);
  }

  void setCharging(bool charging) { charging_ = charging; }

  int batteryLevel() const { return batteryLevel_; }

  bool charging() const { return charging_; }

  void update(uint16_t dt) {
    for (auto& fish : fish_) {
      updateFish(fish, dt);
    }
    updateBubbles(dt);
    updateFood(dt);
    worldTime_ += dt;
  }

  void render(Framebuffer& framebuffer) {
    framebuffer.fillScreen(rgb(0, 8, 32));

    for (int y = 0; y < kDisplayHeight; y += 16) {
      const uint8_t shade = 8 + (y * 18) / kDisplayHeight;
      framebuffer.fillRect(0, y, kDisplayWidth, 16, rgb(0, shade, 36));
    }

    framebuffer.fillRect(0, kAquariumBottom - 3, kDisplayWidth, 6, rgb(136, 102, 34));

    for (const auto& plant : plants_) {
      const float sway = std::sin(worldTime_ * 0.0015f + plant.offset) * 6.0f;
      const float tipX = plant.x + sway;
      for (int i = 0; i < plant.height; i += 3) {
        const float t = static_cast<float>(i) / plant.height;
        const float bend = t * t;
        const int px = std::lround(plant.x + (tipX - plant.x) * bend);
        const int py = plant.y - i;
        framebuffer.fillRect(px, py, 2, 3, rgb(0, 80 + static_cast<uint8_t>(t * 70), 0));
      }
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

    drawText(framebuffer, "A FEED", 5, kDisplayHeight - 10, rgb(110, 110, 150));
    drawText(framebuffer, "B PLAY", kDisplayWidth - 46, kDisplayHeight - 10, rgb(110, 110, 150));
    drawText(framebuffer, "AquaLife", 4, 3, rgb(210, 210, 210));
    drawBatteryStatus(framebuffer);
  }

 private:
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

  void spawnFood() {
    for (auto& item : food_) {
      if (!item.alive) {
        item.x = randomFloat(kAquariumLeft + 8, kAquariumRight - 8);
        item.y = kAquariumTop + 4;
        item.vy = 0.3f;
        item.alive = true;
        std::puts("A: Feed");
        return;
      }
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
        case Play:
        case SeekFood:
        case Sleep:
          fish.state = Idle;
          break;
      }
    }

    if (!fish.visible) {
      return;
    }

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

  void setupPlants() {
    for (int i = 0; i < 5; ++i) {
      plants_[i] = {15 + i * 55, kAquariumBottom, randomFloat(0, 6.28318f), static_cast<int>(12 + randomInt(16))};
    }
  }

  std::mt19937 random_{std::random_device{}()};
  std::array<Fish, 2> fish_{};
  std::array<Bubble, 20> bubbles_{};
  std::array<Food, 20> food_{};
  std::array<Plant, 5> plants_{};
  uint32_t worldTime_ = 0;
  uint16_t bubbleSpawnTimer_ = 0;
  uint16_t nextBubbleSpawn_ = 800;
  int batteryLevel_ = 87;
  bool charging_ = false;
};

uint32_t expand565(uint16_t color) {
  const uint8_t red = static_cast<uint8_t>(((color >> 11) & 0x1f) * 255 / 31);
  const uint8_t green = static_cast<uint8_t>(((color >> 5) & 0x3f) * 255 / 63);
  const uint8_t blue = static_cast<uint8_t>((color & 0x1f) * 255 / 31);
  return 0xff000000u | (red << 16) | (green << 8) | blue;
}

void copyFramebufferToTexture(const Framebuffer& framebuffer, SDL_Texture* texture) {
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
      output[y * stride + x] = expand565(input[y * kDisplayWidth + x]);
    }
  }

  SDL_UnlockTexture(texture);
}

void updateWindowTitle(SDL_Window* window, const Simulator& simulator) {
  char title[128];
  std::snprintf(title, sizeof(title), "AquaLife Simulator - Battery %d%%%s", simulator.batteryLevel(),
                simulator.charging() ? " charging" : "");
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
          simulator.feed();
        } else if (key == SDLK_b) {
          simulator.play();
        } else if (key == SDLK_s) {
          simulator.shake();
        } else if (key == SDLK_c) {
          simulator.setCharging(!simulator.charging());
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
    if (now - lastFrame < kFrameTimeMs) {
      SDL_Delay(1);
      continue;
    }

    const auto dt = static_cast<uint16_t>(now - lastFrame);
    lastFrame = now;

    simulator.update(dt);
    simulator.render(framebuffer);
    copyFramebufferToTexture(framebuffer, texture);
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