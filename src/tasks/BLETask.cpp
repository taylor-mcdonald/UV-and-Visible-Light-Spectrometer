// BLETask.cpp
#include "BLETask.h"
#include "shared/SharedData.h"  
#include "shared/BLEShared.h"  

#define SOLAR_SERVICE_UUID  "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BME680_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define AS7341_LOW_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a9"  
#define AS7341_HIGH_CHAR_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ac"  
#define UV_CHAR_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define BATTERY_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define FLICKER_CHAR_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26ad"

NimBLECharacteristic* pBME680Characteristic  = nullptr;
NimBLECharacteristic* pAS7341LowCharacteristic  = nullptr;
NimBLECharacteristic* pAS7341HighCharacteristic = nullptr;
NimBLECharacteristic* pUVCharacteristic      = nullptr;
NimBLECharacteristic* pBatteryCharacteristic = nullptr;
NimBLECharacteristic* pFlickerCharacteristic = nullptr;

// Connection state callbacks
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("BLE: client connected");
    }
    void onDisconnect(NimBLEServer* pServer) {
        // Store last connected address before it's gone
        if (pServer->getPeerInfo(0).getAddress().toString().length() > 0) {
            strncpy(bleLastAddress, 
                pServer->getPeerInfo(0).getAddress().toString().c_str(),
                sizeof(bleLastAddress));
        }
        Serial.println("BLE: client disconnected, restarting advertising");
        NimBLEDevice::startAdvertising();
    }
};

