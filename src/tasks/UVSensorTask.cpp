#include "UVSensorTask.h"

void IRAM_ATTR onUVSensorReady() {
  UVsensorReadFlag = true; // set flag for AS7331 sensor task
}

SfeAS7331ArdI2C uvSensor; // Create an instance of the sensor class


void initUVSensor() {

//// AS7331 Sensor Initialization  ***************************************************//
  // Initialize sensor and run default setup.
  
  //Assign the AS7331 interrupt output to work as an interrupt on the ESP32
  pinMode(UV_RDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(UV_RDY_PIN), onUVSensorReady, RISING);

  // Initialize sensor and run default setup.
  if (uvSensor.begin() == false)
  {
      Serial.println("Sensor failed to begin. Please check your wiring!");
      Serial.println("Halting...");
      while (1)
          ;
  }

  Serial.println("AS7331 UV Sensor began.");

  // Set the delay between measurements so that the processor can read out the
  // results without interfering with the ADC.
  // Set break time to 900us (112 * 8us) to account for the time it takes to poll data.
  // if (ksfTkErrOk != uvSensor.setBreakTime(255))
  // {
  //     Serial.println("Sensor did not set break time properly.");
  //     Serial.println("Halting...");
  //     while (1)
  //         ;
  // }

  // // Set measurement mode and change device operating mode to measure.
  // if (uvSensor.prepareMeasurement(MEAS_MODE_CONT) == false)
  // {
  //     Serial.println("Sensor did not get set properly.");
  //     Serial.println("Spinning...");
  //     while (1)
  //         ;
  // }


  // Set measurement mode and change device operating mode to measure.
  if (uvSensor.prepareMeasurement(MEAS_MODE_CMD) == false) {
    Serial.println("Sensor did not get set properly.");
    Serial.println("Halting...");
    while (1);
    }

  Serial.println("Set mode to command.");

  Serial.println("Set mode to continuous. Starting measurement...");

  // Begin measurement.
  if (ksfTkErrOk != uvSensor.setStartState(true))
      Serial.println("Error starting reading!");

  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("UV Sensor Ready");
  display.display();
  delay(1000);
  // //***********************************************************************************//

}

void startUVSensorTask() {
    xTaskCreatePinnedToCore(
        UVsensorTask,           // Function that implements the task.
        "UV Sensor Task",       // Text name for the task. 
        4096,                   // Stack size in words, not bytes.
        NULL,                   // Parameter passed into the task.
        1,                      // Priority at which the task is created.
        NULL,                   // Used to pass out the created task's handle.
        tskNO_AFFINITY          // Run on any core. 
    );
}

void UVsensorTask(void *pvParameters) {
  for (;;) {
    if (UVsensorReadFlag) {
      UVsensorReadFlag = false;
 
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
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
  }
}