/*
Branch 0.4.0 will implement:
1. The BME680 and remove AHT21 and the DS18B20.  Change the OLED display to show the BME680's Temp, Humidity, 
   Pressure, and Air Quality Index readings instead of the AHT21 and DS18B20 readings. 
2. Enable the BLE server and NOTIFY an android app of all pertinent data.

Devices:  ESP32-C3 - Bluetooth and WiFi enabled microcontroller
            GPIO4 = SDA
            GPIO5 = SCL

            GPIO2 = AS7341 Interrupt pin
            GPIO3 = AS7331 Interrupt pin

            GPIO8 = FWD Button
            GPIO9 = BK Button

          REMOVED: GPIO7 = DS18B20 One Wire
          REMOVED: DS18B20 - Temp Sensor

          AS7331 Spectral UV Sensor (Sparkfun breakout board) - I2C Comm. 
            https://github.com/sparkfun/SparkFun_AS7331_Arduino_Library 
            I2C Address = 0x74 (default but is adjustable)

          AS7341 10-Channel Light and Color Sensor (DEV BOARD) - I2C Comm
            https://www.amazon.com/dp/B0DBQKDV67?ref=ppx_yo2ov_dt_b_fed_asin_title
            This DEV board can supposedly tolerate 3.3V and 5V I2C signals due to it's on-board voltage regulator.
            This build connects the AS7341's VIN pin to the 3.3V pin on the ESP32-C3 and uses 3.3V I2C signals.
            I2C Address = 0x39
            https://github.com/adafruit/Adafruit_AS7341
          
          REMOVED: AHT21 Temp and Humidity Sensor - I2C Comm
          REMOVED:  I2C Address = 

          BME680 Temp, Humidity, Pressure, and Air Quality Sensor - I2C Comm
            https://www.amazon.com/dp/B08Z3LZ9Q6?ref=ppx_yo2ov_dt_b_fed_asin_title
            I2C Address = 0x76 (default but is adjustable)

          OLED I2C IIC Display #1 - I2C Comm
            Address = 0x3C
            Resolution = 128 x 64
            SSD1306 Display Driver          

          Battery charging/monitoring hardware
            A 18650 and a a 3.7V/4.2V to 5V/9V 2A Adjustable Boost Converter Module, adjusted to 5V output.
            (this should probably re-evaluated to a something that just outputs 3.3V, but it is what was available)

Project description:  
Obtain UV-A, UV-B, and UV-C readings from the AS7331 via I2C.
Obtain Visible spectrum readings from the AS7341 via I2C.
Obtain Temp and Humidity readings from the AHT21
Obtain Temp readings from the DS18B20

Display all of this information on the OLED screen.
Use the FWD/BACK buttons to change what info is displayed



          
*/
#include <config.h>
#include <tasks/ScreenTask.h>
#include <tasks/UVSensorTask.h>
#include <tasks/SpectralTask.h>
#include <tasks/BME680Task.h>
#include <tasks/ButtonTask.h>
#include <shared/SharedData.h>
#include <shared/I2CBus.h>


void mutexWatchdogTask(void *pvParameters);

void setup() {
  Serial.begin(115200);
  i2cMutex = xSemaphoreCreateMutex();  // must be first
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN); // Initialize I2C with custom pins
  //Wire.setClock(100000);  // slow bus down for reliability

  ScreenDisplay = 0;
  initScreen();

  // --- Initialize sensors ---

  //  ******************************************************************************//
  initBME680Sensor(Wire);
  Serial.println("BME680 sensor initialized.");

  initUVSensor();
  
  initAS7341Sensor();

  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("BME680 Sensor Ready");
  display.display();
  delay(500);

  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("Spectral Sensor Ready");
  display.display();
  delay(500);

  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("UV Sensor Ready");
  display.display();
  delay(500);

  // --- Settle before tasks start ---
   delay(500);
  // --- FreeRTOS tasks ---
  startScreenTask();
  startUVSensorTask();
  startBME680Tasks();
  startSpectralTasks();
  startButtonTasks();

  xTaskCreatePinnedToCore(mutexWatchdogTask, "MutexWatchdog", 2048, NULL, 2, NULL, tskNO_AFFINITY);

  // Start ISRs for buttons and sensors
  initButtons();
  Serial.println("Button interrupts initialized.");

  initUVSensorInterrupt();
  initAS7341interrupt();
  
}

void loop() {
}

void mutexWatchdogTask(void *pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (strcmp((const char*)mutexTakenBy, "none") != 0) {
            uint32_t heldFor = millis() - mutexTakenAt;
            if (heldFor > 300) {  // held for more than 300ms is suspicious
                Serial.print("WARNING: mutex held by ");
                Serial.print((const char*)mutexTakenBy);
                Serial.print(" for ");
                Serial.print(heldFor);
                Serial.println("ms");
            }
        }
    }
}