#include "tasks/BME680Task.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"

// Task handle
TaskHandle_t bme680TaskHandle = nullptr;

// Create an instance of the BME680 sensor object
Adafruit_BME680 bme680;

// Initialize the BME680 sensor
void initBME680Sensor(TwoWire &wirePort) {
if (!bme680.begin(BME680_ADDRESS, &wirePort)) {
        Serial.println("BME680: begin() failed!");
        while (1);
    }
    Serial.println("BME680: begin() OK");

    bme680.setTemperatureOversampling(BME680_OS_8X);
    bme680.setPressureOversampling(BME680_OS_4X);
    bme680.setHumidityOversampling(BME680_OS_2X);
    bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme680.setGasHeater(320, 150); // 320°C for 150ms

    Serial.println("BME680: configuration written");

    // beginReading() returns absolute completion timestamp, not a duration
    uint32_t endTime = bme680.beginReading();
    if (endTime == 0) {
        Serial.println("BME680: beginReading() failed!");
        while(1);
    }
    Serial.print("BME680: reading started, will be ready at t=");
    Serial.print(endTime);
    Serial.println("ms");

    // Wait until the absolute end time
    while (millis() < endTime) {
        delay(1);
    }
    delay(10); // small extra margin

    if (!bme680.endReading()) {
        Serial.println("BME680: first endReading() failed!");
        while(1);
    }
    Serial.print("BME680: first reading OK");
    Serial.print(" temp=");     Serial.print(bme680.temperature);
    Serial.print(" humidity="); Serial.print(bme680.humidity);
    Serial.print(" pressure="); Serial.print(bme680.pressure / 100.0f);
    Serial.print(" gas=");      Serial.println(bme680.gas_resistance);

    // Start the reading the task will pick up on first fire
    endTime = bme680.beginReading();
    if (endTime == 0) {
        Serial.println("BME680: second beginReading() failed!");
        while(1);
    }
    Serial.println("BME680: init complete, first task reading started");
}

// Task start function
void startBME680Tasks(void) {
  xTaskCreatePinnedToCore(
    BME680SensorTask,        // Function that implements the task.
    "BME680SensorTask",      // Text name for the task. 
    4096,                     // Stack size in words, not bytes.
    NULL,                     // Parameter passed into the task. 
    1,                        // Priority at which the task is created.
    &bme680TaskHandle,        // Pointer to the task handle.
    tskNO_AFFINITY            // Core where the task should run
  );
}

// Task function
void BME680SensorTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(BME680_UPDATE_MS);

  for (;;) {

    Serial.println("BME680 Interrupt detected");

    // if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    //   mutexTakenAt = millis();
    //   mutexTakenBy = "BME680";  // change label per task
    //   // Perform a reading
    //   if (!bme680.endReading()) {
    //     Serial.println("Failed to perform reading :(");
    //   } else {
    //     addBME680Reading(bme680.temperature, bme680.humidity, bme680.pressure, bme680.gas_resistance);
    //   }

    //   // Start the next reading (non-blocking)
    //   if(bme680.beginReading() == 0) {
    //     Serial.println("Failed to start new BME680reading :(");
    //   } 
    //   mutexTakenBy = "none";
    //   xSemaphoreGive(i2cMutex);
    // } else {
    //     Serial.println("BME680 Sensor Task: mutex timeout");
    // }
        
    // // Serial print the reading for debugging
    // printLatestBME680();

        uint32_t remaining = bme680.remainingReadingMillis();
        Serial.print("BME680: remaining=");
        Serial.println(remaining);

        // 0xFFFFFFFF means underflow -- treat as ready
        // Also treat anything over 5000ms as corrupted state
        bool dataReady = (remaining == 0 || remaining > 5000);

        if (dataReady) {
            if (remaining > 5000) {
                Serial.println("BME680: remaining looks corrupted, attempting read anyway");
            }

            if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                mutexTakenBy = "BME680";
                mutexTakenAt = millis();

                if (!bme680.endReading()) {
                    Serial.println("BME680: endReading() failed, restarting");
                } else {
                    addBME680Reading(
                        bme680.temperature,
                        bme680.humidity,
                        bme680.pressure / 100.0f,
                        bme680.gas_resistance
                    );
                    printLatestBME680();
                }

                // Always attempt to start next reading
                uint32_t endTime = bme680.beginReading();
                if (endTime == 0) {
                    Serial.println("BME680: beginReading() failed");
                } else {
                    Serial.print("BME680: next reading ready at t=");
                    Serial.println(endTime);
                }

                mutexTakenBy = "none";
                xSemaphoreGive(i2cMutex);
            } else {
                Serial.println("BME680: mutex timeout");
            }
        }

    // Wait until the next BME680 update interval
    vTaskDelayUntil(&lastWake, interval);
  }
}