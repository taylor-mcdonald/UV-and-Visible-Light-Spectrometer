#include "AHTTask.h"


Adafruit_AHTX0 aht; // Create an instance of the AHTX0 sensor object

TaskHandle_t ahtTaskhandle = nullptr;

void initAHT21Sensor() {
  if (!aht.begin()) {
    Serial.println("Could not find AHT21 sensor");
    while (1); 
  }
  Serial.println("AHT21 sensor initialized");
}

void startAHTTask() {
     xTaskCreatePinnedToCore(
        AHT21sensorTask,     // Function that implements the task.
        "AHT21 Sensor Task", // Text name for the task.
        2048,                // Stack size in words, not bytes.
        NULL,                // Parameter passed into the task.
        1,                   // Priority at which the task is created.
        &ahtTaskhandle,      // Used to pass out the created task's handle.
        tskNO_AFFINITY       // Run on any core.
    );
}


void AHT21sensorTask(void *pvParameters) {

  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(AHT_UPDATE_MS);

  for (;;) {
    //AHT21 Example Code
    // Create sensor event objects to store data
    sensors_event_t humidity, temp;

    // Get new data from the AHT21 sensor
    aht.getEvent(&humidity, &temp);
    addAHT21Reading(humidity.relative_humidity, temp.temperature);

    //Serial.println("AHT21 data read and stored");

    // Wait until the next 500 ms boundary
    vTaskDelayUntil(&lastWake, interval);
  }
}

