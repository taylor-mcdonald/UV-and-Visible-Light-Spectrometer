#include "SpectralTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "shared/I2CBus.h"
#include <Wire.h>

// Task handle
TaskHandle_t as7341TaskHandle = nullptr;
TaskHandle_t as7341ReadTaskHandle = nullptr;
TaskHandle_t as7341SmuxTaskHandle = nullptr;


// ISR (notify task)
void IRAM_ATTR AS7341InterruptHandler() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(as7341TaskHandle, &xHigherPriorityTaskWoken);
  
  // Yield to higher priority task if needed
  if (xHigherPriorityTaskWoken) {
      portYIELD_FROM_ISR();  // ESP32 version takes no args
  }

}

// void IRAM_ATTR AS7341InterruptHandler() {
//   AS7341sensorInterruptFlag = true; // set flag to handle AS7341 interrupts
// }

// void IRAM_ATTR onSPSensorReady() {
//   AS7341sensorReadFlag = true; // set flag for AS7341 sensor task
// }

Adafruit_AS7341 as7341; // Create an instance of the AS7341 sensor object

AS7341Reading AS7341_Buffer = {};

void initAS7341Sensor(TwoWire &wirePort) {
  unsigned long time1, time2, time3, time4;

  // Adafruit AS7341 Sensor Initialization *******************************************//

  // Setup pin to read the INT Pin.  Do not enable the interrupt yet
  pinMode(SP_RDY_PIN, INPUT_PULLUP);
  
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

  // Should be ready for an interrupt now.
  while(digitalRead(SP_RDY_PIN)==1){
    //Serial.println("Waiting on SMUX to set");
  }

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
  // xTaskCreatePinnedToCore(
  //     AS7341sensorTask,       // Function that implements the task.
  //     "AS7341sensorTask",     // Text name for the task.
  //     4096,                   // Stack size in words, not bytes.
  //     NULL,                   // Parameter passed into the task.
  //     1,                      // Priority at which the task is created.
  //     NULL,                   // Pointer to the task handle.
  //     0                       // Core where the task should run 
  //   );                 

  xTaskCreatePinnedToCore(
    AS7341InterruptTask,      // Function that implements the task.
    "AS7341InterruptTask",    // Text name for the task. 
    4096,                     // Stack size in words, not bytes.
    NULL,                     // Parameter passed into the task. 
    1,                        // Priority at which the task is created.
    &as7341TaskHandle,        // Pointer to the task handle.
    tskNO_AFFINITY            // Core where the task should run
  );

  xTaskCreatePinnedToCore(
    AS7341_Set_SMUX_Task,     // Function that implements the task.
    "AS7341 Set SMUX Task",   // Text name for the task.
    4096,                     // Stack size in words, not bytes.
    NULL,                     // Parameter passed into the task.
    1,                        // Priority at which the task is created.
    &as7341SmuxTaskHandle,    // Pointer to the task handle.
    tskNO_AFFINITY            // Core where the task should run
  );

  xTaskCreatePinnedToCore(
    AS7341_Read_Results_Task, // Function that implements the task.
    "AS7341 Read Task",     // Text name for the task.
    4096,                     // Stack size in words, not bytes.
    NULL,                     // Parameter passed into the task.
    1,                        // Priority at which the task is created.
    &as7341ReadTaskHandle,    // Pointer to the task handle.
    tskNO_AFFINITY            // Core where the task should run
  );
  return;
}

void initAS7341interrupt(){
    // Enable the required interrupts in INTENAB (0xF9)
    // Bit 7: ASIEN (saturation), Bit 3: SP_IEN (spectral), Bit 0: SIEN (system)
    as7341.writeRegister(AS7341_INTENAB, 0x89); // Enable ASIEN + SP_IEN + SIEN
    
    // Disable threshold interrupts, enable only completion interrupts  
    //as7341.writeRegister(AS7341_INTENAB, 0x81); // Only ASIEN + SIEN

    // Enable SMUX completion interrupt in CFG9 (0xB2)
    as7341.writeRegister(0xB2, 0x10); // Enable SIEN_SMUX
    
    // Clear any existing interrupt flags
    as7341.writeRegister(AS7341_STATUS, 0xFF);
  
  //Assign the AS7341 interrupt output to work as an interrupt on the ESP32
  attachInterrupt(digitalPinToInterrupt(SP_RDY_PIN), AS7341InterruptHandler, FALLING);

  xTaskNotifyGive(as7341SmuxTaskHandle);

  // Start measurment command while keeping power and wait on (SP_EN = 1, PON = 1, WEN = 1)
  //as7341.writeRegister(0x80, 0b00001011);
  return;
}

