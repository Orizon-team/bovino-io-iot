#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <map>
#include <vector>

// ==================== DISPOSITIVO IOT (ZONA) ====================
extern const char* DEVICE_ID;
extern const char* DEVICE_LOCATION;

enum ZoneType {
    ZONE_FEEDER,
    ZONE_WATERER,
    ZONE_PASTURE,
    ZONE_REST,
    ZONE_GENERIC
};

extern ZoneType CURRENT_ZONE_TYPE;

// ==================== MODO DISPOSITIVO ====================
enum DeviceMode {
    DEVICE_MASTER,
    DEVICE_SLAVE
};

extern DeviceMode CURRENT_DEVICE_MODE;

// ==================== ESP-NOW ====================
constexpr int ESPNOW_CHANNEL = 1;
constexpr int MAX_SLAVES = 10;
extern uint8_t MASTER_MAC_ADDRESS[6];

// ==================== WiFi ====================
extern const char* WIFI_SSID;              
extern const char* WIFI_PASSWORD;          
constexpr unsigned long WIFI_TIMEOUT = 20000;
constexpr unsigned long WIFI_RETRY_INTERVAL = 300000;
extern bool ENABLE_WIFI_SYNC; // Permite desactivar WiFi para simulación
extern bool ENABLE_WIFI_PORTAL; // Activa el portal de configuracion por web
extern const char* CONFIG_PORTAL_PASSWORD;

// ==================== ZONA GEOGRAFICA ====================
extern const char* ZONE_NAME; // Nombre de la zona macro que agrupa a maestro/esclavos

// ==================== API ====================
extern const char* API_URL;
extern const char* API_KEY;                
constexpr int HTTP_TIMEOUT = 15000;        
constexpr int MAX_RETRY_ATTEMPTS = 3;

// ==================== BLE - ESCANEO ADAPTATIVO ====================
constexpr int SCAN_DURATION_ACTIVE = 3;
constexpr int SCAN_DURATION_NORMAL = 5;
constexpr int SCAN_DURATION_ECO = 8;

constexpr unsigned long SCAN_INTERVAL_ACTIVE = 10000;
constexpr unsigned long SCAN_INTERVAL_NORMAL = 30000;
constexpr unsigned long SCAN_INTERVAL_ECO = 60000;

constexpr int ANIMALS_CHANGE_THRESHOLD = 3;
constexpr int STABLE_SCANS_FOR_ECO = 10;

#define TARGET_COMPANY_ID 0x004C
extern const char* BLE_DEVICE_NAME;

// ==================== TIMINGS MAESTRO-ESCLAVO ====================
constexpr unsigned long SLAVE_SEND_INTERVAL = 10000;
constexpr unsigned long MASTER_SYNC_INTERVAL = 60000;

// ==================== DISTANCIA POR RSSI ====================
constexpr int RSSI_REFERENCE = -59;
constexpr float PATH_LOSS_EXPONENT = 2.0;

constexpr float DISTANCE_NEAR = 2.0;
constexpr float DISTANCE_MEDIUM = 5.0;
constexpr float DISTANCE_FAR = 10.0;
extern float PRESENCE_DISTANCE_THRESHOLD;

// ==================== COMPORTAMIENTO ====================
constexpr unsigned long MIN_TIME_EATING = 15;
constexpr unsigned long MIN_TIME_DRINKING = 5;
constexpr unsigned long MIN_TIME_RESTING = 30;

constexpr unsigned long ANIMAL_MISSING_TIMEOUT = 86400000;
constexpr unsigned long ABNORMAL_BEHAVIOR_TIME = 7200000;

// ==================== HARDWARE ====================
constexpr int LED_LOADER = 13;
constexpr int LED_DANGER = 14;
constexpr int LED_SUCCESS = 26;
constexpr int LED_ERROR = 25;
constexpr int ZUMBADOR = 15;

constexpr int LCD_SDA = 21;
constexpr int LCD_SCL = 22;
constexpr uint8_t LCD_I2C_ADDR = 0x27;
constexpr int LCD_COLS = 16;
constexpr int LCD_ROWS = 2;

// ==================== BUFFER OFFLINE ====================
constexpr int MAX_OFFLINE_RECORDS = 100;
constexpr unsigned long SYNC_INTERVAL = 300000;

// ==================== NTP ====================
extern const char* NTP_SERVER1;
extern const char* NTP_SERVER2;
constexpr long GMT_OFFSET_SEC = -21600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr int NTP_TIMEOUT_SECONDS = 30;

// ==================== ESTRUCTURAS ====================
struct BeaconData {
    uint32_t animalId;
    String macAddress;
    int8_t rssi;
    float distance;
    unsigned long firstSeen;
    unsigned long lastSeen;
    bool isPresent;
    String detectedLocation;
};

extern std::map<uint32_t, BeaconData> slaveBeaconData;

struct AnimalBehavior {
    uint32_t animalId;
    unsigned long timeInZone;
    unsigned long entryTime;
    unsigned long exitTime;
    int visitCount;
    bool isPresent;
    bool missingAlert;
    float avgDistance;
    int8_t avgRssi;
};

struct ESPNowMessage {
    char deviceId[32];
    char location[64];
    uint8_t zoneType;
    uint32_t animalId;
    int8_t rssi;
    float distance;
    bool isPresent;
    unsigned long timestamp;
};

#endif // CONFIG_H
