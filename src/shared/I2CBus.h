#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t i2cMutex1;
extern volatile uint32_t mutexTakenAt;
extern volatile const char* mutexTakenBy;

void i2cBusScan(TwoWire &wire, const char* busName);