/*
The AS7341 visible spectrum sensor is a real pain.  This task needs to be broken up into multiple
to do slightly different things depending on what was previously done.  When in SPM mode, the 
sensor's INT pin will go low when a reading is ready and that is what triggers this task to run,
but only half of the spectral channels are read at a time.  So we need to keep track of which half was read last
and then read the other half next time and configuring the SMUX appropriately. 
*/
void AS7341InterruptTask(void *pvParameters) {
  uint8_t stat, stat2, stat3, stat5, stat6, FDstat, INTENABstat, SINT_FD, SINT_SMUX;
  bool ASAT, AINT, FINT, C_INT, SINT;
  bool AVALID, ASAT_DIGITAL, ASAT_ANALOG, FDSAT_ANALOG, FDSAT_DIGITAL;
  bool INT_SP_H, INT_SP_L;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(i2cMutex1, pdMS_TO_TICKS(100)) == pdTRUE) {
      mutexTakenAt = millis();
      mutexTakenBy = "AS7341 Interrupt";  // change label per task
      //Serial.println("AS7341 Interrupt detected");

      // Read ENABLE and some config registers for troubleshooting
      // uint8_t enable = as7341.getRegister(AS7341_ENABLE);
      // uint8_t atime = as7341.getRegister(AS7341_ATIME);
      // uint8_t astepL = as7341.getRegister(AS7341_ASTEP_L);
      // uint8_t astepH = as7341.getRegister(AS7341_ASTEP_H);
      // uint16_t astep = (astepH << 8) | astepL;
      // uint8_t wtime = as7341.getRegister(AS7341_WTIME); 
      // uint8_t cfg1 = as7341.getRegister(AS7341_CFG1);
      // uint8_t cfg8 = as7341.getRegister(AS7341_CFG8);
      // uint8_t cfg9 = as7341.getRegister(AS7341_CFG9);

      // Read all the interrupt status registers to clear the interrupt
      stat = as7341.getRegister(AS7341_STATUS);
      stat2 = as7341.getRegister(AS7341_STATUS2);
      // uint8_t stat3 = as7341.getRegister(AS7341_STATUS3);
      stat5 = as7341.getRegister(AS7341_STATUS5);
      // uint8_t stat6 = as7341.getRegister(AS7341_STATUS6);
      // uint8_t FDstat = as7341.getRegister(AS7341_FD_STATUS);
      // uint8_t INTENABstat = as7341.getRegister(AS7341_INTENAB);

      // // Print raw register values
      // Serial.println("Enable and Config Registers:");
      // Serial.print("ENABLE (0x80): ");  Serial.println(enable, BIN);
      // Serial.print("CFG1   (0xA9): ");  Serial.println(cfg1,   BIN);
      // Serial.print("CFG8   (0xB1): ");  Serial.println(cfg8,   BIN);
      // Serial.print("CFG9   (0xB2): ");  Serial.println(cfg9,   BIN);
      // Serial.print("ATIME  (0x81): ");  Serial.println(atime);
      // Serial.print("ASTEP(0xCA/B): ");  Serial.println(astep);
      // Serial.print("WTIME  (0x83): ");  Serial.println(wtime);
      // Serial.println("AS7341 Interrupt Status Registers:");
      // Serial.print("STATUS  (0x93): ");  Serial.println(stat,   BIN);
      // Serial.print("STATUS2 (0xA3): ");  Serial.println(stat2,  BIN);
      // Serial.print("STATUS3 (0xA4): ");  Serial.println(stat3,  BIN);
      // Serial.print("STATUS5 (0xA6): ");  Serial.println(stat5,  BIN);
      // Serial.print("STATUS6 (0xA7): ");  Serial.println(stat6,  BIN);
      // Serial.print("FDSTAT  (0xDB): ");  Serial.println(FDstat, BIN);
      // Serial.print("INTENAB (0xF9): ");  Serial.println(INTENABstat, BIN);
      mutexTakenBy = "none";
      xSemaphoreGive(i2cMutex1);
    } else {
      Serial.println("AS7341InterruptTask: mutex timeout reading status registers");
      continue;  // skip this interrupt cycle
    }

    ASAT = (stat >> 7) & 0x01; // Spectral and Flicker Detect interrupt
    AINT = (stat >> 3) & 0x01; // Spectral Channel interrupt
    FINT = (stat >> 2) & 0x01; // FIFO Buffer interrupt
    C_INT = (stat >> 1) & 0x01; // Calibration interrupt
    SINT = stat & 0x01; // System interrupt

    AVALID = (stat2 >> 6) & 0x01; // ADC data valid
    ASAT_DIGITAL = (stat2 >> 4) & 0x01; // Digital saturation
    ASAT_ANALOG = (stat2 >> 3) & 0x01; // Analog saturation
    FDSAT_ANALOG = (stat2 >> 1) & 0x01; // Flicker detect analog saturation
    FDSAT_DIGITAL = stat2 & 0x01; // Flicker detect digital saturation

    // INT_SP_H = (stat3 >> 5) & 0x01; // Spectral channel high threshold
    // INT_SP_L = (stat3 >> 4) & 0x01; // Spectral channel low threshold

    SINT_FD = (stat5 >> 3) & 0x01; // Flicker detect interrupt
    SINT_SMUX = (stat5 >> 2) & 0x01; // SMUX operation complete interrupt

    // FIFO_OV = (stat6 >> 7) & 0x01; // FIFO overflow
    // OVTEMP = (stat6 >> 5) & 0x01; // Over temperature
    // FD_TRIG = (stat6 >> 4) & 0x01; // Flicker detect trigger error
    // SP_TRIG = (stat6 >> 2) & 0x01; // Spectral channel trigger error
    // SAI_ACTIVE = (stat6 >> 1) & 0x01; // Sleep After Interrupt active
    // INT_BUSY = stat6 & 0x01; // Initialization busy

    // Check for system interrupt
    if (SINT) {
      // Serial.println("System Interrupt");
      //System interrupt indicates either a Flicker Detection or a SMUX Opertation Interrupt
      
      // Check for SMUX operation interrupt on bit 2 of STATUS 5
      if(SINT_SMUX) {
        // If this bit is set, the SMUX operation is complete and we can start a new measurement
        // What stage are we in? Maybe it doesn't matter, just start a new measurement
        // Serial.println("SMUX operation complete, starting new measurement...");
        // Clear this flag
        //as7341.writeRegister(AS7341_STATUS5, 0x04);  // clear SINT_SMUX
        //as7341.enableSpectralMeasurement(true); // start spectral measurement

        if (xSemaphoreTake(i2cMutex1, pdMS_TO_TICKS(100)) == pdTRUE) {
          mutexTakenAt = millis();
          mutexTakenBy = "AS7341 Start Measurement";  // change label per task
          as7341.writeRegister(0x80, 0b00001011);   //start measurement bit 1, keep bit 0 (PON) on and bit 3 (WAIT) on 
          mutexTakenBy = "none";
          xSemaphoreGive(i2cMutex1);
        } else {
          Serial.println("AS7341InterruptTask: mutex timeout on SMUX start");
        }  
      }

      // Check for Flicker Detection interrupt on bit 3 of STATUS 5
      //if(SINT_FD) {
        // If this bit is set, the FD_STATUS register has changed
        
        // Print for now, figure out what to do later
        // Serial.print("FD_STATUS (0xDB) reads: ");
        //Serial.println(as7341.getRegister(AS7341_FIFO_MAP), BIN);
        // printByteBinary(FDstat);
     //}
    }

    // Check for Saturation interrupt
    // if (ASAT) {
    //   Serial.println("Saturation Interrupt, Checking SATUS2 for more info... ");
    //   printByteBinary(stat2);
    // }
      // Check Bit 6 of STATUS 2 to see if a measurement was completed successfully
      if (AVALID) {
        // Serial.println("Measurement complete, reading results...");
        // Set flag to read the results in the AS7341 Read Results task
        //AS7341sensorReadFlag = true;
        xTaskNotifyGive(as7341ReadTaskHandle);
      }

      // Check other saturation conditions
      // if (ASAT_DIGITAL | ASAT_ANALOG | FDSAT_ANALOG | FDSAT_DIGITAL) {
      //   Serial.println("Saturation detected");
      //   Serial.print("ASAT_DIGITAL: ");
      //   Serial.println(ASAT_DIGITAL ? "YES" : "NO");
      //   Serial.print("ASAT_ANALOG: ");
      //   Serial.println(ASAT_ANALOG ? "YES" : "NO");
      //   Serial.print("FDSAT_ANALOG: ");
      //   Serial.println(FDSAT_ANALOG ? "YES" : "NO");
      //   Serial.print("FDSAT_DIGITAL: ");
      //   Serial.println(FDSAT_DIGITAL ? "YES" : "NO");
      // }
    

    // // Check for Spectral Channel Interrupt
    // if (AINT) {
    //   Serial.println("Spectral Channel Interrupt, Checking SATUS3 for more info... ");
    //   printByteBinary(stat3);

    //   // Check Bit 4 and 5 of STATUS 3 to see if a channel was outside the thresholds
    //   if (INT_SP_H) {
    //     Serial.println("One or more channels above high threshold");
    //     AS7341_currentGain--;
    //     if (AS7341_currentGain < 0) AS7341_currentGain = 0; // min gain is 0
    //     as7341.writeRegister(AS7341_CFG1, AS7341_currentGain);  
    //     Serial.print("Decreasing gain to: "); Serial.println(AS7341_currentGain);
    //   }
    //   if (INT_SP_L) {
    //     Serial.println("One or more channels below low threshold");
    //     AS7341_currentGain++;
    //     if (AS7341_currentGain > 10) AS7341_currentGain = 10; // max gain is 10
    //     as7341.writeRegister(AS7341_CFG1, AS7341_currentGain);
    //     Serial.print("Increasing gain to: "); Serial.println(AS7341_currentGain);
    //   }
    // }

    // Check for FIFO Buffer Interrupt
    // if (FINT) {
    //   // Serial.println("FIFO Buffer Interrupt");
    //   // Not using FIFO, so this should not happen
    // }

    // Check for Calibration Interrupt
    // if (C_INT) {
    //   Serial.println("Calibration Interrupt");
    //   // Not using Calibration, so this should not happen
    // }
    if (xSemaphoreTake(i2cMutex1, pdMS_TO_TICKS(100)) == pdTRUE) {
      mutexTakenAt = millis();
      mutexTakenBy = "AS7341 Interrupt Cleanup";  // change label per task
      // Ensure INTENAB is correct before next interrupt cycle
      as7341.writeRegister(AS7341_INTENAB, 0x89); // ASIEN + SP_IEN + SIEN

      // Disable threshold interrupts, enable only completion interrupts  
      //as7341.writeRegister(AS7341_INTENAB, 0x81); // Only ASIEN + SIEN

      // All interrupts handled, write the stat value back to the STATUS register to clear
      as7341.writeRegister(AS7341_STATUS, stat); // clear all status bits
      mutexTakenBy = "none";
      xSemaphoreGive(i2cMutex1);
    } else {
      Serial.println("AS7341InterruptTask: mutex timeout on cleanup writes");
    }
  }
}

