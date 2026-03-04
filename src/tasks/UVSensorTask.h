#pragma once

#include <Arduino.h>
#include <SparkFun_AS7331.h>
#include "ScreenTask.h"
#include "shared/SharedData.h"
#include "Config.h"


void UVsensorTask(void *pvParameters);
void startUVsensorMeasurementTask(void *pvParameters);

void initUVSensor(TwoWire &wirePort);
void startUVSensorTask(void);
void initUVSensorInterrupt(void);