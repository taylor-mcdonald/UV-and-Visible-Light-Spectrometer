#pragma once
#include <Arduino.h>
#include "shared/SharedData.h"
#include <Config.h>
#include <DS18B20.h>
#include <OneWire.h>


void initDS18B20Sensor();
void startDS18B20Task();
void DS18B20sensorTask(void *pvParameters);