void AS7341_Set_SMUX_Task(void *pvParameters) {
  for (;;) {
    // Wait here until AS7341InterruptTask wakes us up
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Serial.println("Setting the SMUX!");
    
    // SMUX reconfiguration is a multi-step sequence that must not be interrupted
    if (xSemaphoreTake(i2cMutex1, pdMS_TO_TICKS(200)) == pdTRUE) {
      mutexTakenAt = millis();
      mutexTakenBy = "AS7341 Set SMUX";  // change label per task
      // turn off SP_EN
      as7341.writeRegister(0x80, 0b00001001);

      // Enable special interrupt (SINT_SMUX). As soon as SMUX command has finished interrupt is activated.
      // Register: CFG9 / 0xB2
      //as7341.writeRegister(0xB2, 0x10);
      // Enable special interrupt SIEN | Register: INTENAB / 0xF9
      //as7341.writeRegister(0xF9, 0x01);

      // Disable threshold interrupts, enable only completion interrupts  
      //as7341.writeRegister(AS7341_INTENAB, 0x81); // Only ASIEN + SIEN

      // Write SMUX configuration from RAM to set SMUX chain | Register: CFG6 / 0xAF
      as7341.writeRegister(0xAF, 0x10);
      
      if (AS7341_SMUX_low) {
        // Serial.println("Setting SMUX to LOW channels (F1-F4, Clear, NIR)");
        //as7341.my_setup_F1F4_Clear_NIR();
        as7341.setup_F1F4_Clear_NIR();
      } else {
        // Serial.println("Setting SMUX to HIGH channels (F5-F8, Clear, NIR)");
        //as7341.my_setup_F5F8_Clear_NIR();
        as7341.setup_F5F8_Clear_NIR();
      } 

      // Start SMUX command while keeping power and wait on (SMUXEN = 1, PON = 1, WEN = 1)
      as7341.writeRegister(0x80, 0b00011001);

      mutexTakenBy = "none";
      xSemaphoreGive(i2cMutex1);
    } else {
      Serial.println("AS7341_Set_SMUX_Task: mutex timeout");
    }

    // Should be ready for an interrupt now.
  }
}

