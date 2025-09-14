#pragma once
#include <Arduino.h>
#include "shared/SharedData.h"
#include "Config.h"

void IRAM_ATTR onFWD_button_detect();
void IRAM_ATTR onBK_button_detect();

void initButtons();

void FWD_buttonTask(void *pvParameters);
void BK_buttonTask(void *pvParameters);
