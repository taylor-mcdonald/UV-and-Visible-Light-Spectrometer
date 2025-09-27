#pragma once

#include <Arduino.h>
//#include <Adafruit_AS7341.h> // Include the Adafruit AS7341 library for AS7341
#include "driver/MyAS7341.h"
#include "shared/SharedData.h"
#include "config.h"


void initAS7341Sensor(void);
void initAS7341interrupt(void);

void startSpectralTasks(void);

void AS7341InterruptHandler(void *pvParameters);

void AS7341InterruptTask(void *pvParameters);
void AS7341_Set_SMUX_Task(void *pvParameters);
void AS7341_Read_Results_Task(void *pvParameters);
//void AS7341sensorTask(void *pvParameters);

void printAS7341registers(void);

void testAS7341_INT_simple(void);
