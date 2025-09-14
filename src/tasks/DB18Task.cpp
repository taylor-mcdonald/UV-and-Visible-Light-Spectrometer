#include "DB18Task.h"

DS18B20 ds(DS18B20_PIN);  //The DS18B20 is on GPIO7

TaskHandle_t ds18taskhandle;

void initDS18B20Sensor() {
  ds.setResolution(12); // Set the resolution to 12 bits (0.0625°C)
  Serial.println("DS18B20 sensor initialized");
}

void startDS18B20Task() {
     xTaskCreatePinnedToCore(
        DS18B20sensorTask,     // Function that implements the task.
        "DS18B20 Task",        // Text name for the task.
        4096,                  // Stack size in words, not bytes.
        NULL,                  // Parameter passed into the task.
        1,                     // Priority at which the task is created.
        &ds18taskhandle,       // Used to pass out the created task's handle.
        tskNO_AFFINITY         // Run on any core.
    );
}

void DS18B20sensorTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(DS18_UPDATE_MS); // 1000 ms
  for (;;) {
      // --- Read UV sensor here ---
      addDS18Reading(ds.getTempC());
      //Serial.println("DS18 Tempurature data read and stored");
  }
  
  // Wait until the next 500 ms boundary
  vTaskDelayUntil(&lastWake, interval);
}
