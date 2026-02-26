#include "FuelGaugeTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include "shared/SharedData.h"

SFE_MAX1704X lipo(MAX1704X_MAX17043);

void initFuelGauge(TwoWire &wirePort) {
    lipo.enableDebugging();  // temporary, remove once working

    if (!lipo.begin(wirePort)) {
        Serial.println("MAX17043: not found, check wiring!");
        while(1);
    }

    lipo.quickStart();
    lipo.setThreshold(20);  // alert at 20% battery

    Serial.println("MAX17043: found");
    Serial.print("MAX17043: voltage = ");
    Serial.print(lipo.getVoltage(), 3);
    Serial.println("V");
    Serial.print("MAX17043: percent = ");
    Serial.print(lipo.getSOC(), 1);
    Serial.println("%");
}

void fuelGaugeTask(void *pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(30000);

    for (;;) {
        if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            batteryVoltage = lipo.getVoltage();
            batteryPercent = lipo.getSOC();
            xSemaphoreGive(i2cMutex);

            Serial.print("Battery: ");
            Serial.print(batteryVoltage, 3);
            Serial.print("V  ");
            Serial.print(batteryPercent, 1);
            Serial.println("%");
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