#include "ScreenTask.h"

// 'UV Icon', 32x32px
const unsigned char UV_Icon_bmp[] PROGMEM = {
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
const unsigned char humidity_bmp[] PROGMEM = {
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
const unsigned char thermometer_bmp[] PROGMEM = {
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

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);



TaskHandle_t screenTaskHandle;

void initScreen() {
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
  Serial.println("Screen initialized");
  delay(3000);

}

void startScreenTask() {
     xTaskCreatePinnedToCore(
        screenTask,             // Function that implements the task.
        "Screen Task",          // Text name for the task.
        4096,                   // Stack size in words, not bytes.
        NULL,                   // Parameter passed into the task.
        1,                      // Priority at which the task is created.
        &screenTaskHandle,       // Used to pass out the created task's handle.
        tskNO_AFFINITY          // Run on any core.
    );
}


void screenTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t interval = pdMS_TO_TICKS(SCREEN_UPDATE_MS); // 500 ms
  
  for (;;) {
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
        AS7341_latest_low = AS7341_history_low[(AS7341_historyIndex_low - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
        AS7341_latest_high = AS7341_history_high[(AS7341_historyIndex_high - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
        display.setCursor(0, 0);
        display.setTextSize(1);
        display.print("AS7341 F1-F8:");
        display.setCursor(0, 10);
        display.print(AS7341_latest_low.F1_F5); display.print(", ");
        display.print(AS7341_latest_low.F2_F6); display.print(", ");
        display.print(AS7341_latest_low.F3_F7); display.print(", ");
        display.print(AS7341_latest_low.F4_F8);
        display.setCursor(0, 20);
        display.print(AS7341_latest_high.F1_F5); display.print(", ");
        display.print(AS7341_latest_high.F2_F6); display.print(", ");
        display.print(AS7341_latest_high.F3_F7); display.print(", ");
        display.print(AS7341_latest_high.F4_F8);
        display.setCursor(0, 30);
        display.print("NIR:"); display.print(AS7341_latest_low.NIR);
        display.setCursor(64, 30);
        display.print("CLR:"); display.print(AS7341_latest_high.Clr);
        display.setCursor(0, 40);
       // display.print("FLKR:"); display.print(AS7341_latest.FLKR);
        break;
      };
      case 5: {
        // Display AS7341 Bar Chart on the OLED Display
        AS7341_latest_low = AS7341_history_low[(AS7341_historyIndex_low - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
        AS7341_latest_high = AS7341_history_high[(AS7341_historyIndex_high - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
        UV_latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];
        
        // --- Chart layout ---
        const int chartHeight = 40;   // pixels tall
        const int chartY = SCREEN_HEIGHT - 1;  
        const int barWidth = 8;       // each AS7341 channel bar width
        const int spacing = 2;        // gap between bars

        // Normalize AS7341 values
        uint16_t maxVal = 1;
        //Get the values in reverse order for display (F8 to F1 -> low nm to high nm)
        uint16_t channels[9] = {
          AS7341_latest_high.NIR, AS7341_latest_high.F4_F8, AS7341_latest_high.F3_F7, AS7341_latest_high.F2_F6, 
          AS7341_latest_high.F1_F5, AS7341_latest_low.F4_F8, AS7341_latest_low.F3_F7, AS7341_latest_low.F2_F6,
          AS7341_latest_low.F1_F5
        };

        for (int i = 0; i < 9; i++) {
          if (channels[i] > maxVal) maxVal = channels[i];
        }

        // Draw AS7341 bars
        for (int i = 0; i < 9; i++) {
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
          int x = 90 + i * (barWidth + spacing);
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
  // Wait until the next 500 ms boundary
  vTaskDelayUntil(&lastWake, interval);
}


