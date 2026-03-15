#include "SpectralTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include <Wire.h>
#include "FFT.h"


Adafruit_AS7341 as7341; // Create an instance of the AS7341 sensor object

void initAS7341Sensor(TwoWire &wirePort) {
    if (!as7341.begin(AS7341_I2CADDR_DEFAULT, &wirePort)) {
        Serial.println("AS7341: begin() FAILED");
        while (1) { delay(10); }
    }
    Serial.println("AS7341: begin() OK");

    // Log what begin() left behind so we know our starting state
    Serial.printf("AS7341 post-begin CFG8 (0xB1):   0x%02X\n", as7341.getRegister(AS7341_CFG8));
    Serial.printf("AS7341 post-begin INTENAB (0xF9): 0x%02X\n", as7341.getRegister(AS7341_INTENAB));
    Serial.printf("AS7341 post-begin ENABLE (0x80):  0x%02X\n", as7341.getRegister(AS7341_ENABLE));

    as7341.powerEnable(true);
    delay(10);

    // Integration timing — shared by spectral mode, set here as defaults
    as7341.setATIME(AS7341_current_ATime);
    as7341.setASTEP(AS7341_current_AStep);

    // Clear all status bits
    as7341.writeRegister(AS7341_STATUS, 0xFF);
}

void startSpectralTasks() {

    BaseType_t result = xTaskCreatePinnedToCore(
        AS7341_Flicker_Capture_Task,
        "FlickerCapture",
        16384,                       // larger stack — big sample buffers
        NULL,
        2,                          // higher priority than FFT
        &flickerCaptureTaskHandle,
        1                           // Core 1
    );

    xTaskCreatePinnedToCore(
        AS7341_FFT_Task,
        "FFTTask",
        16384,
        NULL,
        1,
        &fftTaskHandle,
        1                           // Core 1
    );

    xTaskCreatePinnedToCore(
        AS7341_Spectral_Capture_Task,
        "SpectralCapture",
        4096,
        NULL,
        1,
        &spectralCaptureTaskHandle,
        1                           // Core 1
    );
    Serial.print("FlickerCapture task create result: ");
    Serial.println(result == pdPASS ? "OK" : "FAILED");
}

void setupForSpectral() {
    // Full stop
    as7341.writeRegister(AS7341_ENABLE, 0x00);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Integration timing
    as7341.setATIME(AS7341_current_ATime);
    as7341.setASTEP(AS7341_current_AStep);

    // Wait time between measurements
    as7341.writeRegister(AS7341_WTIME, 89);  // ~250ms

    // Starting gain — AGC will adjust from here
    as7341.writeRegister(AS7341_CFG1, AS7341_spectralGainStart);

    // Spectral AGC thresholds — target 10% low, 90% high
    as7341.writeRegister(AS7341_SP_LOW_TH_L,  0x99);  // 10% of 0xFFFF
    as7341.writeRegister(AS7341_SP_LOW_TH_H,  0x19);
    as7341.writeRegister(AS7341_SP_HIGH_TH_L, 0x66);  // 90% of 0xFFFF
    as7341.writeRegister(AS7341_SP_HIGH_TH_H, 0xE6);

    // CFG8 (0xB1): SP_AGC on, FD_AGC off, FIFO_TH=2 (8 entries)
    uint8_t cfg8 = as7341.getRegister(AS7341_CFG8);
    cfg8 |=  AS7341_SPECTRAL_AUTO_GAIN;   // set SP_AGC
    cfg8 &= ~AS7341_FLICKER_AUTO_GAIN;   // clear FD_AGC
    cfg8  = (cfg8 & 0x3F) | (2 << 6);   // FIFO_TH=2
    as7341.writeRegister(AS7341_CFG8, cfg8);

    // CFG10 (0xB3): AGC hysteresis AGC_H=87.5%, AGC_L=12.5%
    as7341.writeRegister(AS7341_CFG10, 0xC2);

    // SMUX configuration
    // SMUX — write channel config, then trigger load
    as7341.writeRegister(AS7341_CFG6, 0x10);  // set SMUX command: write from RAM
    if (AS7341_SMUX_low) {
        as7341.setup_F1F4_Clear_NIR();
    } else {
        as7341.setup_F5F8_Clear_NIR();
    }
    // Trigger SMUX load: SMUXEN + WEN + PON
    as7341.writeRegister(AS7341_ENABLE, 0x19);
    while (as7341.getRegister(AS7341_ENABLE) & 0x10) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Interrupts — spectral complete and saturation only
    as7341.writeRegister(AS7341_INTENAB, 0x81); // ASIEN + SIEN

    // Clear status
    as7341.writeRegister(AS7341_STATUS, 0xFF);

    // Enable spectral measurements — SP_EN + WEN + PON
    as7341.writeRegister(AS7341_ENABLE, 0x0B);
}

