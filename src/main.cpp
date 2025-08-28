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

#include <SparkFun_AS7331.h>
#include <DS18B20.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h> // Include the Adafruit AHTX0 library for AHT21
#include <Adafruit_AS7341.h> // Include the Adafruit AS7341 library



DS18B20 ds(7);  //The DS18B20 is on GPIO7
Adafruit_AHTX0 aht; // Create an instance of the AHTX0 sensor object
SfeAS7331ArdI2C uvSensor; // Create an instance of the sensor class
Adafruit_AS7341 as7341; // Create an instance of the AS7341 sensor object

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3D ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define SDA and SCL pins
#define CUSTOM_SDA_PIN 4
#define CUSTOM_SCL_PIN 5

// Define button pins
#define FWD_button 8
#define BK_button 9

// ====== CONFIG ======
#define DRDY_PIN 3        // GPIO for AS7331 Data Ready interrupt
#define SCREEN_UPDATE_MS 500   //
#define SENSOR_TASK_DELAY 50   // safety delay if needed
#define AHT_UPDATE_MS 1000
#define DS_UPDATE_MS 1000
#define debounce_delay 150

volatile unsigned long last_interrupt_time = 0; // Global or static in ISR

// ====== FLAGS ======
volatile bool screenUpdateFlag = false;
volatile bool UVsensorReadFlag = false;
volatile bool AHT21sensorReadFlag = false;
volatile bool DS18B20sensorReadFlag = false;
volatile bool FWD_buttonReadFlag = false;
volatile bool BK_buttonReadFlag = false;
volatile bool AS7341sensorReadFlag = false;

// ====== TIMER HANDLES ======
hw_timer_t *screenTimer = NULL;
hw_timer_t *AHT_Timer = NULL;
hw_timer_t *DS_Timer = NULL;

// ====== ISR FUNCTIONS ======
void IRAM_ATTR onScreenTimer() {
  screenUpdateFlag = true;   // set flag for screen task
}

void IRAM_ATTR onUVSensorReady() {
  UVsensorReadFlag = true; // set flag for AS7331 sensor task
  AS7341sensorReadFlag = true; // set flag for AS7341 sensor task
}

void IRAM_ATTR onFWD_button_detect() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > debounce_delay) {
    // This is a valid interrupt, perform desired action
    FWD_buttonReadFlag = true;     // set flag for sensor task
    last_interrupt_time = current_time; // Update last accepted time
  }
}

void IRAM_ATTR onBK_button_detect() {
  unsigned long current_time = millis();
  if (current_time - last_interrupt_time > debounce_delay) {
    BK_buttonReadFlag = true;     // set flag for sensor task
    last_interrupt_time = current_time; // Update last accepted time
  }
}

void IRAM_ATTR AHTSensorReady() {
  AHT21sensorReadFlag = true;     // set flag for sensor task
  DS18B20sensorReadFlag = true;
}

void IRAM_ATTR DS18SensorReady() {
  DS18B20sensorReadFlag = true;     // set flag for sensor task
}

// ================== Global Variables ==================
struct UVReading {
  float uva;
  float uvb;
  float uvc;
  float uvIndex;
  unsigned long UV_timestamp;
};

struct AS7341Reading {
  uint16_t F1;	// 415nm -> Violet
  uint16_t F2;	// 445nm -> Indigo-Blue
  uint16_t F3;  // 480nm -> Blue
  uint16_t F4;	// 515nm -> Green
  uint16_t F5;  // 555nm -> Yellow-Green
  uint16_t F6;  // 590nm -> Yellow-Orange
  uint16_t F7;  // 630nm -> Red
  uint16_t F8;  // 680nm -> more Red
  uint16_t NIR; // Near Infrared
  uint16_t Clr; // Clear Channel	
  uint16_t FLKR;// Flicker Detection	
  unsigned long AS7341_timestamp;
};

struct AHT21Reading {
  float temp;
  float humid;
  unsigned long AHT_timestamp;
};

struct DS18Reading {
  float DS_temp;
  unsigned long DS_timestamp;
};

