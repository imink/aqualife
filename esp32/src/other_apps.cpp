#include "other_apps.h"

#include "app_runtime.h"
#include "aqualife_app.h"

void renderFishStatusApp() {
  framebuffer.fillScreen(rgb(8, 12, 24));
  drawAppHeader("Fish Status");

  constexpr int rowTop = 22;
  constexpr int rowHeight = 33;
  constexpr int barX = 76;
  constexpr int barWidth = 112;

  for (int i = 0; i < aquaLifeFishCount(); i++) {
    const int y = rowTop + i * rowHeight;
    framebuffer.fillRect(4, y - 3, DISPLAY_WIDTH - 8, rowHeight - 3, rgb(13, 25, 42));
    drawPixelText(aquaLifeFishName(i), 9, y, rgb(230, 240, 255));
    drawPixelText("Hunger", 9, y + 11, rgb(160, 180, 205));
    drawPixelText("Happy", 9, y + 21, rgb(160, 180, 205));
    drawProgressBar(barX, y + 10, barWidth, 7, aquaLifeFishHunger(i), rgb(96, 220, 130));
    drawProgressBar(barX, y + 20, barWidth, 7, aquaLifeFishHappiness(i), rgb(255, 198, 82));
  }

  drawBatteryStatus();
  framebuffer.pushSprite(0, 0);
}

void renderDeviceInfoApp() {
  framebuffer.fillScreen(rgb(14, 14, 18));
  drawAppHeader("Device Info");
  updateBatteryStatus();

  char line[64];
  snprintf(line, sizeof(line), "Version %s", AQUALIFE_VERSION);
  drawPixelText(line, 9, 24, rgb(230, 240, 255));

  snprintf(line, sizeof(line), "Git %s", AQUALIFE_GIT_SHA);
  drawPixelText(line, 9, 37, rgb(180, 200, 225));

  snprintf(line, sizeof(line), "Battery %ld%%", static_cast<long>(batteryLevel));
  drawPixelText(line, 9, 50, rgb(180, 200, 225));

  snprintf(line, sizeof(line), "IMU %s", imuEnabled ? "enabled" : "disabled");
  drawPixelText(line, 9, 63, imuEnabled ? rgb(90, 255, 150) : rgb(255, 130, 130));

  snprintf(line, sizeof(line), "Uptime %lu s", static_cast<unsigned long>(millis() / 1000));
  drawPixelText(line, 9, 76, rgb(180, 200, 225));

  drawPixelText("Button B: next app", 9, DISPLAY_HEIGHT - 15, rgb(130, 150, 180));
  drawBatteryStatus();
  framebuffer.pushSprite(0, 0);
}