void AS7341_Read_Results_Task(void *pvParameters) {
  for (;;) {
    // Wait here until AS7341InterruptTask wakes us up
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(i2cMutex1, pdMS_TO_TICKS(100)) == pdTRUE) {
      mutexTakenAt = millis();
      mutexTakenBy = "AS7341 Read Results";  // change label per task

      as7341.getResults(AS7341_Buffer);

      mutexTakenBy = "none";
      xSemaphoreGive(i2cMutex1);
    } else {
      Serial.println("AS7341_Read_Results_Task: mutex timeout");
      continue;  // skip this read cycle
    }
    
    // Serial.print("Saturated: ");
    // Serial.println(AS7341_Buffer.saturation ? "YES" : "NO");

    // Serial.print("Gain code: ");
    // Serial.println(AS7341_Buffer.gain);

    // Serial.print(AS7341_SMUX_low ? "F1-F4" : "F5-F8");
    // Serial.print(AS7341_SMUX_low ? "F1: " : "F5: ");
    // Serial.println(AS7341_Buffer.F1_F5);
    // Serial.print(AS7341_SMUX_low ? "F2: " : "F6: ");
    // Serial.println(AS7341_Buffer.F2_F6);
    // Serial.print(AS7341_SMUX_low ? "F3: " : "F7: ");
    // Serial.println(AS7341_Buffer.F3_F7);
    // Serial.print(AS7341_SMUX_low ? "F4: " : "F8: ");
    // Serial.println(AS7341_Buffer.F4_F8);
    // Serial.print(AS7341_SMUX_low ? "NIR: " : "NIR: ");
    // Serial.println(AS7341_Buffer.NIR);
    // Serial.print(AS7341_SMUX_low ? "Clear: " : "Clear: ");
    // Serial.println(AS7341_Buffer.Clr);

    // Store the reading in the appropriate history buffer
    if (AS7341_SMUX_low) {
      AS7341_history_low[AS7341_historyIndex_low] = AS7341_Buffer;
      AS7341_historyIndex_low = (AS7341_historyIndex_low + 1) % AS7341_HISTORY_SIZE;
      //AS7341_Time1 = millis();
      // Serial.print("Time for low channel read: ");
      // Serial.println( AS7341_Time1 - AS7341_Time2);
    } else {
      AS7341_history_high[AS7341_historyIndex_high] = AS7341_Buffer;
      AS7341_historyIndex_high = (AS7341_historyIndex_high + 1) % AS7341_HISTORY_SIZE;
      //AS7341_Time2 = millis();
      // Serial.print("Time for high channel read: ");
      // Serial.println(AS7341_Time2 - AS7341_Time1);
    }

    AS7341_SMUX_low = !AS7341_SMUX_low; // toggle for next time
    xTaskNotifyGive(as7341SmuxTaskHandle);
  }
}
  


