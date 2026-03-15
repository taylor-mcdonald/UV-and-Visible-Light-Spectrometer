#include "FuelGaugeTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include "shared/SharedData.h"

SFE_MAX1704X lipo(MAX1704X_MAX17048);

void initFuelGauge(TwoWire &wirePort) {
    lipo.enableDebugging();  // temporary, remove once working

    if (!lipo.begin(wirePort)) {
        Serial.println("MAX17048: not found, check wiring!");
        while(1);
    }

    lipo.quickStart();
    lipo.setThreshold(20);  // alert at 20% battery

    Serial.printf("MAX17048: %.3fV  %.1f%%  %.2f%%/hr\n",
        lipo.getVoltage(),
        lipo.getSOC(),
        lipo.getChangeRate());
}

void fuelGaugeTask(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(30000);

    for (;;) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            batteryReading.voltage    = lipo.getVoltage();
            batteryReading.percent    = lipo.getSOC();
            batteryReading.changeRate = lipo.getChangeRate();
            batteryReading.timestamp  = millis();
            xSemaphoreGive(i2cMutex);
        } else {
            Serial.println("FuelGauge: mutex timeout");
        }

        vTaskDelayUntil(&lastWake, interval);
    }
}

void startFuelGaugeTask() {
    xTaskCreatePinnedToCore(
        fuelGaugeTask,
        "FuelGauge Task",
        2048,
        NULL,
        1,
        NULL,
        tskNO_AFFINITY
    );
}