// BLETask.h
#pragma once
#include <NimBLEDevice.h>

void initBLE();
void startBLETask();
void updateBME680Characteristic();
void updateAS7341LowCharacteristic();
void updateAS7341HighCharacteristic();
void updateUVCharacteristic();
void updateBatteryCharacteristic();

extern NimBLECharacteristic* pBME680Characteristic;
extern NimBLECharacteristic* pAS7341LowCharacteristic;
extern NimBLECharacteristic* pAS7341HighCharacteristic;
extern NimBLECharacteristic* pUVCharacteristic;
extern NimBLECharacteristic* pBatteryCharacteristic;