#include "UVSensorTask.h"

// Task handle
TaskHandle_t UV_TaskHandle = nullptr;
TaskHandle_t UV_Start_TaskHandle = nullptr;

// ISR (notify task)
void IRAM_ATTR onUVSensorReady() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(UV_TaskHandle, &xHigherPriorityTaskWoken);
  
  // Yield to higher priority task if needed
  if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();  // ESP32 version takes no args
  }
}

// void IRAM_ATTR onUVSensorReady() {
//   UVsensorReadFlag = true; // set flag for AS7331 sensor task
// }

SfeAS7331ArdI2C uvSensor; // Create an instance of the sensor class

void initUVSensor() {

  //// AS7331 Sensor Initialization  ***************************************************//
  // Initialize sensor and run default setup.
  if (uvSensor.begin() == false) {
    Serial.println("Sensor failed to begin. Please check your wiring!");
    Serial.println("Halting...");
    while (1);
  }

  Serial.println("AS7331 UV Sensor began.");

  // Set measurement mode and change device operating mode to measure.
  if (uvSensor.prepareMeasurement(MEAS_MODE_CMD) == false) {
    Serial.println("Sensor did not get set properly.");
    Serial.println("Halting...");
    while (1);
  }

  Serial.println("Set mode to command.");

  //Serial.println("Set mode to continuous. Starting measurement...");

  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("UV Sensor Ready");
  display.display();
  delay(1000);
  // //***********************************************************************************//
  return;
}

void startUVSensorTask() {
  xTaskCreatePinnedToCore(
      UVsensorTask,           // Function that implements the task.
      "UV Sensor Task",       // Text name for the task. 
      4096,                   // Stack size in words, not bytes.
      NULL,                   // Parameter passed into the task.
      1,                      // Priority at which the task is created.
      &UV_TaskHandle,         // Used to pass out the created task's handle.
      tskNO_AFFINITY          // Run on any core. 
  );

    xTaskCreatePinnedToCore(
      startUVsensorMeasurementTask,   // Function that implements the task.
      "start UV measurement Task",    // Text name for the task. 
      2048,                           // Stack size in words, not bytes.
      NULL,                           // Parameter passed into the task.
      1,                              // Priority at which the task is created.
      &UV_Start_TaskHandle,           // Used to pass out the created task's handle.
      tskNO_AFFINITY                  // Run on any core. 
  );
  return;
}

void initUVSensorInterrupt() {
    //Assign the AS7331 interrupt output to work as an interrupt on the ESP32
  pinMode(UV_RDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(UV_RDY_PIN), onUVSensorReady, RISING);
}

void UVsensorTask(void *pvParameters) {
  for (;;) {
    // if (UVsensorReadFlag) {
    //   UVsensorReadFlag = false;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification from ISR

      // Read all UV channels
 
     if (ksfTkErrOk != uvSensor.readAllUV())
        Serial.println("Error reading UV.");

      // --- Read UV sensor here ---
      float uva = uvSensor.getUVA();
      float uvb = uvSensor.getUVB();
      float uvc = uvSensor.getUVC();
      addUVReading(uva, uvb, uvc);

      //Serial.println("UV data read and stored");
      printLatestUV();
    }
    //vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
}

void startUVsensorMeasurementTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(UV_MEASUREMENT_INTERVAL); // 750 ms

  for (;;) {
    // Begin measurement.
    if (ksfTkErrOk != uvSensor.setStartState(true))
      Serial.println("Error starting reading!");

    vTaskDelayUntil(&lastWake, interval);
  }
}
