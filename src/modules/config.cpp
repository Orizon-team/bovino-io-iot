#include "config.h"
// ==================== CONFIGURACIÓN BLE ====================
const char* BLE_DEVICE_NAME = "ESP32-BovinoIOT";

// ==================== CONFIGURACIÓN WiFi ====================
const char* WIFI_SSID = "UZIEL 1257";
const char* WIFI_PASSWORD = "123456789";
const char* CONFIG_PORTAL_PASSWORD = "bovinoiot";

// ==================== CONFIGURACIÓN API ====================
const char* API_URL = "https://api-schoolguardian.onrender.com/api/livestock/zone-data";
const char* API_KEY = "tu-api-key";

// ==================== SERVIDORES NTP ====================
const char* NTP_SERVER1 = "pool.ntp.org";
const char* NTP_SERVER2 = "time.nist.gov";

// ==================== IDENTIFICACIÓN DE ESTE DISPOSITIVO IOT (ZONA) ====================
const char* DEVICE_ID = "IOT_ZONA_001";
const char* DEVICE_LOCATION = "Comedero Norte";
ZoneType CURRENT_ZONE_TYPE = ZONE_FEEDER;

// ==================== MODO DISPOSITIVO ====================
DeviceMode CURRENT_DEVICE_MODE = DEVICE_MASTER;

// ==================== ESP-NOW ====================
uint8_t MASTER_MAC_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};