//  Index to define which screen is to be drawn on the display
//  0 - UV Raw Data
//  1 - UV Index
//  2 - AHT21 Data
//  3 - DS18B20 Data
//  4 - AS7341 Data (visible spectrum)
//  5 - Bar Chart of AS7341 and UV Index
volatile int ScreenDisplay;
#define NumOfScreens 6

// Create a rotating history of all sensor readings
#define UVHISTORY_SIZE 120
UVReading UVhistory[UVHISTORY_SIZE];
int UVhistoryIndex = 0;
UVReading UV_latest;

#define AS7341_HISTORY_SIZE 120
AS7341Reading AS7341_history[AS7341_HISTORY_SIZE];
int AS7341_historyIndex = 0;
AS7341Reading AS7341_latest;

#define AHTHISTORY_SIZE 120
AHT21Reading AHThistory[AHTHISTORY_SIZE];
int AHThistoryIndex = 0;
AHT21Reading AHT_latest;

#define DS18HISTORY_SIZE 120
DS18Reading DS18history[DS18HISTORY_SIZE];
int DS18historyIndex = 0;
DS18Reading DS_latest;

// ================== Prototypes ==================
void screenTask(void *pvParameters);
void FWD_buttonTask(void *pvParameters);
void BK_buttonTask(void *pvParameters);
void UVsensorTask(void *pvParameters);
void AS7341sensorTask(void *pvParameters);
void AHT21sensorTask(void *pvParameters);
void DS18B20sensorTask(void *pvParameters);
void addUVReading(float uva, float uvb, float uvc);
float calculateUVIndex(float uva, float uvb);
void addAHT21Reading(float hmd, float tmp);
void addDS18Reading(float tmp);
void addAS7341Reading(uint16_t F1, uint16_t F2, uint16_t F3, uint16_t F4, uint16_t F5, uint16_t F6, uint16_t F7, uint16_t F8, uint16_t NIR, uint16_t Clr, uint16_t FLKR);
void printLatestUV();
void printLatestAHT();
void printLatestDS18();


