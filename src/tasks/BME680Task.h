#pragma once

#include <Arduino.h>
#include <Adafruit_BME680.h> // Include the Adafruit BME680 library
#include "shared/SharedData.h"
#include "config.h"

void initBME680Sensor(TwoWire &wirePort);
void startBME680Tasks(void);
void BME680SensorTask(void *pvParameters);

