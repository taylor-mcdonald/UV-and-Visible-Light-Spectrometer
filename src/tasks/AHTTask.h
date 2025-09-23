#pragma once

#include <Adafruit_AHTX0.h> // Include the Adafruit AHTX0 library for AHT21
#include "shared/SharedData.h"
#include "Config.h"

void initAHT21Sensor();
void startAHTTask();

void AHT21sensorTask(void *pvParameters);