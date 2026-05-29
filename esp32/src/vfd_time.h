#pragma once

#include <Arduino.h>

void setupVfdTimeApp();
void updateVfdTimeApp(uint16_t dt);
void renderVfdTimeApp();
void handleVfdTimeButtonASingleClick();
void handleVfdTimeButtonADoubleClick();
