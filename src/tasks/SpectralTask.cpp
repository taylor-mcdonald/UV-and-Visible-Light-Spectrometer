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

    // AZ_CONFIG (0xD6): autozero frequency for spectral ADC
    // 0xFF = only before first measurement (default)
    // 0x01 = every cycle
    as7341.writeRegister(AS7341_AZ_CONFIG, 0x0F);

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
    // Full stop — FDEN=0 and PON=0 required before writing
    // FD_TIME1 (0xD8) and FD_TIME2 (0xDA) per datasheet
    as7341.writeRegister(AS7341_ENABLE, 0x00);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Set flicker integration time — FLICKER_FD_TIME=180 → 500μs per sample → 2000 samples/sec
    // FD_TIME2 (0xDA) bits 7:3 = FD_GAIN, bits 2:0 = FD_TIME MSB (always 0 for FD_TIME=180)
    as7341.writeRegister(AS7341_FD_TIME1, FLICKER_FD_TIME & 0xFF);
    as7341.writeRegister(AS7341_FD_TIME2, (fdGain << 3) & 0xF8);

    // AZ_CONFIG (0xD6): set to 0 to disable autozero when FDEN=1
    // If non-zero, autozero fires on ADC5 and interrupts flicker detection
    as7341.writeRegister(AS7341_AZ_CONFIG, 0x00);

    // CFG8 (0xB1):
    // - Clear AS7431_FLICKER_AUTO_GAIN (bit 3) — hardware FD AGC non-functional without SP_EN
    // - Clear AS7431_SPECTRAL_AUTO_GAIN (bit 2) — SP_EN never set in flicker mode
    // - FIFO_TH (bits 7:6) = 3 → 16-entry threshold before FINT asserts
    uint8_t cfg8 = as7341.getRegister(AS7341_CFG8);
    cfg8 &= ~AS7341_FLICKER_AUTO_GAIN;
    cfg8 &= ~AS7341_SPECTRAL_AUTO_GAIN;
    cfg8  = (cfg8 & 0x3F) | (3 << 6);
    as7341.writeRegister(AS7341_CFG8, cfg8);

    // AGC_GAIN_MAX (0xCF) bits 7:4 = AGC_FD_GAIN_MAX — cap hardware AGC ceiling at 9 (256x)
    // Software AGC uses this same range: 0 (0.5x) through 9 (256x)
    uint8_t agcMax = as7341.getRegister(AS7341_AGC_GAIN_MAX);
    as7341.writeRegister(AS7341_AGC_GAIN_MAX, (agcMax & 0x0F) | (9 << 4));

    // No interrupts needed — FlickerCaptureTask polls FIFO_LVL (0xFD) directly
    as7341.writeRegister(AS7341_INTENAB, 0x00);

    // Clear all status flags before starting
    as7341.writeRegister(AS7341_STATUS, 0xFF);

    // PON only — SMUX configuration must happen before FDEN is set
    as7341.writeRegister(AS7341_ENABLE, 0x01);
    vTaskDelay(pdMS_TO_TICKS(5));

    // Configure SMUX to route flicker photodiode to FD channel
    as7341.setupFDSmux();

    // Configure FIFO — sets FIFO_WRITE_FD (FD_CFG0 0xD7 bit 7) so flicker
    // data routes to FIFO, sets FIFO_TH in CFG8 (0xB1)
    as7341.configureFIFO_FD(true);

    // FDEN + PON only — SP_EN must never be set in flicker capture mode
    as7341.writeRegister(AS7341_ENABLE, 0x41);

    // Flush any stale data from FIFO before capture begins
    as7341.writeRegister(AS7341_CONTROL, 0x02);
    as7341.writeRegister(AS7341_CONTROL, 0x00);
}

