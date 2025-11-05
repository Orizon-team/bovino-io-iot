#include "config.h"
// ==================== CONFIGURACIÓN BLE ====================
const char* BLE_DEVICE_NAME = "ESP32-BovinoIOT";

// ==================== CONFIGURACIÓN WiFi ====================
const char* WIFI_SSID = "UZIEL 1257";
const char* WIFI_PASSWORD = "123456789";

// ==================== CONFIGURACIÓN API ====================
const char* API_URL = "https://api-schoolguardian.onrender.com/api/livestock/zone-data";
const char* API_KEY = "tu-api-key";

// ==================== SERVIDORES NTP ====================
const char* NTP_SERVER1 = "pool.ntp.org";
const char* NTP_SERVER2 = "time.nist.gov";

// ==================== IDENTIFICACIÓN DE ESTE DISPOSITIVO IOT (ZONA) ====================
const char* DEVICE_ID = "1"; 
// const char* DEVICE_ID = "2"; 
 
const char* DEVICE_LOCATION = "Zebedero Norte";
// const char* DEVICE_LOCATION = "Bebedero Norte";
ZoneType CURRENT_ZONE_TYPE = ZONE_PASTURE;
// ZoneType CURRENT_ZONE_TYPE = ZONE_WATERER;

// ==================== MODO DISPOSITIVO ====================
DeviceMode CURRENT_DEVICE_MODE = DEVICE_MASTER;
// DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;

// ==================== ESP-NOW ====================
uint8_t MASTER_MAC_ADDRESS[6] = {0xF0, 0x24, 0xF9, 0x46, 0x57, 0x14};

std::map<uint32_t, BeaconData> slaveBeaconData;