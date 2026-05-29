#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "app_runtime.h"
#include "aqualife_app.h"
#include "other_apps.h"
#include "vfd_time.h"

constexpr int ACTIVE_FPS = 25;
constexpr int IDLE_FPS = 12;
constexpr int ACTIVE_FRAME_TIME_MS = 1000 / ACTIVE_FPS;
constexpr int IDLE_FRAME_TIME_MS = 1000 / IDLE_FPS;
constexpr uint32_t ACTIVE_DISPLAY_MS = 8000;
constexpr float SHAKE_THRESHOLD = 2.8f;

M5Canvas framebuffer(&M5.Display);

uint32_t lastFrame = 0;
uint32_t lastBatteryRead = 0;
uint32_t lastInteractionTime = 0;
uint32_t lastImuRead = 0;
uint32_t lastImuDebugLog = 0;
uint32_t shakeStartedAt = 0;
int32_t batteryLevel = -1;
uint8_t currentBrightness = ACTIVE_BRIGHTNESS;
FirmwareApp currentApp = AquariumApp;
bool displayActive = true;
bool imuEnabled = false;
bool imuUpdated = false;
float imuAccelX = 0.0f;
float imuAccelY = 0.0f;
float imuAccelZ = 0.0f;
float imuShakeStrength = 0.0f;
uint32_t shakeDuration = 0;
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

float clampPercent(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 100.0f) {
    return 100.0f;
  }
  return value;
}

bool isActiveDisplay(uint32_t now) {
  return now - lastInteractionTime < ACTIVE_DISPLAY_MS;
}

int currentFrameTimeMs(uint32_t now) {
  return isActiveDisplay(now) ? ACTIVE_FRAME_TIME_MS : IDLE_FRAME_TIME_MS;
}

void markInteraction() {
  lastInteractionTime = millis();
  if (!displayActive) {
    displayActive = true;
    LOG_PRINTF("Display active lightness: %u -> %u\n", M5.Display.getBrightness(), ACTIVE_BRIGHTNESS);
  }
  if (currentBrightness != ACTIVE_BRIGHTNESS) {
    currentBrightness = ACTIVE_BRIGHTNESS;
    M5.Display.setBrightness(currentBrightness);
  }
}

