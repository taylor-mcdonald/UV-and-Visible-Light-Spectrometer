#pragma once

#include <Arduino.h>
//#include <Adafruit_AS7341.h> // Include the Adafruit AS7341 library for AS7341
#include "driver/MyAS7341.h"
#include "shared/SharedData.h"
#include "config.h"


void initAS7341Sensor(TwoWire &wirePort);

void startSpectralTasks(void);

void AS7341InterruptHandler(void *pvParameters);

void AS7341InterruptTask(void *pvParameters);
void AS7341_Set_SMUX_Task(void *pvParameters);
void AS7341_Read_Results_Task(void *pvParameters);

void AS7341_Flicker_Capture_Task(void *pvParameters);
void AS7341_Spectral_Capture_Task(void *pvParameters);

void setupForSpectral(void);
void setupForFlicker(uint8_t gain);