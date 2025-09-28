#pragma once
#include <Arduino.h>
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

extern AS7341Reading AS7341_Buffer; // temporary buffer for reading results

extern volatile bool AS7341_SMUX_low; // true = F1-F4, false = F5-F8
extern volatile uint8_t AS7341_currentGain;
extern volatile uint16_t AS7341_current_AStep;
extern volatile uint8_t AS7341_current_ATime;
extern volatile long AS7341_Time1;
extern volatile long AS7341_Time2;

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
extern volatile int ScreenDisplay;
#define NumOfScreens 6

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

#define AHTHISTORY_SIZE 120
extern AHT21Reading AHThistory[AHTHISTORY_SIZE];
extern int AHThistoryIndex;
extern AHT21Reading AHT_latest;

#define DS18HISTORY_SIZE 120
extern DS18Reading DS18history[DS18HISTORY_SIZE];
extern int DS18historyIndex;
extern DS18Reading DS_latest;

void addUVReading(float uva, float uvb, float uvc);
void printLatestUV(void);
float calculateUVIndex(float uva, float uvb);

void printByteBinary(uint8_t value);

void addAHT21Reading(float hmd, float tmp);
void addDS18Reading(float tmp);