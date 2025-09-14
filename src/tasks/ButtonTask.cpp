#include "ButtonTask.h"


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

void initButtons() {
  // Set forward and back button interrupts
  pinMode(FWD_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FWD_button), onFWD_button_detect, FALLING);
  pinMode(BK_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BK_button), onBK_button_detect, FALLING);


  xTaskCreatePinnedToCore(
    FWD_buttonTask,
    "Forward Button Task",
    4096,
    NULL,
    1,
    NULL,
    tskNO_AFFINITY
    );

  xTaskCreatePinnedToCore(
    BK_buttonTask,
    "Back Button Task",
    4096,
    NULL,
    1,
    NULL,
    tskNO_AFFINITY
    );

    return;
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