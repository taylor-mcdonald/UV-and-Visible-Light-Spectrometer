#include "ButtonTask.h"

// Task handle
TaskHandle_t FwdButtonHandle = nullptr;
TaskHandle_t BackButtonHandle = nullptr;

void IRAM_ATTR onFWD_button_detect() {
  unsigned long current_time = millis();
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (current_time - last_interrupt_time_fwd > debounce_delay) {
    // This is a valid interrupt, perform desired action
    //FWD_buttonReadFlag = true;     // set flag for sensor task
    vTaskNotifyGiveFromISR(FwdButtonHandle, &xHigherPriorityTaskWoken);
    last_interrupt_time_fwd = current_time; // Update last accepted time
  }
    // Yield to higher priority task if needed
  if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();  // ESP32 version takes no args
  }
}

void IRAM_ATTR onBK_button_detect() {
  unsigned long current_time = millis();
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (current_time - last_interrupt_time_bk > debounce_delay) {
    //BK_buttonReadFlag = true;     // set flag for sensor task
    vTaskNotifyGiveFromISR(BackButtonHandle, &xHigherPriorityTaskWoken);
    last_interrupt_time_bk = current_time; // Update last accepted time
  }
      // Yield to higher priority task if needed
  if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();  // ESP32 version takes no args
  }
}

void startButtonTasks() {
  xTaskCreatePinnedToCore(
    FWD_buttonTask,
    "Forward Button Task",
    1024,
    NULL,
    1,
    &FwdButtonHandle,
    tskNO_AFFINITY
  );

  xTaskCreatePinnedToCore(
    BK_buttonTask,
    "Back Button Task",
    1024,
    NULL,
    1,
    &BackButtonHandle,
    tskNO_AFFINITY
  );

  return;
}

void initButtons() {

  // Set forward and back button interrupts
  pinMode(FWD_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FWD_button), onFWD_button_detect, FALLING);
  pinMode(BK_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BK_button), onBK_button_detect, FALLING);

  return;
}

void FWD_buttonTask(void *pvParameters) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification from ISR

    // Change the Screen to display when the screen update is called
    ScreenDisplay = (ScreenDisplay + 1) % NumOfScreens;

    }
}

void BK_buttonTask(void *pvParameters) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification from ISR

    // Change the Screen to display when the screen update is called
    ScreenDisplay = (ScreenDisplay - 1 + NumOfScreens) % NumOfScreens;

    }
}