void initBLE() {
    NimBLEDevice::init("SolarSensor");  // device name shown during scan
    NimBLEDevice::setMTU(512);          // request maximum MTU

    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(SOLAR_SERVICE_UUID);

    // Create characteristics with notify property
    pBME680Characteristic = pService->createCharacteristic(
        BME680_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );


    pAS7341LowCharacteristic = pService->createCharacteristic(
        AS7341_LOW_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pAS7341HighCharacteristic = pService->createCharacteristic(
        AS7341_HIGH_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pUVCharacteristic = pService->createCharacteristic(
        UV_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pBatteryCharacteristic = pService->createCharacteristic(
        BATTERY_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pFlickerCharacteristic = pService->createCharacteristic(
    FLICKER_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SOLAR_SERVICE_UUID);
    pAdvertising->start();

    Serial.println("BLE: advertising started");
}

void bleTask(void* pvParameters) {
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(1000); // push updates every second

    for (;;) {
        // Only send notifications if a client is connected
        if (NimBLEDevice::getServer()->getConnectedCount() > 0) {
            updateBME680Characteristic();
            //updateAS7341LowCharacteristic();
            //updateAS7341HighCharacteristic();
            //updateUVCharacteristic();   // UV is updated immediately from the UV task, no need to repeat here
            updateBatteryCharacteristic(); 
            //updateFlickerCharacteristic();
        }
        vTaskDelayUntil(&lastWake, interval);
    }
}

void startBLETask() {
    xTaskCreatePinnedToCore(
        bleTask,
        "BLE Task",
        4096,
        NULL,
        1,
        NULL,
        tskNO_AFFINITY
    );
}

void updateBME680Characteristic() {
    BME680Reading latest = BME680history[(BME680historyIndex - 1 + BME680HISTORY_SIZE) % BME680HISTORY_SIZE];
    
    char json[128];
    snprintf(json, sizeof(json),
        "{\"t\":%.2f,\"h\":%.2f,\"p\":%.2f,\"g\":%.2f}",
        latest.temp,
        latest.humid,
        latest.press,
        latest.gas_resistance / 1000.0f  // send as kOhms
    );

    pBME680Characteristic->setValue((uint8_t*)json, strlen(json));
    pBME680Characteristic->notify();

    bleNotifCount++;
    strncpy(bleLastUpdated, "BME680", sizeof(bleLastUpdated));
}

void updateUVCharacteristic() {
    UVReading latest = UVhistory[(UVhistoryIndex - 1 + UVHISTORY_SIZE) % UVHISTORY_SIZE];

    char json[128];
    snprintf(json, sizeof(json),
        "{\"uva\":%.2f,\"uvb\":%.2f,\"uvc\":%.2f,\"uvi\":%.2f}",
        latest.uva,
        latest.uvb,
        latest.uvc,
        latest.uvIndex
    );

    pUVCharacteristic->setValue((uint8_t*)json, strlen(json));
    pUVCharacteristic->notify();

    bleNotifCount++;
    strncpy(bleLastUpdated, "UV", sizeof(bleLastUpdated));
}

void updateAS7341LowCharacteristic() {
    AS7341Reading low  = AS7341_history_low[(AS7341_historyIndex_low   - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];
    

    char json[256];
    snprintf(json, sizeof(json),
        "{\"f1\":%u,\"f2\":%u,\"f3\":%u,\"f4\":%u,\"nir\":%u,\"clr\":%u,\"g\":%u}",
        low.F1_F5,  low.F2_F6,  low.F3_F7,  low.F4_F8,
        low.NIR,    low.Clr,   low.gain
    );

        pAS7341LowCharacteristic->setValue((uint8_t*)json, strlen(json));
    pAS7341LowCharacteristic->notify();

    bleNotifCount++;
    strncpy(bleLastUpdated, "AS7341 Low", sizeof(bleLastUpdated));
}

void updateAS7341HighCharacteristic() {
    AS7341Reading high = AS7341_history_high[(AS7341_historyIndex_high - 1 + AS7341_HISTORY_SIZE) % AS7341_HISTORY_SIZE];

    char json[256];
    snprintf(json, sizeof(json),
        "{\"f5\":%u,\"f6\":%u,\"f7\":%u,\"f8\":%u,\"nir\":%u,\"clr\":%u,\"g\":%u}",
        high.F1_F5, high.F2_F6, high.F3_F7, high.F4_F8,
        high.NIR,    high.Clr,   high.gain
    );

    pAS7341HighCharacteristic->setValue((uint8_t*)json, strlen(json));
    pAS7341HighCharacteristic->notify();

    bleNotifCount++;
    strncpy(bleLastUpdated, "AS7341 High", sizeof(bleLastUpdated));
}

void updateBatteryCharacteristic() {
    char json[80];
    snprintf(json, sizeof(json),
        "{\"v\":%.3f,\"pct\":%.1f,\"cr\":%.2f}",
        batteryReading.voltage,
        batteryReading.percent,
        batteryReading.changeRate   // %/hr, +ve=charging, -ve=discharging
    );
    pBatteryCharacteristic->setValue((uint8_t*)json, strlen(json));
    pBatteryCharacteristic->notify();
    bleNotifCount++;
    strncpy(bleLastUpdated, "Battery", sizeof(bleLastUpdated));
}

void updateFlickerCharacteristic() {
    if (!flickerResult.valid) return;

    // Build a compact JSON array of peaks with magnitudes
    // e.g. {"ts":12345,"n":2,"hz":[60.0,120.0],"mag":[1.0,0.45]}
    char json[256];
    int pos = snprintf(json, sizeof(json),
        "{\"ts\":%lu,\"n\":%u,\"hz\":[",
        flickerResult.timestamp,
        flickerResult.peakCount
    );
    for (uint8_t i = 0; i < flickerResult.peakCount && pos < (int)sizeof(json) - 20; i++) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            i ? ",%.1f" : "%.1f", flickerResult.peaks[i]);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "],\"mag\":[");
    for (uint8_t i = 0; i < flickerResult.peakCount && pos < (int)sizeof(json) - 10; i++) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            i ? ",%.2f" : "%.2f", flickerResult.magnitudes[i]);
    }
    snprintf(json + pos, sizeof(json) - pos, "]}");

    pFlickerCharacteristic->setValue((uint8_t*)json, strlen(json));
    pFlickerCharacteristic->notify();

    bleNotifCount++;
    strncpy(bleLastUpdated, "Flicker", sizeof(bleLastUpdated));
}