// void AS7341sensorTask(void *pvParameters) {
//   for (;;) {
//     if (AS7341sensorReadFlag) {
//       AS7341sensorReadFlag = false;

//        // Read all channels at the same time and store in as7341 object
//       if (!as7341.readAllChannels()){
//         Serial.println("Error reading all channels!");
//         return;
//       }

//       // --- Read AS7341 sensor here ---
//       uint16_t F1 = as7341.getChannel(AS7341_CHANNEL_415nm_F1);
//       uint16_t F2 = as7341.getChannel(AS7341_CHANNEL_445nm_F2);
//       uint16_t F3 = as7341.getChannel(AS7341_CHANNEL_480nm_F3);
//       uint16_t F4 = as7341.getChannel(AS7341_CHANNEL_515nm_F4);
//       uint16_t F5 = as7341.getChannel(AS7341_CHANNEL_555nm_F5);
//       uint16_t F6 = as7341.getChannel(AS7341_CHANNEL_590nm_F6);
//       uint16_t F7 = as7341.getChannel(AS7341_CHANNEL_630nm_F7);
//       uint16_t F8 = as7341.getChannel(AS7341_CHANNEL_680nm_F8);
//       uint16_t NIR = as7341.getChannel(AS7341_CHANNEL_NIR);
//       uint16_t Clr = as7341.getChannel(AS7341_CHANNEL_CLEAR);
//       uint16_t FLKR = as7341.detectFlickerHz();

