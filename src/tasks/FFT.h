#pragma once

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"
#include "shared/SharedData.h"

void AS7341_FFT_Task(void *pvParameters);