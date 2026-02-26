#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t i2cMutex;
extern volatile uint32_t mutexTakenAt;
extern volatile const char* mutexTakenBy;