// 'UV Icon', 32x32px
extern const unsigned char UV_Icon_bmp[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 
	0x00, 0x01, 0x80, 0x00, 0x03, 0x01, 0x80, 0x40, 0x03, 0x81, 0x81, 0xc0, 0x01, 0xc0, 0x03, 0x80, 
	0x00, 0xc3, 0xc3, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3f, 0xfc, 0x00, 
	0x00, 0x3f, 0xfc, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x3f, 0x7f, 0xfe, 0xfc, 0x3f, 0x7f, 0xfe, 0xfc, 
	0x00, 0x7f, 0xfe, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x1f, 0xf8, 0x00, 
	0x00, 0x0f, 0xf0, 0x00, 0x00, 0x47, 0xe2, 0x00, 0x01, 0xc0, 0x03, 0x80, 0x01, 0x80, 0x01, 0xc0, 
	0x03, 0x00, 0x00, 0xc0, 0x00, 0x22, 0x44, 0x00, 0x00, 0x22, 0x4c, 0x00, 0x00, 0x22, 0x68, 0x00, 
	0x00, 0x22, 0x38, 0x00, 0x00, 0x36, 0x30, 0x00, 0x00, 0x1c, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'humidity', 32x32px
extern const unsigned char humidity_bmp[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 
	0x00, 0x60, 0x00, 0x00, 0x00, 0xf0, 0x60, 0x00, 0x00, 0xf0, 0x60, 0x00, 0x01, 0xf8, 0xf0, 0x00, 
	0x03, 0xf8, 0xf8, 0x00, 0x03, 0xfd, 0xf8, 0x00, 0x07, 0xf9, 0xfc, 0x00, 0x07, 0xf3, 0xfc, 0x00, 
	0x0f, 0xf3, 0xfe, 0x00, 0x0f, 0xe7, 0xfe, 0x00, 0x0f, 0xef, 0xff, 0x00, 0x0f, 0xcf, 0xff, 0x00, 
	0x0f, 0xdf, 0xff, 0x80, 0x0f, 0x9f, 0xff, 0x80, 0x0f, 0xbf, 0xc0, 0x80, 0x07, 0xbf, 0xde, 0x30, 
	0x03, 0xbf, 0x9b, 0x30, 0x00, 0x3f, 0xb3, 0x60, 0x00, 0x3f, 0x9e, 0xc0, 0x00, 0x3f, 0xde, 0xc0, 
	0x00, 0x1f, 0xe1, 0xb8, 0x00, 0x1f, 0xfb, 0x7c, 0x00, 0x0f, 0xf3, 0x6c, 0x00, 0x07, 0xf6, 0x6c, 
	0x00, 0x03, 0xee, 0x3c, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'Thermometer2', 32x32px
extern const unsigned char thermometer_bmp[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x04, 0x40, 0x00, 
	0x00, 0x08, 0x20, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x20, 0x00, 0x00, 0x04, 0x40, 0x00, 
	0x00, 0x08, 0x20, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x08, 0x20, 0x00, 0x00, 0x05, 0x40, 0x00, 
	0x00, 0x0b, 0xa0, 0x00, 0x00, 0x05, 0xc0, 0x00, 0x00, 0x0b, 0xa0, 0x00, 0x00, 0x05, 0x40, 0x00, 
	0x00, 0x0b, 0xa0, 0x00, 0x00, 0x05, 0xc0, 0x00, 0x00, 0x0b, 0xa0, 0x00, 0x00, 0x05, 0x40, 0x00, 
	0x00, 0x0b, 0xa0, 0x00, 0x00, 0x13, 0x90, 0x00, 0x00, 0x2f, 0xe8, 0x00, 0x00, 0x2f, 0xe0, 0x00, 
	0x00, 0x2f, 0xe8, 0x00, 0x00, 0x0f, 0xe8, 0x00, 0x00, 0x2f, 0xe8, 0x00, 0x00, 0x17, 0xd0, 0x00, 
	0x00, 0x08, 0xa8, 0x00, 0x00, 0x04, 0x40, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'µW/cm²', 32x32px
const unsigned char microwatt_bmp[] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x0c, 0x30, 0xc0, 0x00, 0x06, 0x38, 0xc0, 0x00, 0x06, 0x38, 0xc0, 0x04, 0x26, 0x69, 0x80, 
	0x04, 0x26, 0x69, 0x80, 0x04, 0x23, 0x4d, 0x80, 0x04, 0x23, 0xc5, 0x80, 0x04, 0x63, 0xc7, 0x00, 
	0x06, 0x61, 0x87, 0x00, 0x07, 0xb1, 0x87, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 
	0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x03, 0xc5, 0xde, 0x40, 
	0x07, 0x66, 0xf6, 0xc0, 0x06, 0x24, 0x62, 0x00, 0x04, 0x04, 0x62, 0x00, 0x04, 0x04, 0x62, 0x00, 
	0x06, 0x24, 0x62, 0x00, 0x03, 0xe4, 0x62, 0x00, 0x01, 0xc4, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(115200);
  Wire.begin(CUSTOM_SDA_PIN, CUSTOM_SCL_PIN); // Initialize I2C with custom pins

  // OLED Init *******************************************//
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(10, 28);
  display.println("Screen Ready");
  display.display();
  delay(3000);
  
  ScreenDisplay = 0;

  // // AS7331 Sensor Initialization  ***************************************************//
  // Initialize sensor and run default setup.
  //Assign the AS7331 interrupt output to work as an interrupt on the ESP32
  pinMode(DRDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DRDY_PIN), onUVSensorReady, RISING);

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
  

  // // Adafruit AS7341 Sensor Initialization *******************************************//
  if (!as7341.begin()){
    Serial.println("Could not find AS7341");
    while (1) { delay(10); }
  }

  as7341.setATIME(100); // 100ms integration time
  as7341.setASTEP(999); // sets the integration time to 100ms
  as7341.setGain(AS7341_GAIN_256X); // set a high gain
  
  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("AS7341 Sensor Ready");
  display.display();
  Serial.println("AS7341 Sensor Read");
  delay(1000);
  // //***********************************************************************************//

  // // Attempt to initialize the AHT21 sensor and set read interupt timer*****************************************//
  if (!aht.begin()) {
    Serial.println("AHT21 sensor not found. Check connections.");
    while (1); // Halt execution if sensor not found
  }
  Serial.println("AHT21 sensor initialized.");
  display.clearDisplay();
  display.setCursor(10, 28);
  display.println("AHT21 Sensor Ready");
  display.display();
  // //  ******************************************************************************//

  // Set forward and back button interrupts
  pinMode(FWD_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FWD_button), onFWD_button_detect, FALLING);
  pinMode(BK_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BK_button), onBK_button_detect, FALLING);

  Serial.println("Button interrupts initialized.");

  // --- Hardware timer for screen updates ---
   // timerBegin(timer_num, prescaler, countUp)
  // timer_num: 0-3 available
  // prescaler: clock divider
  // countUp: true for up-counting
  screenTimer = timerBegin(1, 80, true); //

    // Attach interrupt (third arg = edge triggered or level triggered)
  timerAttachInterrupt(screenTimer, &onScreenTimer, true);

  // timerAlarmWrite(timer, compare_value, auto_reload)
  // Here we want e.g. 500 ms update → 500,000 µs
  timerAlarmWrite(screenTimer, SCREEN_UPDATE_MS*1000, true);
  // 80 prescaler -> 1 tick = 1 µs (assuming 80 MHz APB clock)
  timerAlarmEnable(screenTimer); // Enable the alarm
  Serial.println("screen timer interrupt initialized.");



  AHT_Timer = timerBegin(0, 80, true); //
  timerAttachInterrupt(AHT_Timer, &AHTSensorReady, true);
  timerAlarmWrite(AHT_Timer, AHT_UPDATE_MS*1000, true);
  timerAlarmEnable(AHT_Timer); // Enable the alarm
  Serial.println("AHT21 timer interrupt initialized.");

  // DS_Timer = timerBegin(1000); //
  // timerAttachInterrupt(DS_Timer, &DS18SensorReady);
  // timerAlarm(DS_Timer, DS_UPDATE_MS, true, 0);

  // --- FreeRTOS tasks ---
  xTaskCreatePinnedToCore(screenTask, "Screen Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(UVsensorTask, "UV Sensor Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(AHT21sensorTask, "AHT21 Sensor Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(FWD_buttonTask, "Forward Button Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(BK_buttonTask, "Back Button Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(DS18B20sensorTask, "DS18 Sensor Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(AS7341sensorTask, "AS7341 Sensor Task", 4096, NULL, 1, NULL, tskNO_AFFINITY);
}

void loop() {
}


// ====== TASKS ======
void screenTask(void *pvParameters) {
  for (;;) {
    if (screenUpdateFlag) {
      screenUpdateFlag = false;
      
      //Serial.print("About to update screen: ");
      //Serial.println(ScreenDisplay); 
      display.clearDisplay();

    
      switch(ScreenDisplay) {
        // UV Raw Data Screen
        case 0: {
          UV_latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
          //Serial.print("UVA:");
          //Serial.print(UV_latest.uva);
          //Serial.print(" UVB:");
          //Serial.print(UV_latest.uvb);
          //Serial.print(" UVC:");
          //Serial.println(UV_latest.uvc);
          // Display UV Readings on the OLED Display for 2 seconds

          // UV Icon
          display.drawBitmap(0, 0, UV_Icon_bmp, 32, 32, SSD1306_WHITE);
          display.drawBitmap(0, 33, microwatt_bmp, 32, 32, SSD1306_WHITE);
          display.setCursor(34, 4);
          display.setTextSize(1);
          display.print("UV-A:");
          display.print(UV_latest.uva);
          //display.print("µW/cm²");
          
          display.setCursor(34, 24);
          display.setTextSize(1);
          display.print("UV-B:");
          display.print(UV_latest.uvb);
          //display.print("µW/cm²");

          display.setCursor(34, 44);
          display.setTextSize(1);
          display.print("UV-C:");
          display.print(UV_latest.uvc);
          //display.print("µW/cm²");
          break;
        };
        
        // UV Index Screen
        case 1: {
          UV_latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
          display.setCursor(0, 10);
          display.setTextSize(1);
          display.print("UV Index:");
          display.setCursor(24, 32);
          display.setTextSize(2);
          display.print(UV_latest.uvIndex);
          break;
        };

        // AHT21 Data Screen
        case 2: {
          AHT_latest = AHThistory[(AHThistoryIndex - 1 + AHTHISTORY_SIZE) % AHTHISTORY_SIZE];
          // Print temperature data
          //Serial.print("Temperature: ");
          //Serial.print(AHT_latest.temp);
          //Serial.println(" °C");

          // Print humidity data
          //Serial.print("Humidity: ");
          //Serial.print(AHT_latest.humid);
          //Serial.println(" %");

          // Display AHT21 temp & Humidity on the OLED Display for 2 seconds
          display.clearDisplay();
          // Thermometer Icon
          display.drawBitmap(0, 0, thermometer_bmp, 32, 32, SSD1306_WHITE);
          display.setCursor(24, 4);
          display.setTextSize(1);
          display.print("Temp (C): ");
          //display.setTextSize(2);
          display.print(AHT_latest.temp);

          // Droplet Icon
          display.drawBitmap(0, 32, humidity_bmp, 32, 32, SSD1306_WHITE);
          display.setCursor(24, 28);
          //display.setTextSize(1);
          display.print("Hum(rel%): ");
          //display.setTextSize(2);
          display.print(AHT_latest.humid);

          display.setCursor(32, 48);
          display.setTextSize(2);
          display.print("AHT21");
          break;
        };

        // DS18B20 Screen
        case 3: {
          // Display DS18B20 Reading on the OLED Display
          // Thermometer Icon
          DS_latest = DS18history[(DS18historyIndex - 1 + DS18HISTORY_SIZE) % DS18HISTORY_SIZE];
          display.drawBitmap(0, 0, thermometer_bmp, 32, 32, SSD1306_WHITE);
          display.setCursor(34, 4);
          display.setTextSize(1);
          display.print("Temp (C): ");
          //display.setTextSize(2);
          display.print(DS_latest.DS_temp);
          display.setCursor(0, 38);
          //display.setTextSize(1);
          display.print("DS18B20"); 
          break;
        };
        case 4: {
          // Display AS7341 Reading on the OLED Display
          AS7341_latest = AS7341_history[(AS7341_historyIndex - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
          display.setCursor(0, 0);
          display.setTextSize(1);
          display.print("AS7341 F1-F8:");
          display.setCursor(0, 10);
          display.print(AS7341_latest.F1); display.print(", ");
          display.print(AS7341_latest.F2); display.print(", ");
          display.print(AS7341_latest.F3); display.print(", ");
          display.print(AS7341_latest.F4);
          display.setCursor(0, 20);
          display.print(AS7341_latest.F5); display.print(", ");
          display.print(AS7341_latest.F6); display.print(", ");
          display.print(AS7341_latest.F7); display.print(", ");
          display.print(AS7341_latest.F8);
          display.setCursor(0, 30);
          display.print("NIR:"); display.print(AS7341_latest.NIR);
          display.setCursor(64, 30);
          display.print("CLR:"); display.print(AS7341_latest.Clr);
          display.setCursor(0, 40);
          display.print("FLKR:"); display.print(AS7341_latest.FLKR);
          break;
        };
        case 5: {
          // Display AS7341 Bar Chart on the OLED Display
          AS7341_latest = AS7341_history[(AS7341_historyIndex - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
          UV_latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
          
          // --- Chart layout ---
          const int chartHeight = 40;   // pixels tall
          const int chartY = SCREEN_HEIGHT - 1;  
          const int barWidth = 8;       // each AS7341 channel bar width
          const int spacing = 2;        // gap between bars

          // Normalize AS7341 values
          uint16_t maxVal = 1;
          uint16_t channels[8] = {
            AS7341_latest.F1, AS7341_latest.F2, AS7341_latest.F3, AS7341_latest.F4,
            AS7341_latest.F5, AS7341_latest.F6, AS7341_latest.F7, AS7341_latest.F8
          };

          for (int i = 0; i < 8; i++) {
            if (channels[i] > maxVal) maxVal = channels[i];
          }

          // Draw AS7341 bars
          for (int i = 0; i < 8; i++) {
            int barHeight = map(channels[i], 0, maxVal, 0, chartHeight);
            int x = i * (barWidth + spacing);
            display.fillRect(x, chartY - barHeight, barWidth, barHeight, SSD1306_WHITE);
          }

          // --- UV bars (3 bars, drawn on the right side) ---
          float uvVals[3] = {UV_latest.uva, UV_latest.uvb, UV_latest.uvc};
          float maxUV = 1.0;
          for (int i = 0; i < 3; i++) {
            if (uvVals[i] > maxUV) maxUV = uvVals[i];
          }

          for (int i = 0; i < 3; i++) {
            int barHeight = map(uvVals[i], 0, maxUV, 0, chartHeight);
            int x = 80 + i * (barWidth + spacing);
            display.fillRect(x, chartY - barHeight, barWidth, barHeight, SSD1306_WHITE);
          }

          // --- Labels ---
          display.setTextSize(1);
          display.setTextColor(SSD1306_WHITE);
          display.setCursor(0,0);
          display.print("AS7341 + AS7331 UV");

          break;
        };
        default:
          break;
      }
      display.display();
      //Serial.print("Just updated screen: ");
      //Serial.println(ScreenDisplay); 
      //Serial.println("Screen updated");
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // small sleep
  }
}

void FWD_buttonTask(void *pvParameters) {
  for (;;) {
    if (FWD_buttonReadFlag) {
      FWD_buttonReadFlag = false;

      // Change the Screen to display when the screen update is called
      ScreenDisplay = (ScreenDisplay + 1) % NumOfScreens;
      //Serial.println("FWD Button hit");
      //Serial.print("Screen to display: ");
      //Serial.println(ScreenDisplay);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // small sleep
  }
}

void BK_buttonTask(void *pvParameters) {
  for (;;) {
    if (BK_buttonReadFlag) {
      BK_buttonReadFlag = false;

      // Change the Screen to display when the screen update is called
      ScreenDisplay = (ScreenDisplay - 1) % NumOfScreens;
      //Serial.println("Back Button hit");
      //Serial.print("Screen to display: ");
      //Serial.println(ScreenDisplay);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // small sleep
  }
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

void AS7341sensorTask(void *pvParameters) {
  for (;;) {
    if (AS7341sensorReadFlag) {
      AS7341sensorReadFlag = false;

       // Read all channels at the same time and store in as7341 object
      if (!as7341.readAllChannels()){
        Serial.println("Error reading all channels!");
        return;
      }

      // --- Read AS7341 sensor here ---
      uint16_t F1 = as7341.getChannel(AS7341_CHANNEL_415nm_F1);
      uint16_t F2 = as7341.getChannel(AS7341_CHANNEL_445nm_F2);
      uint16_t F3 = as7341.getChannel(AS7341_CHANNEL_480nm_F3);
      uint16_t F4 = as7341.getChannel(AS7341_CHANNEL_515nm_F4);
      uint16_t F5 = as7341.getChannel(AS7341_CHANNEL_555nm_F5);
      uint16_t F6 = as7341.getChannel(AS7341_CHANNEL_590nm_F6);
      uint16_t F7 = as7341.getChannel(AS7341_CHANNEL_630nm_F7);
      uint16_t F8 = as7341.getChannel(AS7341_CHANNEL_680nm_F8);
      uint16_t NIR = as7341.getChannel(AS7341_CHANNEL_NIR);
      uint16_t Clr = as7341.getChannel(AS7341_CHANNEL_CLEAR);
      uint16_t FLKR = as7341.detectFlickerHz();

      addAS7341Reading(F1, F2, F3, F4, F5, F6, F7, F8, NIR, Clr, FLKR);

      //Serial.println("AS7341 data read and stored");
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
  }
}

void AHT21sensorTask(void *pvParameters) {
  for (;;) {
    if (AHT21sensorReadFlag) {
      AHT21sensorReadFlag = false;

      //AHT21 Example Code
      // Create sensor event objects to store data
      sensors_event_t humidity, temp;

      // Get new data from the AHT21 sensor
      aht.getEvent(&humidity, &temp);
      addAHT21Reading(humidity.relative_humidity, temp.temperature);

      //Serial.println("AHT21 data read and stored");
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
  }
}

void DS18B20sensorTask(void *pvParameters) {
  for (;;) {
    if (DS18B20sensorReadFlag) {
      DS18B20sensorReadFlag = false;

      // --- Read UV sensor here ---
      addDS18Reading(ds.getTempC());
      //Serial.println("DS18 Tempurature data read and stored");

      // start the UV Sensor conversion
      if (ksfTkErrOk != uvSensor.setStartState(true))
      Serial.println("Error starting reading!");
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
  }
}

// ================== Functions ==================
void addUVReading(float uva, float uvb, float uvc) {
  UVReading r;
  r.uva = uva;
  r.uvb = uvb;
  r.uvc = uvc;
  r.uvIndex = calculateUVIndex(uva, uvb);
  r.UV_timestamp = millis();

  UVhistory[UVhistoryIndex] = r;
  UVhistoryIndex = (UVhistoryIndex + 1) % UVHISTORY_SIZE;
}

void addAS7341Reading(uint16_t F1, uint16_t F2, uint16_t F3, uint16_t F4, uint16_t F5, uint16_t F6, uint16_t F7, uint16_t F8, uint16_t NIR, uint16_t Clr, uint16_t FLKR) {
  AS7341Reading r;
  r.F1 = F1;
  r.F2 = F2;
  r.F3 = F3;
  r.F4 = F4;
  r.F5 = F5;
  r.F6 = F6;
  r.F7 = F7;
  r.F8 = F8;
  r.NIR = NIR;
  r.Clr = Clr;
  r.FLKR = FLKR;
  r.AS7341_timestamp = millis();

  AS7341_history[AS7341_historyIndex] = r;
  AS7341_historyIndex = (AS7341_historyIndex + 1) % AS7341_HISTORY_SIZE;
}

float calculateUVIndex(float uva, float uvb) {
  return (0.002 * uva + 0.005 * uvb);  // adjust calibration later
}

void addAHT21Reading(float hmd, float tmp) {
  AHT21Reading r;
  r.temp = tmp;
  r.humid = hmd;
  r.AHT_timestamp = millis();

  AHThistory[AHThistoryIndex] = r;
  AHThistoryIndex = (AHThistoryIndex + 1) % AHTHISTORY_SIZE;
}

void addDS18Reading(float tmp) {
  DS18Reading r;
  r.DS_temp = tmp;
  r.DS_timestamp = millis();

  DS18history[DS18historyIndex] = r;
  DS18historyIndex = (DS18historyIndex + 1) % DS18HISTORY_SIZE;
}

void printLatestUV() {
  UVReading latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
  Serial.printf("t=%lu ms | UVA=%.2f UVB=%.2f UVC=%.2f UVI=%.2f\n", latest.UV_timestamp, latest.uva, latest.uvb, latest.uvc, latest.uvIndex);
}

void printLatestAHT() {
  AHT21Reading latest = AHThistory[(AHThistoryIndex - 1 + AHTHISTORY_SIZE) % AHTHISTORY_SIZE];
  Serial.printf("t=%lu ms | tempurature=%.2f humidity=%.2f\n", latest.AHT_timestamp, latest.temp, latest.humid);
}

void printLatestDS18() {
  DS18Reading latest = DS18history[(DS18historyIndex - 1 + DS18HISTORY_SIZE) % DS18HISTORY_SIZE];
  Serial.printf("t=%lu ms | tempurature=%.2f\n", latest.DS_timestamp, latest.DS_temp);
}