void AS7341_Flicker_Capture_Task(void *pvParameters) {
    const TickType_t pauseCheckInterval = pdMS_TO_TICKS(50);

    // Starting gain for software AGC — 32x (gain=6) is appropriate for
    // indoor office lighting. AGC will adjust up or down each cycle.
    uint8_t fdGain = 6;

    Serial.println("FlickerCaptureTask: task entered");
    vTaskDelay(pdMS_TO_TICKS(2000));
    Serial.println("FlickerCaptureTask: starting");

    for (;;) {
        // SpectralCaptureTask sets spectralCaptureInProgress when it needs
        // the sensor — yield until it is done
        if (spectralCaptureInProgress) {
            Serial.println("FlickerCaptureTask: pausing for spectral capture");
            while (spectralCaptureInProgress) {
                vTaskDelay(pauseCheckInterval);
            }
            Serial.println("FlickerCaptureTask: resuming");
        }

        // Configure sensor for flicker capture, applying current AGC gain.
        // setupForFlicker() does a full stop/restart cycle so FD_TIME2 (0xDA)
        // can be written safely.
        setupForFlicker(fdGain);

        // ── CAPTURE 2048 SAMPLES ──────────────────────────────────────────
        // Poll FIFO until we have a full buffer or timeout.
        // At 2000 samples/sec with FIFO_TH=16, FIFO fills every ~8ms.
        // captureTimeout is 2× expected duration as a safety net.
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

        if (samplesCollected == FLICKER_SAMPLE_COUNT) {
            // ── READ SATURATION FLAGS ─────────────────────────────────────
            // STATUS2 (0xA3) bit 1 = FDSAT_ANALOG: photodiode analog circuit
            //   overloaded — light too intense for sensor physically
            // STATUS2 (0xA3) bit 0 = FDSAT_DIGITAL: counter maxed out —
            //   gain too high
            // FD_STATUS (0xDB) bit 4 = FD_SAT: general flicker saturation
            uint8_t status2  = as7341.getRegister(AS7341_STATUS2);
            uint8_t fdStatus = as7341.getRegister(AS7341_FD_STATUS);

            bool fdsat_analog  = (status2  >> 1) & 0x01;
            bool fdsat_digital = (status2  >> 0) & 0x01;
            bool fd_sat        = (fdStatus >> 4) & 0x01;

            // ── COMPUTE PEAK ──────────────────────────────────────────────
            // Peak sample value drives AGC and weak-signal detection.
            // At gain=9 (256x), ADC ceiling is ~181 counts under normal
            // conditions; analog saturation pegs samples at 437.
            uint16_t peakVal = 0;
            for (int i = 0; i < FLICKER_SAMPLE_COUNT; i++) {
                if (flickerSamples[i] > peakVal) peakVal = flickerSamples[i];
            }

            // ── SOFTWARE AGC ──────────────────────────────────────────────
            // Hardware FD AGC (CFG8 bit 3) is non-functional without SP_EN.
            // Gain is adjusted here and applied at the top of the next cycle
            // via setupForFlicker(fdGain). Gain range: 0 (0.5x) to 9 (256x).
            // Target peak range: 60–160 counts.
            if (fdsat_analog && fdGain > 0) {
                // Analog overload — reduce gain aggressively even though
                // the photodiode itself is saturated
                flickerResult.valid = false;
                fdGain -= 2;
            } else if (fdsat_digital && fdGain > 0) {
                // Digital counter overflow — gain too high
                fdGain -= 2;
            } else if (fd_sat && fdGain > 0) {
                // General saturation — back off gently
                fdGain -= 1;
            } else if (peakVal < 20 && fdGain < 9) {
                // Signal very weak — increase gain aggressively
                fdGain += 2;
            } else if (peakVal < 60 && fdGain < 9) {
                // Signal below target range — increase gain
                fdGain += 1;
            } else if (peakVal > 160 && fdGain > 0) {
                // Signal above target range — reduce gain
                fdGain -= 1;
            }
            fdGain = constrain(fdGain, 0, 9);

            // ── SEND TO FFT ───────────────────────────────────────────────
            // Discard captures that are too weak (noise floor) or saturated.
            // FFT task receives a pointer to flickerSamples in PSRAM.
            if (peakVal < 10) {
                // Signal at quantization noise floor — FFT would find spurious peaks
                flickerResult.valid = false;
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
