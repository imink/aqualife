#pragma once

#include <Arduino.h>

int aquaLifeFishCount();
const char* aquaLifeFishName(int index);
float aquaLifeFishHunger(int index);
float aquaLifeFishHappiness(int index);

void setupAquaLifeApp();
void feedAquaLifeApp();
void scareAquaLifeFish();
void updateAquaLifeApp(uint16_t dt);
void renderAquaLifeApp();
