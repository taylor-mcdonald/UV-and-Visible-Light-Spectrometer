#include "I2CBus.h"
SemaphoreHandle_t i2cMutex = nullptr;
volatile uint32_t mutexTakenAt = 0;
volatile const char* mutexTakenBy = "none";