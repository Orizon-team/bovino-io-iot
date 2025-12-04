#include "config/device_config.h"
#include <Arduino.h>

String LOADED_SUB_LOCATION = "";
String LOADED_ZONE_NAME = "";
String LOADED_DEVICE_ID = "";
int LOADED_ZONE_ID = 0;

DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;
uint8_t MASTER_MAC_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

const char* getDeviceId() {
    static char deviceIdBuf[64];
    if (LOADED_DEVICE_ID.length() > 0) {
        strncpy(deviceIdBuf, LOADED_DEVICE_ID.c_str(), sizeof(deviceIdBuf) - 1);
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(deviceIdBuf, sizeof(deviceIdBuf), "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]);
    }
    deviceIdBuf[sizeof(deviceIdBuf) - 1] = '\0';
    return deviceIdBuf;
}

const char* getDeviceLocation() {
    static char locationBuf[64];
    if (LOADED_ZONE_NAME.length() > 0) {
        strncpy(locationBuf, LOADED_ZONE_NAME.c_str(), sizeof(locationBuf) - 1);
    } else {
        strncpy(locationBuf, "Zona sin configurar", sizeof(locationBuf) - 1);
    }
    locationBuf[sizeof(locationBuf) - 1] = '\0';
    return locationBuf;
}
