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

// struct UVReading {
//   float uva;
//   float uvb;
//   float uvc;
//   float uvIndex;
//   unsigned long UV_timestamp;
// };

// struct AS7341Reading {
//   uint16_t F1;	// 415nm -> Violet
//   uint16_t F2;	// 445nm -> Indigo-Blue
//   uint16_t F3;  // 480nm -> Blue
//   uint16_t F4;	// 515nm -> Green
//   uint16_t F5;  // 555nm -> Yellow-Green
//   uint16_t F6;  // 590nm -> Yellow-Orange
//   uint16_t F7;  // 630nm -> Red
//   uint16_t F8;  // 680nm -> more Red
//   uint16_t NIR; // Near Infrared
//   uint16_t Clr; // Clear Channel	
//   uint16_t FLKR;// Flicker Detection	
//   as7341_gain_t gain;      // Current gain setting
//   long AS7341_IntegrationTime;

//   unsigned long AS7341_timestamp;
// };

volatile bool AS7341_SMUX_low = true; // true = F1-F4, false = F5-F8

volatile uint8_t AS7341_currentGain = 1; // default gain 1x
volatile uint16_t AS7341_current_AStep = 599;
volatile uint8_t AS7341_current_ATime = 29;

volatile long AS7341_Time1 = 0;
volatile long AS7341_Time2 = 0;

// struct AHT21Reading {
//   float temp;
//   float humid;
//   unsigned long AHT_timestamp;
// };

// struct DS18Reading {
//   float DS_temp;
//   unsigned long DS_timestamp;
// };

//  Index to define which screen is to be drawn on the display
//  0 - UV Raw Data
//  1 - UV Index
//  2 - AHT21 Data
//  3 - DS18B20 Data
//  4 - AS7341 Data (visible spectrum)
//  5 - Bar Chart of AS7341 and UV Index
volatile int ScreenDisplay;
//#define NumOfScreens 6

// Create a rotating history of all sensor readings
//#define UVHISTORY_SIZE 120
UVReading UVhistory[UVHISTORY_SIZE];
int UVhistoryIndex = 0;
UVReading UV_latest;



//#define AS7341_HISTORY_SIZE 120
AS7341Reading AS7341_history_low[AS7341_HISTORY_SIZE];
int AS7341_historyIndex_low = 0;
AS7341Reading AS7341_latest_low;

AS7341Reading AS7341_history_high[AS7341_HISTORY_SIZE];
int AS7341_historyIndex_high = 0;
AS7341Reading AS7341_latest_high;

//#define AHTHISTORY_SIZE 120
AHT21Reading AHThistory[AHTHISTORY_SIZE];
int AHThistoryIndex = 0;
AHT21Reading AHT_latest;

//#define DS18HISTORY_SIZE 120
DS18Reading DS18history[DS18HISTORY_SIZE];
int DS18historyIndex = 0;
DS18Reading DS_latest;

// void addUVReading(float uva, float uvb, float uvc);
// void printLatestUV(void);
// float calculateUVIndex(float uva, float uvb);

// void printByteBinary(uint8_t value);

// void addAHT21Reading(float hmd, float tmp);
// void addDS18Reading(float tmp);
// void addAS7341Reading(
//   uint16_t F1,
//   uint16_t F2,
//   uint16_t F3,
//   uint16_t F4,
//   uint16_t F5,
//   uint16_t F6,
//   uint16_t F7,
//   uint16_t F8,
//   uint16_t NIR,
//   uint16_t Clr,
//   uint16_t FLKR,
//   as7341_gain_t gain,
//   long AS7341_IntegrationTime
// );

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

// void addAS7341Reading(uint16_t F1, uint16_t F2, uint16_t F3, uint16_t F4, \
//   uint16_t NIR, uint16_t Clr, as7341_gain_t gain) {
  
//   AS7341Reading r;
//   r.F1_F5 = F1;
//   r.F2_F6 = F2;
//   r.F3_F7 = F3;
//   r.F4_F8 = F4;
//   r.NIR = NIR;
//   r.Clr = Clr;
//   r.gain = gain;
//   //r.AS7341_IntegrationTime = AS7341_IntegrationTime;
//   r.timestamp = millis();

//   if (AS7341_SMUX_low) {
//     AS7341_history_low[AS7341_historyIndex_low] = r;
//     AS7341_historyIndex_low = (AS7341_historyIndex_low + 1) % AS7341_HISTORY_SIZE;
//   } else {
//     AS7341_history_high[AS7341_historyIndex_high] = r;
//     AS7341_historyIndex_high = (AS7341_historyIndex_high + 1) % AS7341_HISTORY_SIZE;
//   }
// }

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

void printByteBinary(uint8_t value) {
  Serial.print("0b ");
  for (int i = 7; i >= 0; i--) {
    Serial.print((value >> i) & 0x01);
    if (i == 4) Serial.print(" ");  // space between nibbles
  }
  Serial.println();
}