//       as7341_gain_t gain = as7341.getGain();      // Current gain setting
//       long AS7341_IntegrationTime = as7341.getTINT(); // Current integration time

//       addAS7341Reading(F1, F2, F3, F4, F5, F6, F7, F8, NIR, Clr, FLKR, gain, AS7341_IntegrationTime);

//       //Serial.println("AS7341 data read and stored");
//     }
//     vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY)); // avoid busy loop
//   }
// }

// void printAS7341registers(void) {
//     // print the status registers for debugging
//   // ENABLE (0x80)
//   Serial.print("ENABLE (0x80) reads: ");  
//   printByteBinary(as7341.getRegister(AS7341_ENABLE));

//   // CONFIG (0x70)
//   as7341.setBank(true); // set to BANK1
//   Serial.print("CONFIG (0x70) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_CONFIG), BIN);
//   printByteBinary(as7341.getRegister(AS7341_CONFIG));
//   as7341.setBank(false); // set to BANK0

//   // ATIME (0x81)
//   Serial.print("ATIME (0x81) reads: ");
//   Serial.println(as7341.getRegister(AS7341_ATIME));
//   // ASTEP (0x84, 0x85)
//   Serial.print("ASTEP (0xCA, 0xCb) reads: "); 
//   Serial.println((as7341.getRegister(AS7341_ASTEP_H) << 8) | as7341.getRegister(AS7341_ASTEP_L));
//   // WTIME (0x83)
//   Serial.print("WTIME (0x83) reads: ");
//   Serial.println(as7341.getRegister(AS7341_WTIME));
//   // INTENAB (0xF9)
//   Serial.print("INTENAB (0xF9) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_INTENAB), BIN);
//   printByteBinary(as7341.getRegister(AS7341_INTENAB));

//   //CONTROL (0xFA)
//   Serial.print("CONTROL (0xFA) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_CONTROL), BIN);
//   printByteBinary(as7341.getRegister(AS7341_CONTROL));

//   // AGC_GAIN_MAX (0xCF)
//   Serial.print("AGC_GAIN_MAX (0xCF) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_AGC_GAIN_MAX), BIN);
//   printByteBinary(as7341.getRegister(AS7341_AGC_GAIN_MAX));

//   // CFG0 (0xA9)
//   Serial.print("CFG0 (0xA9) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_CFG0), BIN);
//   printByteBinary(as7341.getRegister(AS7341_CFG0));

//   // AGAIN CFG1 (0xAA  bit 4:0)
//   Serial.print("AGAIN (0xAA) reads: ");
//   uint8_t CFG1 = as7341.getRegister(AS7341_CFG1);
//   //Serial.print(CFG1, BIN);
//   printByteBinary(CFG1);
//   Serial.print("  Gain: ");
//   Serial.println(CFG1 & 0x1F);

//   // CFG3 (0xAC)
//   Serial.print("CGF3 (0xAA) reads: ");
//   printByteBinary(as7341.getRegister(AS7341_CFG3));

