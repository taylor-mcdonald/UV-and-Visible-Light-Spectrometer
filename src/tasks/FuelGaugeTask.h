#pragma once
#include <Wire.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>

void initFuelGauge(TwoWire &wirePort);
void startFuelGaugeTask();