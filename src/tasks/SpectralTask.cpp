#include "SpectralTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include <Wire.h>


Adafruit_AS7341 as7341; // Create an instance of the AS7341 sensor object

AS7341Reading AS7341_Buffer = {};

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
    // Create queues for passing flicker buffers to FFT task
    // Queue depth 2 — if FFT falls behind, drop oldest
    flickerLowQueue  = xQueueCreate(2, sizeof(FlickerBuffer));
    flickerHighQueue = xQueueCreate(2, sizeof(FlickerBuffer));

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

    Serial.println("FlickerCaptureTask: task entered");  // before delay
    // Wait for sensor to be fully initialized
    vTaskDelay(pdMS_TO_TICKS(2000));

    Serial.println("FlickerCaptureTask: starting");

    for (;;) {
        // Check if spectral capture wants the sensor
        if (spectralCaptureInProgress) {
            Serial.println("FlickerCaptureTask: pausing for spectral capture");
            // Wait until spectral capture releases the sensor
            while (spectralCaptureInProgress) {
                vTaskDelay(pauseCheckInterval);
            }
            Serial.println("FlickerCaptureTask: resuming");
        }

            // Stop any ongoing spectral measurement
            as7341.writeRegister(AS7341_ENABLE, 0x01); // PON only

            // ── LOW FREQUENCY CAPTURE (~50ms integration, 40 samples) ──────────
            as7341.setATIME(FLICKER_LOW_ATIME);
            as7341.setASTEP(FLICKER_LOW_ASTEP);
            
            // Configure SMUX for FD photodiode
            as7341.setupFDSmux();

            as7341.configureFIFO(true);

            // Enable spectral measurement (SP_EN + PON + WEN)
            as7341.writeRegister(AS7341_ENABLE, 0x0B);

            Serial.print("ATIME: "); 
            Serial.println(as7341.getATIME());
            Serial.print("ASTEP: "); 
            Serial.println(as7341.getASTEP());
            Serial.print("FIFO_MAP: "); 
            Serial.println(as7341.getRegister(AS7341_FIFO_MAP), HEX);
            Serial.print("CFG0: "); 
            Serial.println(as7341.getRegister(AS7341_CFG0), HEX);
            Serial.print("ENABLE: "); 
            Serial.println(as7341.getRegister(AS7341_ENABLE), HEX);

            uint16_t samplesCollected = 0;
            uint32_t captureStart = millis();
            const uint32_t lowCaptureTimeout = 3000; // 3 second timeout

            while (samplesCollected < FLICKER_LOW_SAMPLE_COUNT &&
                   (millis() - captureStart) < lowCaptureTimeout) {

                vTaskDelay(pdMS_TO_TICKS(45)); // wait ~1 integration period
                

                uint8_t newSamples = as7341.readFIFO(
                    &flickerLowBuf.samples[samplesCollected],
                    FLICKER_LOW_SAMPLE_COUNT - samplesCollected
                );
                samplesCollected += newSamples;
                
                // LOW FREQUENCY loop - add inside the while loop:
                Serial.print("low drain: newSamples=");
                Serial.print(newSamples);
                Serial.print(" total=");
                Serial.print(samplesCollected);
                Serial.print(" fifo_lvl=");
                Serial.println(as7341.getRegister(AS7341_FIFO_LVL));
            }

            Serial.print("FlickerCapture: low samples collected: ");
            Serial.println(flickerLowBuf.count);

            // Also print elapsed time after low capture:
            Serial.print("low capture elapsed ms: ");
            Serial.println(millis() - captureStart);

            flickerLowBuf.count      = samplesCollected;
            flickerLowBuf.isLowFreq  = true;

            // ── HIGH FREQUENCY CAPTURE (~1ms integration, 1000 samples) ─────────
            as7341.setATIME(FLICKER_HIGH_ATIME);
            as7341.setASTEP(FLICKER_HIGH_ASTEP);
            as7341.configureFIFO(true); // clears FIFO and reconfigures

            // Re-enable measurement
            as7341.writeRegister(AS7341_ENABLE, 0x0B);


            samplesCollected = 0;
            captureStart = millis();
            const uint32_t highCaptureTimeout = 3000; // 3 second timeout

            while (samplesCollected < FLICKER_HIGH_SAMPLE_COUNT &&
                   (millis() - captureStart) < highCaptureTimeout) {

                vTaskDelay(pdMS_TO_TICKS(5)); // shorter wait for high freq

                // FIFO holds max 64 samples — drain frequently
                uint8_t newSamples = as7341.readFIFO(
                    &flickerHighBuf.samples[samplesCollected],
                    min((int)(FLICKER_HIGH_SAMPLE_COUNT - samplesCollected), 64)
                );
                samplesCollected += newSamples;
            }

            Serial.print("FlickerCapture: high samples collected: ");
            Serial.println(flickerHighBuf.count);

            flickerHighBuf.count     = samplesCollected;
            flickerHighBuf.isLowFreq = false;

        // Post buffers to FFT queue — don't block long, FFT task should keep up
        if (flickerLowBuf.count > 0) {
            xQueueSend(flickerLowQueue, &flickerLowBuf, pdMS_TO_TICKS(100));
        }
        if (flickerHighBuf.count > 0) {
            xQueueSend(flickerHighQueue, &flickerHighBuf, pdMS_TO_TICKS(100));
        }

        // Small yield before next capture cycle
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AS7341_FFT_Task(void *pvParameters) {
    // Stub — full implementation in Step 4
    Serial.println("FFTTask: started (stub)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void AS7341_Spectral_Capture_Task(void *pvParameters) {
    // Stub — full implementation in Step 5
    Serial.println("SpectralCaptureTask: started (stub)");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
