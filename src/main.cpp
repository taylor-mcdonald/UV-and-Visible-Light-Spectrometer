/*
Devices:  ESP32-C3 - Bluetooth and WiFi enabled microcontroller
          GPIO4 = SDA
          GPIO5 = SCL

          GPIO7 = DS18B20 One Wire
          DS18B20 - Temp Sensor

          AS7331 Spectral UV Sensor (Sparkfun breakout board) - I2C Comm. 
            https://github.com/sparkfun/SparkFun_AS7331_Arduino_Library 
            I2C Address = 0x74 (default but is adjustable)

          AS7341 10-Channel Light and Color Sensor (DEV BOARD) - I2C Comm
            https://www.amazon.com/dp/B0DBQKDV67?ref=ppx_yo2ov_dt_b_fed_asin_title
            This DEV board can supposedly tolerate 3.3V and 5V I2C signals due to it's on-board voltage regulator.
            This build connects the AS7341's VIN pin to the 3.3V pin on the ESP32-C3 and uses 3.3V I2C signals.
            I2C Address = 0x39
            https://github.com/adafruit/Adafruit_AS7341
          
          AHT21 Temp and Humidity Sensor - I2C Comm
            I2C Address = 

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

FUTURE PROJECT GOALS:
1. Switch the AHT21 for the BME680.  Possibly remove the DS18B20 too.
2. Enable bluetooth connectivity to an android app and send all data there.

          
*/
#include <config.h>
#include <tasks/ScreenTask.h>
#include <tasks/UVSensorTask.h>
#include <tasks/SpectralTask.h>
#include <tasks/AHTTask.h>
#include <tasks/DB18Task.h>
#include <tasks/ButtonTask.h>
#include <shared/SharedData.h>


void setup() {
  Serial.begin(115200);
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN); // Initialize I2C with custom pins

  ScreenDisplay = 0;
  initScreen();

  initDS18B20Sensor();
  Serial.println("DS18B20 sensor initialized.");

    // //***********************************************************************************//
  initAHT21Sensor();
  Serial.println("AHT21 sensor initialized.");
  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("AHT21 Sensor Ready");
  display.display();
  // //  ******************************************************************************//

  initUVSensor();
  
  initAS7341Sensor();
  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("AS7341 Sensor Ready");
  display.display();

  //testAS7341_INT_simple();


  // --- FreeRTOS tasks ---
  startScreenTask();
  startUVSensorTask();
  startAHTTask();
  startDS18B20Task();
  startSpectralTasks();
  startButtonTasks();

  initButtons();
  Serial.println("Button interrupts initialized.");

  initUVSensorInterrupt();
  initAS7341interrupt();
  
}

void loop() {
}