void setupForFlicker(uint8_t fdGain) {
    // Full stop — required before writing FD_TIME2 (0xDA)
    as7341.writeRegister(AS7341_ENABLE, 0x00);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Safe write window: FDEN=0, PON=0
    as7341.writeRegister(AS7341_FD_TIME1, FLICKER_FD_TIME & 0xFF);
    uint8_t fdt2 = as7341.getRegister(AS7341_FD_TIME2);
    as7341.writeRegister(AS7341_FD_TIME2, (fdGain << 3) & 0xF8);  // MSB bits = 0, gain in bits 7:3

    // Autozero every cycle
    as7341.writeRegister(AS7341_AZ_CONFIG, 0x01);

    // CFG8 (0xB1): both AGC bits clear, FIFO_TH=3 (16 entries)
    uint8_t cfg8 = as7341.getRegister(AS7341_CFG8);
    cfg8 &= ~AS7341_FLICKER_AUTO_GAIN;
    cfg8 &= ~AS7341_SPECTRAL_AUTO_GAIN;
    cfg8  = (cfg8 & 0x3F) | (3 << 6);
    as7341.writeRegister(AS7341_CFG8, cfg8);

    // AGC_GAIN_MAX (0xCF): ceiling at 9 (256x), preserve lower nibble
    uint8_t agcMax = as7341.getRegister(AS7341_AGC_GAIN_MAX);
    as7341.writeRegister(AS7341_AGC_GAIN_MAX, (agcMax & 0x0F) | (9 << 4));

    // Interrupts — none needed for FIFO polling
    as7341.writeRegister(AS7341_INTENAB, 0x00);

    // Clear status
    as7341.writeRegister(AS7341_STATUS, 0xFF);

    // PON only
    as7341.writeRegister(AS7341_ENABLE, 0x01);
    vTaskDelay(pdMS_TO_TICKS(5));

    // SMUX for flicker
    as7341.setupFDSmux();

    // FIFO configuration
    as7341.configureFIFO_FD(true);

    // FDEN + PON only — no SP_EN
    as7341.writeRegister(AS7341_ENABLE, 0x41);

    // Flush FIFO
    as7341.writeRegister(AS7341_CONTROL, 0x02);
    as7341.writeRegister(AS7341_CONTROL, 0x00);
}

