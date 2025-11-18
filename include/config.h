#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <map>
#include <vector>

extern const char* DEVICE_ID;          // ID único del dispositivo ESP32
extern const char* DEVICE_LOCATION;    // Nombre de la zona (Comedero, Bebedero, etc.)

extern String LOADED_ZONE_NAME;        // Zona cargada desde NVS
extern String LOADED_SUB_LOCATION;     // Sub-localidad cargada desde NVS
extern String LOADED_DEVICE_ID;        // ID cargado desde NVS

enum ZoneType {
    ZONE_FEEDER,     // Zona de alimentación
    ZONE_WATERER,    // Zona de bebedero
    ZONE_PASTURE,    // Zona de pastoreo
    ZONE_REST,       // Zona de descanso
    ZONE_GENERIC     // Zona genérica
};

extern ZoneType CURRENT_ZONE_TYPE;     // Tipo de zona configurada

enum DeviceMode {
    DEVICE_MASTER,   // Maestro: conecta WiFi y envía datos a backend
    DEVICE_SLAVE     // Esclavo: envía datos al maestro vía ESP-NOW
};

extern DeviceMode CURRENT_DEVICE_MODE; // Modo de operación del dispositivo

constexpr int ESPNOW_CHANNEL = 1;                           // Canal WiFi para ESP-NOW
constexpr int MAX_SLAVES = 10;                              // Máx. esclavos que puede gestionar un maestro
constexpr unsigned long ESPNOW_SEND_INTERVAL = 30000;       // Intervalo de envío esclavo→maestro (ms)
extern uint8_t MASTER_MAC_ADDRESS[6];                       // Dirección MAC del maestro

extern const char* WIFI_SSID;                               // SSID de la red WiFi
extern const char* WIFI_PASSWORD;                           // Contraseña WiFi
constexpr unsigned long WIFI_TIMEOUT = 20000;               // Timeout de conexión WiFi (ms)
constexpr unsigned long WIFI_RETRY_INTERVAL = 300000;       // Intervalo entre reintentos WiFi (ms)
constexpr bool ENABLE_WIFI_SYNC = true;                     // Habilitar sincronización WiFi en maestro
constexpr bool ENABLE_WIFI_PORTAL = true;                   // Habilitar portal de configuración
extern const char* CONFIG_PORTAL_PASSWORD;                  // Contraseña del portal de config

extern const char* API_URL;                                 // URL del backend API
extern const char* API_KEY;                                 // API key para autenticación
constexpr int HTTP_TIMEOUT = 15000;                         // Timeout HTTP (ms)
constexpr int MAX_RETRY_ATTEMPTS = 3;                       // Máx. reintentos en fallos HTTP

extern const char* MQTT_BROKER;                             // IP del broker MQTT
constexpr int MQTT_PORT = 8883;                             // Puerto del broker MQTT (8883 con TLS)
extern const char* MQTT_USER;                               // Usuario MQTT (opcional)
extern const char* MQTT_PASSWORD;                           // Contraseña MQTT (opcional)
extern const char* MQTT_TOPIC;                              // Topic MQTT para publicar
constexpr bool ENABLE_MQTT = true;                          // Habilitar envío por MQTT
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000;     // Intervalo de reconexión MQTT (ms)

constexpr int SCAN_DURATION_ACTIVE = 3;                     // Duración escaneo BLE en modo activo (s)
constexpr int SCAN_DURATION_NORMAL = 5;                     // Duración escaneo BLE en modo normal (s)
constexpr int SCAN_DURATION_ECO = 8;                        // Duración escaneo BLE en modo eco (s)

constexpr unsigned long SCAN_INTERVAL_ACTIVE = 10000;       // Intervalo entre escaneos en modo activo (ms)
constexpr unsigned long SCAN_INTERVAL_NORMAL = 30000;       // Intervalo entre escaneos en modo normal (ms)
constexpr unsigned long SCAN_INTERVAL_ECO = 60000;          // Intervalo entre escaneos en modo eco (ms)

constexpr int ANIMALS_CHANGE_THRESHOLD = 3;                 // Número de cambios para activar modo activo
constexpr int STABLE_SCANS_FOR_ECO = 10;                    // Escaneos sin cambios para activar modo eco

#define BEACON_UUID_1 "FDA50693-A4E2-4FB1-AFCF-C6EB07647825"  // UUID principal de iBeacons
#define BEACON_UUID_2 "D546DF97-4757-47EF-BE09-3E2DCBDD0C77"  // UUID secundario de iBeacons
#define BEACON_UUID_3 "00000000-0000-0000-0000-000000000000"  // UUID terciario (no usado)

#define BEACON_MAC_PREFIX "dc:0d:30:2c:e8"                   // Prefijo MAC de los beacons

constexpr int MIN_RSSI_THRESHOLD = -90;                     // RSSI mínimo para aceptar beacon (dBm)

#define TARGET_COMPANY_ID 0x004C                             // Company ID de iBeacon (Apple)

enum BeaconFilterMode {
    FILTER_BY_UUID,          // Filtrar por UUID de iBeacon (RECOMENDADO)
    FILTER_BY_MAC_PREFIX,    // Filtrar por prefijo de dirección MAC
    FILTER_BY_NAME_PREFIX,   // Filtrar por prefijo del nombre BLE
    FILTER_BY_COMPANY_ID,    // Filtrar por Company ID
    FILTER_DISABLED          // Aceptar todos los beacons (solo debug)
};

constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;

enum AnimalIdSource {
    USE_MAJOR_MINOR,         // Combinar Major + Minor = 32 bits
    USE_MAJOR_ONLY,          // Solo Major (16 bits)
    USE_MINOR_ONLY,          // Solo Minor (16 bits)
    USE_MAC_ADDRESS          // Usar MAC como ID (fallback)
};

constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_MINOR;

extern const char* BLE_DEVICE_NAME;                         // Nombre BLE del dispositivo

constexpr int RSSI_REFERENCE = -59;                         // RSSI de referencia para cálculo de distancia
constexpr float PATH_LOSS_EXPONENT = 2.0;                   // Exponente de pérdida de señal

constexpr float DISTANCE_NEAR = 2.0;                        // Distancia "cerca" (metros)
constexpr float DISTANCE_MEDIUM = 5.0;                      // Distancia "media" (metros)
constexpr float DISTANCE_FAR = 10.0;                        // Distancia "lejos" (metros)

constexpr unsigned long MIN_TIME_EATING = 15;               // Tiempo mínimo comiendo (min)
constexpr unsigned long MIN_TIME_DRINKING = 5;              // Tiempo mínimo bebiendo (min)
constexpr unsigned long MIN_TIME_RESTING = 30;              // Tiempo mínimo descansando (min)

constexpr unsigned long ANIMAL_MISSING_TIMEOUT = 86400000;  // Timeout para marcar animal perdido (24h en ms)
constexpr unsigned long ABNORMAL_BEHAVIOR_TIME = 7200000;   // Tiempo para detectar comportamiento anormal (2h en ms)

constexpr int LED_LOADER = 13;                              // Pin LED de carga
constexpr int LED_DANGER = 14;                              // Pin LED de peligro
constexpr int LED_SUCCESS = 26;                             // Pin LED de éxito
constexpr int LED_ERROR = 25;                               // Pin LED de error
constexpr int ZUMBADOR = 15;                                // Pin del zumbador

constexpr int RESET_BUTTON = 27;                            // Pin botón de reset
constexpr unsigned long RESET_BUTTON_HOLD_TIME = 3000;      // Tiempo presión botón para reset (ms)
constexpr unsigned long DEBOUNCE_DELAY = 50;                // Retardo anti-rebote (ms)

constexpr int LCD_SDA = 21;                                 // Pin SDA del LCD I2C
constexpr int LCD_SCL = 22;                                 // Pin SCL del LCD I2C
constexpr uint8_t LCD_I2C_ADDR = 0x27;                      // Dirección I2C del LCD
constexpr int LCD_COLS = 16;                                // Columnas del LCD
constexpr int LCD_ROWS = 2;                                 // Filas del LCD

constexpr int MAX_OFFLINE_RECORDS = 100;                    // Máx. registros en buffer offline
constexpr unsigned long SYNC_INTERVAL = 300000;             // Intervalo de sincronización con backend (5 min en ms)

extern const char* NTP_SERVER1;                             // Servidor NTP primario
extern const char* NTP_SERVER2;                             // Servidor NTP secundario
constexpr long GMT_OFFSET_SEC = -21600;                     // Offset GMT (segundos)
constexpr int DAYLIGHT_OFFSET_SEC = 0;                      // Offset horario de verano (segundos)
constexpr int NTP_TIMEOUT_SECONDS = 30;                     // Timeout para sincronización NTP (s)

struct BeaconData {
    uint32_t animalId;            // ID del animal (Major+Minor)
    String macAddress;            // Dirección MAC del beacon
    String uuid;                  // UUID del iBeacon
    uint16_t major;               // Major del iBeacon
    uint16_t minor;               // Minor del iBeacon
    int8_t rssi;                  // Intensidad de señal (dBm)
    float distance;               // Distancia calculada (metros)
    unsigned long firstSeen;      // Primera detección (timestamp)
    unsigned long lastSeen;       // Última detección (timestamp)
    bool isPresent;               // Está presente en la zona
    String detectedLocation;      // Ubicación donde fue detectado
};

struct AnimalBehavior {
    uint32_t animalId;            // ID del animal
    unsigned long timeInZone;     // Tiempo total en la zona (ms)
    unsigned long entryTime;      // Timestamp de entrada
    unsigned long exitTime;       // Timestamp de salida
    int visitCount;               // Número de visitas
    bool isPresent;               // Está presente actualmente
    bool missingAlert;            // Alerta de animal perdido
    float avgDistance;            // Distancia promedio
    int8_t avgRssi;               // RSSI promedio
};

struct ESPNowMessage {
    char deviceId[32];            // ID del dispositivo que envía
    char location[64];            // Ubicación del dispositivo
    uint8_t zoneType;             // Tipo de zona
    uint32_t animalId;            // ID del animal detectado
    int8_t rssi;                  // RSSI del beacon
    float distance;               // Distancia calculada
    bool isPresent;               // Está presente
    unsigned long timestamp;      // Timestamp de detección
};

void initResetButton();         // Inicializa botón de reset
bool checkResetButton();        // Verifica si botón está presionado

#endif // CONFIG_H
