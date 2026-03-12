#include "SpectralTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include <Wire.h>
#include "FFT.h"


Adafruit_AS7341 as7341; // Create an instance of the AS7341 sensor object

void initAS7341Sensor(TwoWire &wirePort) {
  unsigned long time1, time2, time3, time4;

  // Adafruit AS7341 Sensor Initialization *******************************************//
  
  //TwoWire *wirePtr = &wirePort;
  if (!as7341.begin(AS7341_I2CADDR_DEFAULT, &wirePort)) {
      Serial.println("AS7341: begin() FAILED");
      while (1) { delay(10); }
  }
  Serial.println("AS7341: begin() OK");

  

  as7341.powerEnable(true); // enable the internal oscillator
  delay(10); // wait for the oscillator to stabilize

  //Enabling the gpio_in_en (Bit 2) and gpio_out(Bit 1) in GPIO register 0xBE
  //as7341.writeRegister(AS7341_GPIO2, 0x04); // set bits 1 and clear bit 2 

  // set the INT_SEL bit in the CONFIG register to 1 
  //as7341.enableINTsel(false); // INT pin asserts when a measurement is complete (hopefully)

  // (atime + 1) * (astep + 1) * 2.78 / 1000 = integration time in ms
  as7341.setATIME(AS7341_current_ATime); // 
  as7341.setASTEP(AS7341_current_AStep); 
  // Defaults the integration time for each channel to 50 ms
  // (29 + 1) * (599 + 1) * 2.78 / 1000 = 50.04 ms  
  
  as7341.writeRegister(AS7341_WTIME, 89); // set the WTIME to 89 (250 ms wait time between measurements)

  // Need to set Spectral Threshold HIGH SP_TH_H_MSB Register (Address 0x87) and SP_TH_H_LSB Register (Address 0x86)
  // to a value that is high enough to let AGC work properly.
  as7341.writeRegister(0x87, 0xE6);               // set high threshold to 80% 0xFFFF -> CCCC
  as7341.writeRegister(0x86, 0x66);               //                       90% 0xFFFF -> E666
                                                  //                       95% 0xFFFF -> F333
                                                  //                       98% 0xFFFF -> F999

  // Need to set Spectral Threshold LOW SP_TH_L_MSB Register (Address 0x85) and SP_TH_L_LSB Register (Address 0x84)
  // to a value that is high enough to let AGC work properly.
  as7341.writeRegister(0x85, 0x19);               // set high threshold to 20% 0xFFFF -> 3333
  as7341.writeRegister(0x84, 0x99);               //                       10% 0xFFFF -> 1999
                                                  //                       5% 0xE666 -> 0CCC

  
  //as7341.setGain(AS7341_currentGain); // set a gain
  as7341.writeRegister(AS7341_CFG1, AS7341_currentGain); // set the gain in the CFG1 register

  // Enable built-in spectral AGC
uint8_t cfg8 = as7341.getRegister(AS7341_CFG8) | 0x04; // Set SP_AGC bit
as7341.writeRegister(AS7341_CFG8, cfg8);

// Configure AGC hysteresis in CFG10 (0xB3)
// Current value appears to be default - you might want to adjust
as7341.writeRegister(0xB3, 0xC2); // AGC_H=3 (87.5%), AGC_L=0 (12.5%)

// Disable threshold interrupts, enable only completion interrupts  
as7341.writeRegister(AS7341_INTENAB, 0x81); // Only ASIEN + SIEN

// Set thresholds to never trigger
as7341.writeRegister(0x84, 0x00); // Low LSB = 0
as7341.writeRegister(0x85, 0x00); // Low MSB = 0  
as7341.writeRegister(0x86, 0xFF); // High LSB = 255
as7341.writeRegister(0x87, 0xFF); // High MSB = 255 (65535 total)

 // as7341.enableSpectralAutoGainControl(false); // enable auto gain control
  as7341.enableFlickerAutoGainControl(false); // enable auto gain control

  
  // Serial.print("Timestamp: ");
  // time1 = millis();
  // Serial.println(time1);

  //configure the SMUX for the channels we want to read
  // Turns off SP_EN
  // Write 2 to CFG6 (0xAf) to set the SMUX to write from register 0x00 to 0x1F
  // Write 1 to bit 4 of the ENABLE register (0x80) to start the SMUX configuration, then
  // waits for the same bit to read 0 again.
  AS7341_SMUX_low = true; // true = F1-F4, false = F5-F8
  //as7341.setSMUXLowChannels(AS7341_SMUX_low); //F1F4_Clear_NIR

  // Enable special interrupt (SINT_SMUX). As soon as SMUX command has finished interrupt is activated.
  // Register: CFG9 / 0xB2
  as7341.writeRegister(0xB2, 0x10);
  // Enable special interrupt SIEN | Register: INTENAB / 0xF9
  as7341.writeRegister(0xF9, 0x01);
  // Write SMUX configuration from RAM to set SMUX chain | Register: CFG6 / 0xAF
  as7341.writeRegister(0xAF, 0x10);
  
  if (AS7341_SMUX_low) {
    //Serial.println("Setting SMUX to Low channels (F1-F4, Clear, NIR)");
    //as7341.my_setup_F1F4_Clear_NIR();
    as7341.setup_F1F4_Clear_NIR();
  } else {
   //Serial.println("Setting SMUX to High channels (F5-F8, Clear, NIR)");
    //as7341.my_setup_F5F8_Clear_NIR();
    as7341.setup_F5F8_Clear_NIR();
  } 

  // Start SMUX command while keeping power and wait on (SMUXEN = 1, PON = 1, WEN = 1)
  as7341.writeRegister(0x80, 0b00011001);

  // time2 = millis(); 
  // Serial.print("Time to set SMUX low channels: ");
  // Serial.println(time2 - time1);

  as7341.writeRegister(AS7341_STATUS, 0xFF); // clear all status bits

  // as7341.enableSpectralInterrupt(true); // enable spectral interrupts
  // as7341.enableFlickerDetection(false); // disable flicker detection
  // as7341.enableFlickerAutoGainControl(false); // disable flicker auto gain control

  // as7341.enableSpectralAutoGainControl(false); // enable auto gain control
  // as7341.enableWait(true); // enable wait

  // // Set the device mode. Register 0x70 requires setting the BANK bit first.
  // as7341.setBank(true); // set to BANK1
  // as7341.setMode(SYNS); // SYNC mode
  // as7341.setBank(false); // set to BANK0

  /*
  1. Enable system interrupts
  The main interrupt control register is INTENAB (0xF9).
  Each bit enables one class of interrupt:
  Bit 7: Spectral saturation / Flicker detection
  Bit 3: Spectral measurement complete (AINT)
  Bit 2: FIFO full
  Bit 1: Calibration complete
  Bit 0: System error
  */
  //as7341.writeRegister(AS7341_INTENAB, 0b10001111);

// Disable threshold interrupts, enable only completion interrupts  
as7341.writeRegister(AS7341_INTENAB, 0x81); // Only ASIEN + SIEN

  // Do a dummy check of the registers just written to.
  //printAS7341registers(); 
  
  as7341.writeRegister(AS7341_STATUS, 0xFF); // clear all status bits

  return;
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

void AS7341_Flicker_Capture_Task(void *pvParameters) {
    const TickType_t pauseCheckInterval = pdMS_TO_TICKS(50);

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
        // In AS7341_Flicker_Capture_Task, change the order:

        as7341.writeRegister(AS7341_ENABLE, 0x00);
        vTaskDelay(pdMS_TO_TICKS(5));
        

        // Set FD_TIME while FDEN=0 & PON=0
        as7341.writeRegister(AS7341_FD_TIME1, FLICKER_FD_TIME & 0xFF);  //0xD8
        as7341.writeRegister(AS7341_FD_TIME2, (as7341.getRegister(AS7341_FD_TIME2) & 0xF8) //0xDA
                                    | ((FLICKER_FD_TIME >> 8) & 0x07));
        as7341.writeRegister(0xD6, 0x01);  // autozero every cycle

        
        Serial.print("CFG0 before setupFDSmux: 0x");
        Serial.println(as7341.getRegister(0xA9), HEX);
        as7341.setupFDSmux();  // THEN configure SMUX — must not reset 0xD7
        Serial.print("CFG0 after setupFDSmux: 0x");
        Serial.println(as7341.getRegister(0xA9), HEX);
        Serial.print("FD_CFG0 after setupFDSmux: 0x");
        Serial.println(as7341.getRegister(0xD7), HEX);

         // Configure FIFO — sets FD_CFG0 bit 7
        as7341.configureFIFO_FD(true);

        // Enable 
        as7341.writeRegister(AS7341_ENABLE, 0x01);  // PON only
        vTaskDelay(pdMS_TO_TICKS(2));
        as7341.writeRegister(AS7341_ENABLE, 0b01000001);  // PON + FDEN

        // Flush
        as7341.writeRegister(AS7341_CONTROL, 0x02);
        as7341.writeRegister(AS7341_CONTROL, 0x00);

        // Diagnostics — remove after validated
        Serial.print("ENABLE: 0x");  Serial.println(as7341.getRegister(AS7341_ENABLE), HEX);
        Serial.print("FD_CFG0: 0x"); Serial.println(as7341.getRegister(0xD7), HEX);  // expect 0xA1
        Serial.print("FD_TIME1: "); Serial.println(as7341.getRegister(0xD8));         // expect 180
        Serial.print("FD_TIME2: 0x"); Serial.println(as7341.getRegister(0xDA), HEX);
        Serial.print("CFG8: 0x");   Serial.println(as7341.getRegister(0xB1), HEX);
        Serial.print("FD_AGC_MAX: 0x"); Serial.println(as7341.getRegister(0xCF) >> 4 | 0b00001111);  // expect 9
        Serial.print("STATUS6: 0x"); Serial.println(as7341.getRegister(0xA7), HEX);  // check FD_TRIG bit 3
        Serial.print("AZ_CONFIG: 0x"); Serial.println(as7341.getRegister(0xD6));  // check autozero config

        uint8_t fdt2 = as7341.getRegister(0xDA);
        uint8_t fdGain = (fdt2 >> 3) & 0x1F;  // bits 7:3
        uint8_t fdTimeMSB = fdt2 & 0b00000111;       // bits 2:0
        uint16_t fdTime = (fdTimeMSB << 2) | as7341.getRegister(0xD8);
        float gainMultiplier = 0.5f * (1 << fdGain);  // 0=0.5x, 1=1x, 2=2x, 3=4x...
        Serial.print("FD_TIME2: 0x"); Serial.print(fdt2, HEX);
        Serial.print("  FD_GAIN="); Serial.print(fdGain);
        Serial.print(" ("); Serial.print(gainMultiplier, 0); Serial.print("x)");
        Serial.print("  FD_TIME_MSB="); Serial.println(fdTimeMSB);
        Serial.print("Calculated FD_TIME(μ𝑠): "); Serial.println(fdTime*2.78);

        // ── CAPTURE 2000 SAMPLES ──────────────────────────────────────────
        uint16_t samplesCollected = 0;
        uint32_t captureStart = millis();
        const uint32_t captureTimeout = 4000; // 2× expected duration

        while (samplesCollected < FLICKER_SAMPLE_COUNT &&
               (millis() - captureStart) < captureTimeout) {

            uint8_t newSamples = as7341.readFIFO(
                &flickerSamples[samplesCollected],
                min((int)(FLICKER_SAMPLE_COUNT - samplesCollected), 64)
            );
            samplesCollected += newSamples;

            if (newSamples == 0) {
                vTaskDelay(pdMS_TO_TICKS(2)); // brief yield, FIFO fills ~every 32ms at 16-entry threshold
            }
        }

        uint32_t elapsed = millis() - captureStart;
        Serial.print("FlickerCapture: samples="); Serial.print(samplesCollected);
        Serial.print(" elapsed_ms="); Serial.println(elapsed);

        // Print first 20 samples for validation
        Serial.print("samples[0-19]: ");
        for (int i = 0; i < 20 && i < samplesCollected; i++) {
            Serial.print(flickerSamples[i]);
            Serial.print(" ");
        }
        Serial.println();

        uint16_t *pSamples = flickerSamples;
        // Send to FFT task if we got a full buffer
        if (samplesCollected == FLICKER_SAMPLE_COUNT) {
            xQueueSend(flickerQueue, &pSamples, pdMS_TO_TICKS(100));
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
