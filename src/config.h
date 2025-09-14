#pragma once

//Define GPIO pin for DS18B20 data line
#define DS18B20_PIN 7

// Define SDA and SCL pins
#define CUSTOM_SDA_PIN 4
#define CUSTOM_SCL_PIN 5

// Define button pins
#define FWD_button 8
#define BK_button 9

// ====== CONFIG ======
#define UV_RDY_PIN 3        // GPIO for AS7331 Data Ready interrupt
#define SCREEN_UPDATE_MS 500   //
#define SENSOR_TASK_DELAY 50   // safety delay if needed
#define AHT_UPDATE_MS 1000
#define DS18_UPDATE_MS 1000
#define debounce_delay 150

// AS7341 Definitions
#define SP_RDY_PIN 2       // GPIO for AS7341 Data Ready interrupt
#define PWM_PIN 1          // GPIO for PWM output to AS7341 sync signal  
#define PWM_CHANNEL 0      // LEDC PWM channel
#define PWM_TIMER   0      // LEDC PWM timer 