/*
Branch 0.5.0 will implement:
1. Migrate from the ESP32-C3 to the ESP32-S3 for true dual I2C bus support and dedicated vector math for FFT processing.
2. Migrate AS7341 to dedicated I2C Bus 1 (GPIO6=SCL1, GPIO7=SDA1) to eliminate
   bus contention and enable high-speed FIFO capture for flicker analysis.
2. Remove dead PWM code and GPIO1 references.
3. Replace MAX17043 breakout with Adafruit MAX17048 breakout.
4. Implement AS7341 flicker detection via scheduled FIFO capture, streaming
   raw samples to the Android app for FFT analysis.
5. Implement BLE time sync from Android app via writable characteristic.
6. Implement local data logging of 5-minute sensor averages in ESP32 NVS flash.
7. Implement BLE missed data sync on reconnect.
8. Code cleanup: remove debug Serial.println, mutex watchdog, register dumps,
   and I2C scanner from production code.

Devices:  ESP32-S3 - Bluetooth enabled microcontroller
            I2C Bus 0:
              GPIO5 = SDA0
              GPIO6 = SCL0

            I2C Bus 1 (AS7341 dedicated):
              GPIO10 = SDA1
              GPIO9 = SCL1

            GPIO4  = AS7331 Interrupt pin
            GPIO7  = FWD Button
            GPIO8  = BK Button
            GPIO11 = AS7341 Interrupt pin

          AS7331 Spectral UV Sensor (Sparkfun breakout board) - I2C Bus 0
            https://github.com/sparkfun/SparkFun_AS7331_Arduino_Library
            I2C Address = 0x39 (default but is adjustable)

          AS7341 10-Channel Light and Color Sensor (DEV BOARD) - I2C Bus 1
            https://www.amazon.com/dp/B0DBQKDV67?ref=ppx_yo2ov_dt_b_fed_asin_title
            This DEV board can supposedly tolerate 3.3V and 5V I2C signals due to
            its on-board voltage regulator. This build connects the AS7341's VIN pin
            to the 3.3V pin on the ESP32-C3 and uses 3.3V I2C signals.
            I2C Address = 0x74
            https://github.com/adafruit/Adafruit_AS7341

          BME680 Temp, Humidity, Pressure, and Air Quality Sensor - I2C Bus 0
            https://www.amazon.com/dp/B08Z3LZ9Q6?ref=ppx_yo2ov_dt_b_fed_asin_title
            I2C Address = 0x76 (default but is adjustable)
            https://github.com/adafruit/Adafruit_BME680

          MAX17048 Battery Fuel Gauge - I2C Bus 0
            https://www.adafruit.com/product/5580
            I2C Address = 0x36
            https://github.com/adafruit/Adafruit_MAX1704X

          OLED Display - I2C Bus 0
            I2C Address = 0x3C
            Resolution = 128x64
            SSD1306 Display Driver

          Power Supply:
            18650 Li-Ion cell
            TP4057 LiPo/Li-Ion charger breakout (USB-C charging)
            Adafruit LM3671 3.3V buck converter (note: dropout at ~3.4V,
            bottom 20-30% of battery capacity unavailable - replace with
            buck-boost converter on final PCB)
            Adafruit MAX17048 fuel gauge (replaces previous MAX17043 breakout)

Project description:
Obtain UV-A, UV-B, and UV-C readings from the AS7331 via I2C Bus 0.
Obtain visible spectrum readings from the AS7341 via I2C Bus 1.
Obtain flicker frequency data from AS7341 FIFO via I2C Bus 1.
Obtain Temp, Pressure, Humidity, and Gas sensor readings from the BME680 via I2C Bus 0.
Obtain battery voltage and state of charge from the MAX17048 via I2C Bus 0.
Display all sensor data on the OLED screen.
Use FWD/BACK buttons to cycle through display pages.
Transmit all sensor data to Android app via BLE NOTIFY characteristics.
Log 5-minute sensor averages to NVS flash for sync on BLE reconnect.
*/

          

#include <config.h>
#include <tasks/ScreenTask.h>
#include <tasks/UVSensorTask.h>
#include <tasks/SpectralTask.h>
#include <tasks/BME680Task.h>
#include <tasks/ButtonTask.h>
#include <shared/SharedData.h>
#include <shared/I2CBus.h>
#include <tasks/BLETask.h>
#include <tasks/FuelGaugeTask.h>
#include <Wire.h>



void mutexWatchdogTask(void *pvParameters);



void setup() {
  delay(3000); // wait for usb CDC to enumerate
  Serial.begin(115200);
  delay(3000);
  Serial.println("BOOT");
  Serial.flush();
  
  i2cMutex = xSemaphoreCreateMutex();  // must be first
  i2cMutex1 = xSemaphoreCreateMutex();  // for second I2C bus
  
  delay(1000);
  Serial.println("Starting UV and Visible Light Spectrometer...");

  bool bus0ok = Wire.begin(CUSTOM_SDA0_PIN, CUSTOM_SCL0_PIN); // Initialize I2C bus 0 with custom pins
  Wire.setClock(100000);  // slow bus down for reliability
  Serial.print("Wire.begin() returned: ");
  Serial.println(bus0ok ? "true" : "false");

  i2cBusScan(Wire, "Bus 0"); // Scan bus 0 for devices

  delay(1000); // short delay to ensure bus is ready before scanning

  bool bus1ok = Wire1.begin(CUSTOM_SDA1_PIN, CUSTOM_SCL1_PIN);
  Serial.print("Wire1.begin() returned: ");
  Serial.println(bus1ok ? "true" : "false");
  Wire1.setClock(400000);

  i2cBusScan(Wire1, "Bus 1"); // Scan bus 1 for devices
 
  // Wire1.begin(CUSTOM_SDA1_PIN, CUSTOM_SCL1_PIN); // Initialize I2C bus 1 with custom pins
  // Wire1.setClock(400000);  // We need speed!
  initBLE();          // before starting tasks

  ScreenDisplay = 0;
  initScreen();  // The I2C bus is set in the .cpp file and defaults to Wire.

  // --- Initialize sensors ---

  //  ******************************************************************************//
  initBME680Sensor(Wire);
  Serial.println("BME680 sensor initialized.");

  initUVSensor(Wire);
  
  initAS7341Sensor(Wire1);

  //initFuelGauge(Wire);

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

  //startFuelGaugeTask();
  startBLETask();     // last

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