//   // CFG8 (0xB1)
//   Serial.print("CFG8 (0xB1) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_CFG8), BIN);
//   printByteBinary(as7341.getRegister(AS7341_CFG8));

//   // GPIO2 (0xBE)
//   Serial.print("GPIO2 (0xBE) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_GPIO2), BIN);
//   printByteBinary(as7341.getRegister(AS7341_GPIO2));

//   // STAT (0x71)
//   Serial.print("STAT (0x71) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STAT), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STAT));

//   // STATUS (0x93)
//   Serial.print("STATUS (0x93) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STATUS), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STATUS));

//   // STATUS2 (0xA3)
//   Serial.print("STATUS2 (0xA3) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STATUS2), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STATUS2));

//   // STATUS3 (0xA4)
//   Serial.print("STATUS3 (0xA4) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STATUS3), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STATUS3));

//   // STATUS5 (0xA6)
//   Serial.print("STATUS5 (0xA6) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STATUS5), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STATUS5));

//   // STATUS6 (0xA7)
//   Serial.print("STATUS6 (0xA7) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_STATUS6), BIN);
//   printByteBinary(as7341.getRegister(AS7341_STATUS6));

//   // FIFO_MAP (0xFC)
//   Serial.print("FIFO_MAP (0xFC) reads: ");
//   //Serial.println(as7341.getRegister(AS7341_FIFO_MAP), BIN);
//   printByteBinary(as7341.getRegister(AS7341_FIFO_MAP));



//   return;
// }


// void testAS7341_INT_simple() {
//   const int intPin = SP_RDY_PIN;
//   pinMode(intPin, INPUT_PULLUP);

//   Serial.println(F("\n=== AS7341 INT test start ==="));

//   // 1) Disable any AGC/SP_AGC/flicker AGC for the test
//   // (set CFG8 to 0 -- conservative test; adjust if your hardware needs certain bits)
//   as7341.writeRegister(0xB1, 0x00); // CFG8 -> 0 (turn off spectral AGC/flicker AGC)
//   Serial.print("CFG8 now reads: ");
//   printByteBinary(as7341.getRegister(0xB1));

//   // 2) Enable only ADC-ready interrupt (AINT = bit 3)
//   as7341.writeRegister(0xF9, 0x08); // INTENAB = 0x08 (AINT only)
//   Serial.print("INTENAB set to: ");
//   printByteBinary(as7341.getRegister(0xF9));

//   // 3) Set persistence (trigger on every measurement)
//   as7341.writeRegister(0x0C, 0x01); // PERS = 1
//   Serial.print("PERS readback: ");
//   printByteBinary(as7341.getRegister(0x0C));

//   // 4) Short integration time to make test quick
//   as7341.setATIME(10);
//   as7341.setASTEP(50);
//   Serial.print("ATIME: "); Serial.println(as7341.getATIME());
//   Serial.print("ASTEP: "); Serial.println(as7341.getASTEP());

//   // 5) Start measurement (spectral)
//   as7341.enableSpectralMeasurement(true);
//   Serial.println("Spectral measurement started...");

//   // 6) Poll registers and INT pin for a few seconds
//   for (int i = 0; i < 20; ++i) {
//     uint8_t astat = as7341.getRegister(0x94);   // ASTATUS / ASTATUS
//     uint8_t s2    = as7341.getRegister(0x96);   // STATUS2
//     uint8_t s3    = as7341.getRegister(0xA4);   // STATUS3
//     uint8_t stat  = as7341.getRegister(0x71);   // STAT (if your driver uses this)
//     int pinState = digitalRead(intPin);

//     Serial.print("loop "); Serial.print(i);
//     Serial.print(" | ASTAT: "); printByteBinary(astat);
//     Serial.print(" | STATUS2: "); printByteBinary(s2);
//     Serial.print(" | STATUS3: "); printByteBinary(s3);
//     Serial.print(" | STAT: "); printByteBinary(stat);
//     Serial.print(" | INT pin: "); Serial.println(pinState ? "HIGH" : "LOW");

//     delay(250);
//   }

//   as7341.enableSpectralMeasurement(false);
//   Serial.println("Spectral measurement stopped.");
//   Serial.println("=== AS7341 INT test end ===\n");
// }
