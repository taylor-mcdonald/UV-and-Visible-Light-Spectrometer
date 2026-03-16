#pragma once
#include <Arduino.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
//#include <Adafruit_AS7341.h>

// ================== Global Variables ==================
extern volatile unsigned long last_interrupt_time_fwd; // Global or static in ISR
extern volatile unsigned long last_interrupt_time_bk; // Global or static in ISR

// ====== FLAGS ======
extern volatile bool UVsensorReadFlag;
extern volatile bool FWD_buttonReadFlag;
extern volatile bool BK_buttonReadFlag;
extern volatile bool AS7341sensorInterruptFlag;
extern volatile bool AS7341sensorReadFlag;
extern volatile bool AS7341sensorSMUXFlag;


struct UVReading {
  float uva;
  float uvb;
  float uvc;
  float uvIndex;
  unsigned long UV_timestamp;
};

struct AS7341Reading {
  uint16_t F1_F5;	// 415nm -> Violet
  uint16_t F2_F6;	// 445nm -> Indigo-Blue
  uint16_t F3_F7;  // 480nm -> Blue
  uint16_t F4_F8;	// 515nm -> Green
  uint16_t NIR; // Near Infrared
  uint16_t Clr; // Clear Channel	
  uint8_t gain;      // Current gain setting
  bool saturation;
  long IntegrationTime;
  unsigned long timestamp;
};

extern volatile bool AS7341_SMUX_low; // true = F1-F4, false = F5-F8
extern volatile uint8_t AS7341_spectralGain;
extern volatile uint16_t AS7341_current_AStep;
extern volatile uint8_t AS7341_current_ATime;
extern volatile long AS7341_Time1;
extern volatile long AS7341_Time2;

struct BME680Reading {
  float temp;
  float humid;
  float press;
  float gas_resistance;
  unsigned long BME680_timestamp;
};



//  Index to define which screen is to be drawn on the display
//  0 - UV Raw Data
//  1 - UV Index
//  2 - AHT21 Data
//  3 - DS18B20 Data
//  4 - AS7341 Data (visible spectrum)
//  5 - Bar Chart of AS7341 and UV Index
extern volatile int ScreenDisplay;
#define NumOfScreens 9

// Create a rotating history of all sensor readings
#define UVHISTORY_SIZE 120
extern UVReading UVhistory[UVHISTORY_SIZE];
extern int UVhistoryIndex ;
extern UVReading UV_latest;

#define AS7341_HISTORY_SIZE 120
extern AS7341Reading AS7341_history_low[AS7341_HISTORY_SIZE];
extern int AS7341_historyIndex_low;
extern AS7341Reading AS7341_latest_low;

extern AS7341Reading AS7341_history_high[AS7341_HISTORY_SIZE];
extern int AS7341_historyIndex_high;
extern AS7341Reading AS7341_latest_high;

#define BME680HISTORY_SIZE 120
extern BME680Reading BME680history[BME680HISTORY_SIZE];
extern int BME680historyIndex;
extern BME680Reading BME680_latest;

void addUVReading(float uva, float uvb, float uvc);
void printLatestUV(void);
float calculateUVIndex(float uva, float uvb);

void printByteBinary(uint8_t value);

void addBME680Reading(float temp, float humid, float press, float gas_resistance);
void printLatestBME680(void);

void addAS7341Reading_low(AS7341Reading &r);
void addAS7341Reading_high(AS7341Reading &r);

struct BatteryReading {
    float voltage;
    float percent;
    float changeRate;   // %/hour, positive=charging, negative=discharging
    unsigned long timestamp;
};

extern BatteryReading batteryReading;

// Keep these as convenience aliases so existing screen code compiles unchanged
#define batteryVoltage batteryReading.voltage
#define batteryPercent batteryReading.percent

// ─── Flicker Detection ────────────────────────────────────────────────────────

// Single capture: 2000 samples/sec, 2000 samples = 1 second = 1Hz resolution
#define FLICKER_SAMPLE_COUNT      2048
#define FLICKER_FD_TIME           180    // 180 × 2.78μs = 500μs → 2000 samples/sec
#define MAX_FLICKER_PEAKS         10    // max peaks to report per band


struct FlickerResult {
    float    peaks[MAX_FLICKER_PEAKS];      // frequencies in Hz, up to 10 peaks
    float    magnitudes[MAX_FLICKER_PEAKS]; // normalized magnitude 0.0-1.0
    uint8_t  peakCount;
    uint32_t timestamp;
    bool     valid;
};


extern FlickerResult flickerResult;
extern TaskHandle_t flickerCaptureTaskHandle;
extern TaskHandle_t fftTaskHandle;
extern TaskHandle_t spectralCaptureTaskHandle;
extern QueueHandle_t flickerQueue;       // sends FlickerBuffer* to FFT task
extern uint16_t *flickerSamples;    

extern SemaphoreHandle_t spectralDoneSemaphore;  // flicker blocks on this