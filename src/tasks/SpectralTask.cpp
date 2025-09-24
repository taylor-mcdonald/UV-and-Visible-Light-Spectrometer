#include "SpectralTask.h"

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

void initAS7341Sensor(void) {
  unsigned long time1, time2, time3, time4;

  // Adafruit AS7341 Sensor Initialization *******************************************//


  //Setup a PWM signal on the chosen pin to drive the AS7341's SYNC pin
  // 40 Hz frequency, 13-bit resolution is fine (0-8191)
  const double freq = 40;         // Hz
  const int resolution = 13;      // bits

  // Configure LEDC timer
  ledcSetup(PWM_CHANNEL, freq, resolution);

  // Attach the channel to a pin
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);

  // Compute duty (80% of 8191 = 6553)
  int duty = (int)((8191 * 20.0) / 25.0);  

  // Set duty
  ledcWrite(PWM_CHANNEL, duty);
  
  if (!as7341.begin()){
    Serial.println("Could not find AS7341");
    while (1) { delay(10); }
  }

  as7341.powerEnable(true); // enable the internal oscillator
  delay(10); // wait for the oscillator to stabilize

  //Enabling the gpio_in_en (Bit 2) and gpio_out(Bit 1) in GPIO register 0xBE
  as7341.writeRegister(AS7341_GPIO2, 0x04); // set bits 1 and clear bit 2 

  // set the INT_SEL bit in the CONFIG register to 1 
  //as7341.enableINTsel(false); // INT pin asserts when a measurement is complete (hopefully)

  // (atime + 1) * (astep + 1) * 2.78 / 1000 = integration time in ms
  as7341.setATIME(29); // 
  as7341.setASTEP(599); 
  // Set the integration time for each channel to 50 ms
  // (29 + 1) * (599 + 1) * 2.78 / 1000 = 50.04 ms  
  
  as7341.writeRegister(AS7341_WTIME, 89); // set the WTIME to 89 (250 ms wait time between measurements)
  
  as7341.setGain(AS7341_GAIN_1X); // set a gain

  as7341.enableSpectralAutoGainControl(true); // enable auto gain control
  as7341.enableFlickerAutoGainControl(false); // enable auto gain control

  
  Serial.print("Timestamp: ");
  time1 = millis();
  Serial.println(time1);

  //configure the SMUX for the channels we want to read
  // Turns off SP_EN
  // Write 2 to CFG6 (0xAf) to set the SMUX to write from register 0x00 to 0x1F
  // Write 1 to bit 4 of the ENABLE register (0x80) to start the SMUX configuration, then
  // waits for the same bit to read 0 again.
  AS7341_SMUX_low = true; // true = F1-F4, false = F5-F8
  as7341.setSMUXLowChannels(AS7341_SMUX_low); //F1F4_Clear_NIR
  time2 = millis(); 
  Serial.print("Time to set SMUX low channels: ");
  Serial.println(time2 - time1);

  as7341.enableSpectralInterrupt(true); // enable spectral interrupts
  as7341.enableFlickerDetection(false); // disable flicker detection
  as7341.enableFlickerAutoGainControl(false); // disable flicker auto gain control

  as7341.enableSpectralAutoGainControl(true); // enable auto gain control
  as7341.enableWait(true); // enable wait

  // Set the device mode. Register 0x70 requires setting the BANK bit first.
  as7341.setBank(true); // set to BANK1
  as7341.setMode(SYNS); // SYNC mode
  as7341.setBank(false); // set to BANK0

  // Do a dummy check of the registers just written to.
  // Serial.print("CFG8 (0xB1) reads: ");
  // Serial.println(as7341.getRegister(AS7341_CFG8), BIN);
  // Serial.print("Bit 2 = ");
  // Serial.println(as7341.getSP_AGC() ? "SP_AGC ON" : "SP_AGC OFF");
  printAS7341registers(); 
  
  as7341.writeRegister(AS7341_STATUS, 0xFF); // clear all status bits
  Serial.println("Starting first measurement...");
  // Start the first measurement
  time1 = millis();
  as7341.enableSpectralMeasurement(true); // start spectral measurement

  delay(10);

  // Wait for the first measurement to complete
  while (digitalRead(SP_RDY_PIN) == LOW) {
    delay(1);
  } 
  time2 = millis();
  Serial.print("Time for first measurement: ");
  Serial.println(time2 - time1);

  // get the results from the first measurement
  // get the saturation status and gain from the ASTATUS register
  // uint8_t astat = as7341.getRegister(AS7341_ASTATUS_);

  // Serial.print("Saturation status from ASTAT: ");
  // Serial.println(astat, BIN);
  // Serial.print("  Bit 7 = ");
  // Serial.println((astat & 0x80) ? "SATURATED" : "NOT SATURATED");
  // Serial.print("Gain: ");
  // Serial.println(astat & 0x0F);

  as7341.getResults(AS7341_Buffer);
  
  Serial.print("Saturated: ");
  Serial.println(AS7341_Buffer.saturation ? "YES" : "NO");

  Serial.print("Gain code: ");
  Serial.println(AS7341_Buffer.gain);

  Serial.print(AS7341_SMUX_low ? "F1-F4" : "F5-F8");
  Serial.print(AS7341_SMUX_low ? "F1: " : "F5: ");
  Serial.println(AS7341_Buffer.F1_F5);
  Serial.print(AS7341_SMUX_low ? "F2: " : "F6: ");
  Serial.println(AS7341_Buffer.F2_F6);
  Serial.print(AS7341_SMUX_low ? "F3: " : "F7: ");
  Serial.println(AS7341_Buffer.F3_F7);
  Serial.print(AS7341_SMUX_low ? "F4: " : "F8: ");
  Serial.println(AS7341_Buffer.F4_F8);
  Serial.print(AS7341_SMUX_low ? "NIR: " : "NIR: ");
  Serial.println(AS7341_Buffer.NIR);
  Serial.print(AS7341_SMUX_low ? "Clear: " : "Clear: ");
  Serial.println(AS7341_Buffer.Clr);


  // print the rest of the status registers for debugging
  printAS7341registers(); 

  // Configure the SMUX for the other channels
  //as7341.enableSpectralMeasurement(false); // stop spectral measurement
  //time1 = millis();
  AS7341_SMUX_low = false; // true = F1-F4, false = F5-F8
  as7341.setSMUXLowChannels(AS7341_SMUX_low); //F1F4_Clear_NIR
  //time2 = millis(); 
  //Serial.print("Time to set SMUX high channels: ");
  //Serial.println(time2 - time1);
 
  //as7341.enableFlickerDetection(false); // disable flicker detection
  //as7341.enableFlickerAutoGainControl(false); // disable flicker auto gain control
 // as7341.enableSpectralAutoGainControl(true); // enable auto gain control

  time3 = millis();
  Serial.print("Ready for second measurement...  ");
  Serial.println(time3-time1);
  // Start the first measurement
  //as7341.enableSpectralMeasurement(true); // start spectral measurement

  // Wait for the first measurement to complete
  while (digitalRead(SP_RDY_PIN) == LOW) {
    delay(1);
  } 
  time4 = millis();
  Serial.print("Time for second measurement: ");
  Serial.println(time4 - time3);
  Serial.print("Time from start of first measurment to start of second measurement: ");
  Serial.println(time3 - time1);

  // get the results from the measurement
  // get the saturation status and gain from the ASTATUS register
  //astat = as7341.getRegister(AS7341_ASTATUS_);

  as7341.getResults(AS7341_Buffer);
  
  Serial.print("Saturated: ");
  Serial.println(AS7341_Buffer.saturation ? "YES" : "NO");

  Serial.print("Gain code: ");
  Serial.println(AS7341_Buffer.gain);

  Serial.print(AS7341_SMUX_low ? "F1-F4" : "F5-F8");
  Serial.print(AS7341_SMUX_low ? "F1: " : "F5: ");
  Serial.println(AS7341_Buffer.F1_F5);
  Serial.print(AS7341_SMUX_low ? "F2: " : "F6: ");
  Serial.println(AS7341_Buffer.F2_F6);
  Serial.print(AS7341_SMUX_low ? "F3: " : "F7: ");
  Serial.println(AS7341_Buffer.F3_F7);
  Serial.print(AS7341_SMUX_low ? "F4: " : "F8: ");
  Serial.println(AS7341_Buffer.F4_F8);
  Serial.print(AS7341_SMUX_low ? "NIR: " : "NIR: ");
  Serial.println(AS7341_Buffer.NIR);
  Serial.print(AS7341_SMUX_low ? "Clear: " : "Clear: ");
  Serial.println(AS7341_Buffer.Clr);

  printAS7341registers();


  delay(1000);

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
  //Assign the AS7341 interrupt output to work as an interrupt on the ESP32
  pinMode(SP_RDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SP_RDY_PIN), AS7341InterruptHandler, RISING);
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
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    //Serial.println("AS7341 Interrupt detected");

    // Read all the interrupt status registers to clear the interrupt
    uint8_t stat = as7341.getRegister(AS7341_STAT);
    uint8_t stat2 = as7341.getRegister(AS7341_STATUS2);
    uint8_t stat3 = as7341.getRegister(AS7341_STATUS3);
    uint8_t stat5 = as7341.getRegister(AS7341_STATUS5);
    uint8_t stat6 = as7341.getRegister(AS7341_STATUS6);
    uint8_t FDstat = as7341.getRegister(AS7341_FD_STATUS);

    bool ASAT = (stat >> 7) & 0x01; // Spectral and Flicker Detect interrupt
    bool AINT = (stat >> 3) & 0x01; // Spectral Channel interrupt
    bool FINT = (stat >> 2) & 0x01; // FIFO Buffer interrupt
    bool C_INT = (stat >> 1) & 0x01; // Calibration interrupt
    bool SINT = stat & 0x01; // System interrupt

    bool AVALID = (stat2 >> 6) & 0x01; // ADC data valid
    bool ASAT_DIGITAL = (stat2 >> 4) & 0x01; // Digital saturation
    bool ASAT_ANALOG = (stat2 >> 3) & 0x01; // Analog saturation
    bool FDSAT_ANALOG = (stat2 >> 1) & 0x01; // Flicker detect analog saturation
    bool FDSAT_DIGITAL = stat2 & 0x01; // Flicker detect digital saturation

    bool INT_SP_H = (stat3 >> 5) & 0x01; // Spectral channel high threshold
    bool INT_SP_L = (stat3 >> 4) & 0x01; // Spectral channel low threshold

    bool SINT_FD = (stat5 >> 3) & 0x01; // Flicker detect interrupt
    bool SINT_SMUX = (stat5 >> 2) & 0x01; // SMUX operation complete interrupt

    bool FIFO_OV = (stat6 >> 7) & 0x01; // FIFO overflow
    bool OVTEMP = (stat6 >> 5) & 0x01; // Over temperature
    bool FD_TRIG = (stat6 >> 4) & 0x01; // Flicker detect trigger error
    bool SP_TRIG = (stat6 >> 2) & 0x01; // Spectral channel trigger error
    bool SAI_ACTIVE = (stat6 >> 1) & 0x01; // Sleep After Interrupt active
    bool INT_BUSY = stat6 & 0x01; // Initialization busy

    // Check for system interrupt
    if (SINT) {
      // Serial.println("System Interrupt");
      //System interrupt indicates either a Flicker Detection or a SMUX Opertation Interrupt
      
      // Check for SMUX operation interrupt on bit 2 of STATUS 5
      if(SINT_SMUX) {
        // If this bit is set, the SMUX operation is complete and we can start a new measurement
        // What stage are we in? Maybe it doesn't matter, just start a new measurement
        Serial.println("SMUX operation complete, starting new measurement...");
        as7341.enableSpectralMeasurement(true); // start spectral measurement
      }

      // Check for Flicker Detection interrupt on bit 3 of STATUS 5
      if(SINT_FD) {
        // If this bit is set, the FD_STATUS register has changed
        
        // Print for now, figure out what to do later
        Serial.print("FD_STATUS (0xDB) reads: ");
        //Serial.println(as7341.getRegister(AS7341_FIFO_MAP), BIN);
        printByteBinary(FDstat);
      }
    }

    // Check for Saturation interrupt
    if (ASAT) {
      Serial.println("Saturation Interrupt, Checking SATUS2 for more info... ");
      printByteBinary(stat2);

      // Check Bit 6 of STATUS 2 to see if a measurement was completed successfully
      if (AVALID) {
        Serial.println("Measurement complete, reading results...");
        // Set flag to read the results in the AS7341 Read Results task
        //AS7341sensorReadFlag = true;
        xTaskNotifyGive(as7341ReadTaskHandle);
      }

      // Check other saturation conditions
      if (ASAT_DIGITAL | ASAT_ANALOG | FDSAT_ANALOG | FDSAT_DIGITAL) {
        Serial.println("Saturation detected");
        Serial.print("ASAT_DIGITAL: ");
        Serial.println(ASAT_DIGITAL ? "YES" : "NO");
        Serial.print("ASAT_ANALOG: ");
        Serial.println(ASAT_ANALOG ? "YES" : "NO");
        Serial.print("FDSAT_ANALOG: ");
        Serial.println(FDSAT_ANALOG ? "YES" : "NO");
        Serial.print("FDSAT_DIGITAL: ");
        Serial.println(FDSAT_DIGITAL ? "YES" : "NO");
      }
    }

    // Check for Spectral Channel Interrupt
    if (AINT) {
      Serial.println("Spectral Channel Interrupt, Checking SATUS3 for more info... ");
      printByteBinary(stat3);

      // Check Bit 4 and 5 of STATUS 3 to see if a channel was outside the thresholds
      if (INT_SP_H) {
        Serial.println("One or more channels above high threshold");
      }
      if (INT_SP_L) {
        Serial.println("One or more channels below low threshold");
      }
    }

    // Check for FIFO Buffer Interrupt
    if (FINT) {
      Serial.println("FIFO Buffer Interrupt");
      // Not using FIFO, so this should not happen
    }

    // Check for Calibration Interrupt
    if (C_INT) {
      Serial.println("Calibration Interrupt");
      // Not using Calibration, so this should not happen
    }
  
  // All interrupts handled, write the stat value back to the STATUS register to clear
  as7341.writeRegister(AS7341_STAT, stat); // clear all status bits

  }
}

