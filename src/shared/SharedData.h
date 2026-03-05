#pragma once
#include <Arduino.h>
#include <freertos/queue.h>
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
  uint16_t flicker_hz;  // 0 until flicker detection is implemented
  long IntegrationTime;
  unsigned long timestamp;
};

extern AS7341Reading AS7341_Buffer; // temporary buffer for reading results

extern volatile bool AS7341_SMUX_low; // true = F1-F4, false = F5-F8
extern volatile uint8_t AS7341_currentGain;
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
#define NumOfScreens 8

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

extern float batteryVoltage;
extern float batteryPercent;


// ─── Flicker Detection ────────────────────────────────────────────────────────

#define FLICKER_LOW_SAMPLE_COUNT   40    // 2s at ~20 samples/sec (50ms integration)
#define FLICKER_HIGH_SAMPLE_COUNT  1000  // ~1s at ~1000 samples/sec (1ms integration)
#define MAX_FLICKER_PEAKS          10    // max peaks to report per band

// ATIME/ASTEP for low frequency capture (~50ms integration)
// (29 + 1) * (599 + 1) * 2.78 / 1000 = 50.04ms → ~20 samples/sec
#define FLICKER_LOW_ATIME   29
#define FLICKER_LOW_ASTEP   599

// ATIME/ASTEP for high frequency capture (~1ms integration)
// (0 + 1) * (359 + 1) * 2.78 / 1000 = 1.0008ms → ~1000 samples/sec
#define FLICKER_HIGH_ATIME  0
#define FLICKER_HIGH_ASTEP  359

struct FlickerPeak {
    uint16_t frequency_x10;  // Hz * 10 (e.g. 1.5Hz = 15, 120Hz = 1200)
    uint16_t magnitude;      // 0-65535 normalized
};

struct FlickerResult {
    FlickerPeak peaks[MAX_FLICKER_PEAKS];
    uint8_t     peakCount;
    uint32_t    timestamp;
    bool        valid;
};

// Buffer struct for passing flicker samples from capture task to FFT task
struct FlickerBuffer {
    uint16_t samples[FLICKER_HIGH_SAMPLE_COUNT];
    uint16_t count;
    bool     isLowFreq;
};

extern FlickerResult flickerLowResult;   // 0.5 - 10Hz
extern FlickerResult flickerHighResult;  // 10 - 450Hz

extern volatile bool spectralCaptureInProgress;
extern TaskHandle_t flickerCaptureTaskHandle;
extern TaskHandle_t fftTaskHandle;
extern TaskHandle_t spectralCaptureTaskHandle;

// Queue handles for passing buffers from FlickerCaptureTask to FFTTask
extern QueueHandle_t flickerLowQueue;
extern QueueHandle_t flickerHighQueue;

extern FlickerBuffer flickerLowBuf;
extern FlickerBuffer flickerHighBuf;