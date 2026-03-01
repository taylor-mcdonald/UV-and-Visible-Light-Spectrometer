#include <Arduino.h>
#include <Wire.h>
#include "I2CBus.h"
SemaphoreHandle_t i2cMutex = nullptr;
volatile uint32_t mutexTakenAt = 0;
volatile const char* mutexTakenBy = "none";

void i2cScan() {
    Serial.println("Scanning I2C bus...");
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("I2C device found at 0x");
            Serial.println(addr, HEX);
        }
    }
    Serial.println("Scan complete");
}