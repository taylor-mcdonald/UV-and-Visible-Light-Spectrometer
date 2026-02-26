// BLETask.h
#pragma once
#include <NimBLEDevice.h>

void initBLE();
void startBLETask();
void updateBME680Characteristic();
void updateAS7341Characteristic();
void updateUVCharacteristic();
void updateBatteryCharacteristic();

extern NimBLECharacteristic* pBME680Characteristic;
extern NimBLECharacteristic* pAS7341Characteristic;
extern NimBLECharacteristic* pUVCharacteristic;
extern NimBLECharacteristic* pBatteryCharacteristic;