void AS7341_Flicker_Capture_Task(void *pvParameters) {
    const TickType_t pauseCheckInterval = pdMS_TO_TICKS(50);
    uint8_t fdGain = 6;  // 32x starting gain for indoor office

    Serial.println("FlickerCaptureTask: task entered");
    vTaskDelay(pdMS_TO_TICKS(2000));
    Serial.println("FlickerCaptureTask: starting");

    for (;;) {
        // Yield to spectral capture if needed
        if (spectralCaptureInProgress) {
            Serial.println("FlickerCaptureTask: pausing for spectral capture");
            while (spectralCaptureInProgress) {
                vTaskDelay(pauseCheckInterval);
            }
            Serial.println("FlickerCaptureTask: resuming");
        }

        // ── CONFIGURE SENSOR FOR FLICKER CAPTURE ─────────────────────────
        setupForFlicker(fdGain);

        // Diagnostics — remove after validated
        Serial.print("ENABLE: 0x");    Serial.println(as7341.getRegister(AS7341_ENABLE), HEX);
        Serial.print("CONTROL: 0x");   Serial.println(as7341.getRegister(AS7341_CONTROL), HEX);
        Serial.print("FD_TIME1: ");    Serial.println(as7341.getRegister(AS7341_FD_TIME1));
        Serial.print("FD_TIME2: 0x");  Serial.println(as7341.getRegister(AS7341_FD_TIME2), HEX);
        Serial.print("FIFO_MAP: 0x");  Serial.println(as7341.getRegister(AS7341_FIFO_MAP), HEX);
        Serial.print("FD_CFG0: 0x");   Serial.println(as7341.getRegister(AS7341_FD_CFG0), HEX);
        Serial.print("FD_STATUS: 0x"); Serial.println(as7341.getRegister(AS7341_FD_STATUS), HEX);
        Serial.print("STATUS2: 0x");   Serial.println(as7341.getRegister(AS7341_STATUS2), HEX);
        Serial.print("CFG8: 0x");      Serial.println(as7341.getRegister(AS7341_CFG8), HEX);
        Serial.print("AZ_CONFIG: 0x"); Serial.println(as7341.getRegister(AS7341_AZ_CONFIG), HEX);

        uint8_t fdt2 = as7341.getRegister(AS7341_FD_TIME2);
        uint8_t fdGainRead = (fdt2 >> 3) & 0x1F;
        uint8_t fdTimeMSB  = fdt2 & 0x07;
        uint16_t fdTime    = ((uint16_t)fdTimeMSB << 8) | as7341.getRegister(AS7341_FD_TIME1);
        float gainMultiplier = 0.5f * (1 << fdGainRead);
        Serial.print("FD_TIME2: 0x"); Serial.print(fdt2, HEX);
        Serial.print("  FD_GAIN="); Serial.print(fdGainRead);
        Serial.print(" ("); Serial.print(gainMultiplier, 0); Serial.print("x)");
        Serial.print("  FD_TIME_MSB="); Serial.println(fdTimeMSB);
        Serial.print("Calculated FD_TIME(μs): "); Serial.println(fdTime * 2.78f);

        // ── CAPTURE 2048 SAMPLES ──────────────────────────────────────────
        uint16_t samplesCollected = 0;
        uint32_t captureStart = millis();
        const uint32_t captureTimeout = 4000;

        while (samplesCollected < FLICKER_SAMPLE_COUNT &&
               (millis() - captureStart) < captureTimeout) {

            uint8_t newSamples = as7341.readFIFO(
                &flickerSamples[samplesCollected],
                min((int)(FLICKER_SAMPLE_COUNT - samplesCollected), 64)
            );
            samplesCollected += newSamples;

            if (newSamples == 0) {
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }

        uint32_t elapsed = millis() - captureStart;
        Serial.print("FlickerCapture: samples="); Serial.print(samplesCollected);
        Serial.print(" elapsed_ms="); Serial.println(elapsed);

        Serial.print("samples[0-19]: ");
        for (int i = 0; i < 20 && i < samplesCollected; i++) {
            Serial.print(flickerSamples[i]);
            Serial.print(" ");
        }
        Serial.println();

        if (samplesCollected == FLICKER_SAMPLE_COUNT) {
            // ── READ SATURATION FLAGS ─────────────────────────────────────
            uint8_t status2  = as7341.getRegister(AS7341_STATUS2);
            uint8_t fdStatus = as7341.getRegister(AS7341_FD_STATUS);

            bool fdsat_analog  = (status2  >> 1) & 0x01;
            bool fdsat_digital = (status2  >> 0) & 0x01;
            bool fd_sat        = (fdStatus >> 4) & 0x01;

            Serial.print("fdsat_analog=");  Serial.print(fdsat_analog);
            Serial.print(" fdsat_digital="); Serial.print(fdsat_digital);
            Serial.print(" fd_sat=");        Serial.print(fd_sat);
            Serial.print(" peakVal=");

            // ── COMPUTE PEAK ──────────────────────────────────────────────
            uint16_t peakVal = 0;
            for (int i = 0; i < FLICKER_SAMPLE_COUNT; i++) {
                if (flickerSamples[i] > peakVal) peakVal = flickerSamples[i];
            }
            Serial.println(peakVal);

            // ── SOFTWARE AGC ──────────────────────────────────────────────
            if (fdsat_analog && fdGain > 0) {
                flickerResult.valid = false;
                fdGain -= 2;  // reduce even for analog sat — less gain = less current into ADC
            } else if (fdsat_digital && fdGain > 0) {
                fdGain -= 2;
            } else if (fd_sat && fdGain > 0) {
                fdGain -= 1;
            } else if (peakVal < 20 && fdGain < 9) {
                fdGain += 2;
            } else if (peakVal < 60 && fdGain < 9) {
                fdGain += 1;
            } else if (peakVal > 160 && fdGain > 0) {
                fdGain -= 1;
            }
            fdGain = constrain(fdGain, 0, 9);
            Serial.print("fdGain after AGC: "); Serial.println(fdGain);

            
            // Skip sending to FFT if signal is at noise floor
            if (peakVal < 10) {
                Serial.println("FFT: skipping — signal too weak");
                flickerResult.valid = false;
            // ── SEND TO FFT IF CLEAN ──────────────────────────────────────
            } else if (!fdsat_analog && !fdsat_digital && !fd_sat) {
                uint16_t *pSamples = flickerSamples;
                xQueueSend(flickerQueue, &pSamples, pdMS_TO_TICKS(100));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void AS7341_Spectral_Capture_Task(void *pvParameters) {
    // Stub — full implementation in Step 5
    Serial.println("SpectralCaptureTask: started (stub)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
