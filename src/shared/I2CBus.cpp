#include <Arduino.h>
#include <Wire.h>
#include "I2CBus.h"
SemaphoreHandle_t i2cMutex = nullptr;
SemaphoreHandle_t i2cMutex1 = nullptr;
volatile uint32_t mutexTakenAt = 0;
volatile const char* mutexTakenBy = "none";

void i2cBusScan(TwoWire &wire, const char* busName) {
    Serial.print("Scanning ");
    Serial.print(busName);
    Serial.println("...");
    
    int deviceCount = 0;
    for (byte addr = 1; addr < 127; addr++) {
        wire.beginTransmission(addr);
        if (wire.endTransmission() == 0) {
            Serial.print("  Device found at 0x");
            Serial.println(addr, HEX);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("  No devices found");
    }
    
    Serial.print(busName);
    Serial.print(" scan complete, ");
    Serial.print(deviceCount);
    Serial.println(" device(s) found");
}