#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>
#include <map>
#include <vector>
#include "models/beacon.h"
#include "models/espnow_message.h"
#include "enums/device.h"
#include "enums/beacon.h"

const char* getDeviceId();
const char* getDeviceLocation();

extern String LOADED_SUB_LOCATION;
extern String LOADED_ZONE_NAME;
extern String LOADED_DEVICE_ID;
extern int LOADED_ZONE_ID;
extern DeviceMode CURRENT_DEVICE_MODE;
extern uint8_t MASTER_MAC_ADDRESS[6];
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* CONFIG_PORTAL_PASSWORD;
extern const char* AP_SSID_PREFIX;
extern const char* API_URL;
extern const char* API_KEY;
extern const char* MQTT_BROKER;
extern const char* MQTT_USER;
extern const char* MQTT_PASSWORD;
extern const char* MQTT_TOPIC;
extern const char* BLE_DEVICE_NAME;
extern bool beaconRegistrationModeActive;
extern const char* NTP_SERVER1;
extern const char* NTP_SERVER2;

constexpr int ESPNOW_CHANNEL = 0;
constexpr int MAX_SLAVES = 10;
constexpr unsigned long ESPNOW_SEND_INTERVAL = 3000;
constexpr unsigned long WIFI_TIMEOUT = 20000;
constexpr unsigned long WIFI_RETRY_INTERVAL = 300000;
constexpr bool ENABLE_WIFI_SYNC = true;
constexpr bool ENABLE_WIFI_PORTAL = true;
constexpr int HTTP_TIMEOUT = 15000;
constexpr int MAX_RETRY_ATTEMPTS = 3;
constexpr int MQTT_PORT = 8883;
constexpr bool ENABLE_MQTT = true;
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000;
constexpr int SCAN_DURATION = 5;
constexpr unsigned long SCAN_CYCLE_INTERVAL = 6000;
constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_MINOR;
constexpr int RSSI_REFERENCE = -59;
constexpr float PATH_LOSS_EXPONENT = 2.0;
constexpr int LED_RGB_RED = 25;
constexpr int LED_RGB_GREEN = 26;
constexpr int LED_RGB_BLUE = 14;
constexpr int ZUMBADOR = 15;
constexpr int RESET_BUTTON = 27;
constexpr unsigned long RESET_BUTTON_HOLD_TIME = 3000;
constexpr int MODE_BUTTON = 33;
constexpr unsigned long DEBOUNCE_DELAY = 50;
constexpr int LCD_SDA = 21;
constexpr int LCD_SCL = 22;
constexpr uint8_t LCD_I2C_ADDR = 0x27;
constexpr int LCD_COLS = 16;
constexpr int LCD_ROWS = 2;
constexpr long GMT_OFFSET_SEC = -21600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr int NTP_TIMEOUT_SECONDS = 30;
constexpr int MIN_RSSI_THRESHOLD = -95;
constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;

constexpr const char* BEACON_UUID_1 = "FDA50693-A4E2-4FB1-AFCF-C6EB07647825";
constexpr uint16_t TARGET_COMPANY_ID = 0x004C;

void initResetButton();
bool checkResetButton();
void initModeButton();
void enterRegistrationMode();
void exitRegistrationMode();
void checkModeButtonPress();
bool isRegistrationModeActive();

#endif
