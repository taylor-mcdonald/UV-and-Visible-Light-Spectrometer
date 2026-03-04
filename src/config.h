#pragma once

// Define SDA and SCL pins for I2C Bus 0
#define CUSTOM_SDA0_PIN 5
#define CUSTOM_SCL0_PIN 6

// Define SDA and SCL pins for I2C Bus 1
#define CUSTOM_SDA1_PIN 10
#define CUSTOM_SCL1_PIN 9

// Define button pins
#define FWD_button 7
#define BK_button 8

// ====== CONFIG ======
#define UV_RDY_PIN 4        // GPIO for AS7331 Data Ready interrupt
#define SCREEN_UPDATE_MS 500   //
#define SENSOR_TASK_DELAY 50   // safety delay if needed
#define AHT_UPDATE_MS 1000
#define DS18_UPDATE_MS 1000
#define BME680_UPDATE_MS 1000
#define UV_MEASUREMENT_INTERVAL 750 // time between UV measurements
#define debounce_delay 150

// AS7341 Definitions
#define SP_RDY_PIN 11       // GPIO for AS7341 Data Ready interrupt

//BME680 Definitions
#define BME680_ADDRESS 0x76 // I2C address of the BME680 sensor