void AS7341_Set_SMUX_Task(void *pvParameters) {
  for (;;) {
    // Wait here until AS7341InterruptTask wakes us up
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

      if (AS7341_SMUX_low) {
        Serial.println("Setting SMUX to HIGH channels (F1-F4, Clear, NIR)");
      } else {
        Serial.println("Setting SMUX to LOW channels (F5-F8, Clear, NIR)");
      }
      as7341.setSMUX(AS7341_SMUX_low);  
    }
}

void AS7341_Read_Results_Task(void *pvParameters) {
  for (;;) {
    // Wait here until AS7341InterruptTask wakes us up
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    as7341.getResults(AS7341_Buffer);
    
    Serial.print("Saturated: ");
    Serial.println(AS7341_Buffer.saturation ? "YES" : "NO");

    Serial.print("Gain code: ");
    Serial.println(AS7341_Buffer.gain);

    Serial.print(AS7341_SMUX_low ? "F1-F4" : "F5-F8");
    Serial.print(AS7341_SMUX_low ? "F1: " : "F5: ");
    Serial.println(AS7341_Buffer.F1_F5);
    Serial.print(AS7341_SMUX_low ? "F2: " : "F6: ");
    Serial.println(AS7341_Buffer.F2_F6);
    Serial.print(AS7341_SMUX_low ? "F3: " : "F7: ");
    Serial.println(AS7341_Buffer.F3_F7);
    Serial.print(AS7341_SMUX_low ? "F4: " : "F8: ");
    Serial.println(AS7341_Buffer.F4_F8);
    Serial.print(AS7341_SMUX_low ? "NIR: " : "NIR: ");
    Serial.println(AS7341_Buffer.NIR);
    Serial.print(AS7341_SMUX_low ? "Clear: " : "Clear: ");
    Serial.println(AS7341_Buffer.Clr);

    // Store the reading in the appropriate history buffer
    if (AS7341_SMUX_low) {
      AS7341_history_low[AS7341_historyIndex_low] = AS7341_Buffer;
      AS7341_historyIndex_low = (AS7341_historyIndex_low + 1) % AS7341_HISTORY_SIZE;
    } else {
      AS7341_history_high[AS7341_historyIndex_high] = AS7341_Buffer;
      AS7341_historyIndex_high = (AS7341_historyIndex_high + 1) % AS7341_HISTORY_SIZE;
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

void printAS7341registers(void) {
    // print the status registers for debugging
  // ENABLE (0x80)
  Serial.print("ENABLE (0x80) reads: ");  
  printByteBinary(as7341.getRegister(AS7341_ENABLE));

  // CONFIG (0x70)
  as7341.setBank(true); // set to BANK1
  Serial.print("CONFIG (0x70) reads: ");
  //Serial.println(as7341.getRegister(AS7341_CONFIG), BIN);
  printByteBinary(as7341.getRegister(AS7341_CONFIG));
  as7341.setBank(false); // set to BANK0

  // ATIME (0x81)
  Serial.print("ATIME (0x81) reads: ");
  Serial.println(as7341.getRegister(AS7341_ATIME));
  // ASTEP (0x84, 0x85)
  Serial.print("ASTEP (0xCA, 0xCb) reads: "); 
  Serial.println((as7341.getRegister(AS7341_ASTEP_H) << 8) | as7341.getRegister(AS7341_ASTEP_L));
  // WTIME (0x83)
  Serial.print("WTIME (0x83) reads: ");
  Serial.println(as7341.getRegister(AS7341_WTIME));
  // INTENAB (0xF9)
  Serial.print("INTENAB (0xF9) reads: ");
  //Serial.println(as7341.getRegister(AS7341_INTENAB), BIN);
  printByteBinary(as7341.getRegister(AS7341_INTENAB));

  //CONTROL (0xFA)
  Serial.print("CONTROL (0xFA) reads: ");
  //Serial.println(as7341.getRegister(AS7341_CONTROL), BIN);
  printByteBinary(as7341.getRegister(AS7341_CONTROL));

  // AGC_GAIN_MAX (0xCF)
  Serial.print("AGC_GAIN_MAX (0xCF) reads: ");
  //Serial.println(as7341.getRegister(AS7341_AGC_GAIN_MAX), BIN);
  printByteBinary(as7341.getRegister(AS7341_AGC_GAIN_MAX));

  // CFG0 (0xA9)
  Serial.print("CFG0 (0xA9) reads: ");
  //Serial.println(as7341.getRegister(AS7341_CFG0), BIN);
  printByteBinary(as7341.getRegister(AS7341_CFG0));

  // AGAIN CFG1 (0xAA  bit 4:0)
  Serial.print("AGAIN (0xAA) reads: ");
  uint8_t CFG1 = as7341.getRegister(AS7341_CFG1);
  //Serial.print(CFG1, BIN);
  printByteBinary(CFG1);
  Serial.print("  Gain: ");
  Serial.println(CFG1 & 0x1F);

  // CFG3 (0xAC)
  Serial.print("CGF3 (0xAA) reads: ");
  printByteBinary(as7341.getRegister(AS7341_CFG3));

  // CFG8 (0xB1)
  Serial.print("CFG8 (0xB1) reads: ");
  //Serial.println(as7341.getRegister(AS7341_CFG8), BIN);
  printByteBinary(as7341.getRegister(AS7341_CFG8));

  // GPIO2 (0xBE)
  Serial.print("GPIO2 (0xBE) reads: ");
  //Serial.println(as7341.getRegister(AS7341_GPIO2), BIN);
  printByteBinary(as7341.getRegister(AS7341_GPIO2));

  // STAT (0x71)
  Serial.print("STAT (0x71) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STAT), BIN);
  printByteBinary(as7341.getRegister(AS7341_STAT));

  // STATUS (0x93)
  Serial.print("STATUS (0x93) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STATUS), BIN);
  printByteBinary(as7341.getRegister(AS7341_STATUS));

  // STATUS2 (0xA3)
  Serial.print("STATUS2 (0xA3) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STATUS2), BIN);
  printByteBinary(as7341.getRegister(AS7341_STATUS2));

  // STATUS3 (0xA4)
  Serial.print("STATUS3 (0xA4) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STATUS3), BIN);
  printByteBinary(as7341.getRegister(AS7341_STATUS3));

  // STATUS5 (0xA6)
  Serial.print("STATUS5 (0xA6) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STATUS5), BIN);
  printByteBinary(as7341.getRegister(AS7341_STATUS5));

  // STATUS6 (0xA7)
  Serial.print("STATUS6 (0xA7) reads: ");
  //Serial.println(as7341.getRegister(AS7341_STATUS6), BIN);
  printByteBinary(as7341.getRegister(AS7341_STATUS6));

  // FIFO_MAP (0xFC)
  Serial.print("FIFO_MAP (0xFC) reads: ");
  //Serial.println(as7341.getRegister(AS7341_FIFO_MAP), BIN);
  printByteBinary(as7341.getRegister(AS7341_FIFO_MAP));



  return;
}