void updateDisplayBrightness(uint32_t now) {
  const bool active = isActiveDisplay(now);
  if (displayActive != active) {
    displayActive = active;
    LOG_PRINTF("Display %s lightness: %u -> %u\n", active ? "active" : "inactive", M5.Display.getBrightness(),
               active ? ACTIVE_BRIGHTNESS : IDLE_BRIGHTNESS);
  }

  const uint8_t targetBrightness = active ? ACTIVE_BRIGHTNESS : IDLE_BRIGHTNESS;
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

const char* currentAppName() {
  switch (currentApp) {
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

void switchToNextApp() {
  currentApp = static_cast<FirmwareApp>((static_cast<uint8_t>(currentApp) + 1) % FIRMWARE_APP_COUNT);
  LOG_PRINTF("BTN_B: Switch app -> %s\n", currentAppName());
}

void drawAppHeader(const char* title) {
  framebuffer.fillRect(0, 0, DISPLAY_WIDTH, 13, rgb(4, 18, 44));
  drawPixelText(title, 4, 3, rgb(230, 240, 255));

  char indexText[8];
  snprintf(indexText, sizeof(indexText), "%u/%u", static_cast<unsigned>(static_cast<uint8_t>(currentApp) + 1),
           static_cast<unsigned>(FIRMWARE_APP_COUNT));
  drawPixelText(indexText, DISPLAY_WIDTH - 27, 3, rgb(170, 190, 220));
}

void drawProgressBar(int x, int y, int width, int height, float percent, uint16_t color) {
  framebuffer.drawRect(x, y, width, height, rgb(72, 84, 108));
  const int fillWidth = static_cast<int>((clampPercent(percent) / 100.0f) * (width - 2));
  framebuffer.fillRect(x + 1, y + 1, fillWidth, height - 2, color);
}

void updateBatteryStatus() {
  const uint32_t now = millis();
  if (now - lastBatteryRead < BATTERY_READ_MS && batteryLevel >= 0) {
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

void updateControls() {
  M5.update();

  if (currentApp == VfdTimeApp) {
    if (M5.BtnA.wasDoubleClicked()) {
      markInteraction();
      handleVfdTimeButtonADoubleClick();
    } else if (M5.BtnA.wasSingleClicked()) {
      markInteraction();
      handleVfdTimeButtonASingleClick();
    }
  } else {
    if (M5.BtnA.wasPressed()) {
      markInteraction();
      feedAquaLifeApp();
    }
  }
  if (M5.BtnB.wasPressed()) {
    markInteraction();
    switchToNextApp();
  }

  const uint32_t now = millis();
  if (now - lastImuRead < IMU_SAMPLE_MS) {
    return;
  }
  lastImuRead = now;

  imuEnabled = M5.Imu.isEnabled();
  imuUpdated = M5.Imu.update() != 0;
  if (imuUpdated) {
    auto imu = M5.Imu.getImuData();
    imuAccelX = imu.accel.x;
    imuAccelY = imu.accel.y;
    imuAccelZ = imu.accel.z;
    imuShakeStrength = fabs(imuAccelX) + fabs(imuAccelY) + fabs(imuAccelZ - 1.0f);
  }

  if (IMU_DEBUG_ENABLED && now - lastImuDebugLog >= 1000) {
    lastImuDebugLog = now;
    LOG_PRINTF("IMU enabled=%d updated=%d accel=(%.2f, %.2f, %.2f) shake=%.2f duration=%lu ms\n", imuEnabled,
               imuUpdated, imuAccelX, imuAccelY, imuAccelZ, imuShakeStrength, static_cast<unsigned long>(shakeDuration));
  }

  if (currentApp != VfdTimeApp && imuUpdated && imuShakeStrength > SHAKE_THRESHOLD) {
    if (shakeStartedAt == 0) {
      shakeStartedAt = now;
      LOG_PRINTF("IMU shake started: %.2f\n", imuShakeStrength);
    }
    shakeDuration = now - shakeStartedAt;
  } else {
    if (shakeDuration > 0) {
      LOG_PRINTF("IMU shake stopped after %lu ms\n", static_cast<unsigned long>(shakeDuration));
      if (shakeDuration > SHAKE_SCARE_MS) {
        LOG_PRINTF("IMU shake duration accepted: %lu ms\n", static_cast<unsigned long>(shakeDuration));
        markInteraction();
        scareAquaLifeFish();
      }
    }
    shakeStartedAt = 0;
    shakeDuration = 0;
  }
}

void updateWorld(uint16_t dt) {
  updateControls();
  updateAquaLifeApp(dt);
  if (currentApp == VfdTimeApp) {
    updateVfdTimeApp(dt);
  }
}

void renderCurrentApp() {
  switch (currentApp) {
    case AquariumApp:
      renderAquaLifeApp();
      break;
    case VfdTimeApp:
      renderVfdTimeApp();
      break;
    case FishStatusApp:
      renderFishStatusApp();
      break;
    case DeviceInfoApp:
      renderDeviceInfoApp();
      break;
  }
}

void setup() {
  disableWireless();

  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  const uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 2000) {
    delay(10);
  }

  M5.Display.setRotation(1);
  M5.Display.setColorDepth(16);
  M5.Display.setTextFont(1);
  M5.Display.setBrightness(currentBrightness);
  M5.Display.fillScreen(TFT_BLACK);

  framebuffer.setColorDepth(16);
  if (!framebuffer.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
    LOG_PRINTLN("Failed to allocate framebuffer");
    while (true) {
      delay(1000);
    }
  }
  framebuffer.setTextFont(1);

  setupAquaLifeApp();
  setupVfdTimeApp();
  lastFrame = millis();
  lastInteractionTime = lastFrame;
  lastImuRead = lastFrame;

  LOG_PRINTF("AquaLife ESP32 firmware v%s (%s)\n", AQUALIFE_VERSION, AQUALIFE_GIT_SHA);
  LOG_PRINTF("Build time: %s\n", AQUALIFE_BUILD_TIME);
  LOG_PRINTLN("Display: 240x135 landscape");
  LOG_PRINTLN("FPS: 25 active / 12 idle");
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
  renderCurrentApp();
}
