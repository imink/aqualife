#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#if __has_include("build_info.h")
#include "build_info.h"
#endif
#include "sprites.h"

#ifndef AQUALIFE_VERSION
#define AQUALIFE_VERSION "dev"
#endif

#ifndef AQUALIFE_GIT_SHA
#define AQUALIFE_GIT_SHA "unknown"
#endif

#ifndef AQUALIFE_BUILD_TIME
#define AQUALIFE_BUILD_TIME "unknown"
#endif

constexpr int DISPLAY_WIDTH = 240;
constexpr int DISPLAY_HEIGHT = 135;
constexpr uint32_t BATTERY_READ_MS = 60000;
constexpr uint32_t IMU_SAMPLE_MS = 150;
constexpr uint32_t SHAKE_SCARE_MS = 300;
constexpr uint8_t ACTIVE_BRIGHTNESS = 220;
constexpr uint8_t IDLE_BRIGHTNESS = 3;
constexpr bool LOGS_ENABLED = true;
constexpr bool IMU_DEBUG_ENABLED = false;

constexpr int AQUARIUM_TOP = 12;
constexpr int AQUARIUM_BOTTOM = DISPLAY_HEIGHT - 20;
constexpr int AQUARIUM_LEFT = 2;
constexpr int AQUARIUM_RIGHT = DISPLAY_WIDTH - 2;

#define LOG_PRINTLN(message) \
  do { \
    if (LOGS_ENABLED) { \
      Serial.println(message); \
    } \
  } while (false)

#define LOG_PRINTF(...) \
  do { \
    if (LOGS_ENABLED) { \
      Serial.printf(__VA_ARGS__); \
    } \
  } while (false)

enum FirmwareApp : uint8_t {
  AquariumApp,
  VfdTimeApp,
  FishStatusApp,
  DeviceInfoApp,
};

constexpr uint8_t FIRMWARE_APP_COUNT = 4;

extern M5Canvas framebuffer;
extern uint32_t lastBatteryRead;
extern int32_t batteryLevel;
extern bool imuEnabled;
extern bool imuUpdated;
extern float imuAccelX;
extern float imuAccelY;
extern float imuAccelZ;
extern float imuShakeStrength;
extern uint32_t shakeDuration;
extern m5::Power_Class::is_charging_t batteryChargeState;
extern FirmwareApp currentApp;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
float randFloat(float min, float max);
float clampPercent(float value);

void drawSpriteMasked(const SpriteSheet& sheet, uint8_t frame, int x, int y, int w, int h, bool flipX);
void drawPixelText(const char* text, int x, int y, uint16_t color);
void drawAppHeader(const char* title);
void drawProgressBar(int x, int y, int width, int height, float percent, uint16_t color);
void updateBatteryStatus();
void drawBatteryStatus();
