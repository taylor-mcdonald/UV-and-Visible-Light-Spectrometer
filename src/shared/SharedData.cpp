#include "SharedData.h"

// ================== Global Variables ==================
volatile unsigned long last_interrupt_time_fwd = 0; // Global or static in ISR
volatile unsigned long last_interrupt_time_bk = 0; // Global or static in ISR

// ====== FLAGS ======
volatile bool UVsensorReadFlag = false;
volatile bool FWD_buttonReadFlag = false;
volatile bool BK_buttonReadFlag = false;
volatile bool AS7341sensorInterruptFlag = false;
volatile bool AS7341sensorReadFlag = false;
volatile bool AS7341sensorSMUXFlag = false;

volatile bool AS7341_SMUX_low = true; // true = F1-F4, false = F5-F8

volatile uint8_t AS7341_currentGain = 1; // default gain 1x
volatile uint16_t AS7341_current_AStep = 599;
volatile uint8_t AS7341_current_ATime = 29;

volatile long AS7341_Time1 = 0;
volatile long AS7341_Time2 = 0;

//  Index to define which screen is to be drawn on the display
//  0 - UV Raw Data
//  1 - UV Index
//  2 - AHT21 Data
//  3 - DS18B20 Data
//  4 - AS7341 Data (visible spectrum)
//  5 - Bar Chart of AS7341 and UV Index
volatile int ScreenDisplay;

// Create a rotating history of all sensor readings
UVReading UVhistory[UVHISTORY_SIZE];
int UVhistoryIndex = 0;
UVReading UV_latest;

AS7341Reading AS7341_history_low[AS7341_HISTORY_SIZE];
int AS7341_historyIndex_low = 0;
AS7341Reading AS7341_latest_low;

AS7341Reading AS7341_history_high[AS7341_HISTORY_SIZE];
int AS7341_historyIndex_high = 0;
AS7341Reading AS7341_latest_high;

BME680Reading BME680history[BME680HISTORY_SIZE];
int BME680historyIndex = 0;
BME680Reading BME680_latest;

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

float calculateUVIndex(float uva, float uvb) {
  return (0.002 * uva + 0.005 * uvb);  // adjust calibration later
}

void addBME680Reading(float temp, float humid, float press, float gas_resistance) {
  BME680Reading r;
  r.temp = temp;
  r.humid = humid;
  r.press = press;
  r.gas_resistance = gas_resistance;
  r.BME680_timestamp = millis();

  BME680history[BME680historyIndex] = r;
  BME680historyIndex = (BME680historyIndex + 1) % BME680HISTORY_SIZE;
}

void printLatestUV() {
  UVReading latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
  Serial.printf("t=%lu ms | UVA=%.2f UVB=%.2f UVC=%.2f UVI=%.2f\n", latest.UV_timestamp, latest.uva, latest.uvb, latest.uvc, latest.uvIndex);
}

void printLatestBME680() {
  BME680Reading latest = BME680history[(BME680historyIndex - 1 + BME680HISTORY_SIZE) % BME680HISTORY_SIZE];
  Serial.printf("t=%lu ms | temp=%.2f C humidity=%.2f %% pressure=%.2f hPa gas_resistance=%.2f KOhms\n", latest.BME680_timestamp, latest.temp, latest.humid, latest.press, latest.gas_resistance / 1000.0);
}

void printByteBinary(uint8_t value) {
  Serial.print("0b ");
  for (int i = 7; i >= 0; i--) {
    Serial.print((value >> i) & 0x01);
    if (i == 4) Serial.print(" ");  // space between nibbles
  }
  Serial.println();
}

float batteryVoltage = 0.0f;
float batteryPercent = 0.0f;


FlickerResult flickerResult = {};
volatile bool spectralCaptureInProgress = false;
TaskHandle_t flickerCaptureTaskHandle = nullptr;
TaskHandle_t fftTaskHandle            = nullptr;
TaskHandle_t spectralCaptureTaskHandle = nullptr;
QueueHandle_t flickerQueue            = nullptr;

uint16_t *flickerSamples = nullptr;

