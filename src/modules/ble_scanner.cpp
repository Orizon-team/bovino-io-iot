#include "ble_scanner.h"
#include "mqtt_client.h"
#include <ArduinoJson.h>
#include <cmath>
#include <vector>

BLEScanner bleScanner;

// ==================== Constructor ====================
BLEScanner::BLEScanner() {
    Serial.println("[BLE] Scanner inicializado");
}

// ==================== Inicialización ====================
bool BLEScanner::initialize() {
    Serial.println("[BLE] Sistema de Monitoreo de Ganado - BovinoIOT");
    Serial.printf("[BLE] Zona: %s\n", DEVICE_LOCATION);
    Serial.printf("[BLE] ID Dispositivo: %s\n", DEVICE_ID);
    
    try {
        Serial.println("[BLE] Limpiando estado anterior de BLE...");
        BLEDevice::deinit(false);
        delay(100);
        
        Serial.println("[BLE] Inicializando Bluetooth...");
        BLEDevice::init(BLE_DEVICE_NAME);
        Serial.println("[BLE] Bluetooth inicializado");
        
        BLEScan* pBLEScan = BLEDevice::getScan();
        
        if (pBLEScan == nullptr) {
            Serial.println("[BLE] Error: No se pudo obtener objeto de escaneo");
            return false;
        }
        
        // Limpiar resultados anteriores
        pBLEScan->clearResults();
        
        // Configurar callback
        pBLEScan->setAdvertisedDeviceCallbacks(new AnimalBeaconCallbacks(this));
        
        // Configurar escaneo activo
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);
        
        Serial.println("[BLE] Sistema BLE listo");
        Serial.printf("[BLE] Duración de escaneo: %d segundos\n", SCAN_DURATION);
        Serial.printf("[BLE] RSSI mínimo: %d dBm (aprox. 1 metro)\n", MIN_RSSI_THRESHOLD);
        
        return true;
        
    } catch (const std::exception& e) {
        Serial.printf("[BLE]  Error en inicialización: %s\n", e.what());
        return false;
    }
}

// ==================== Escaneo Simple ====================
void BLEScanner::performScan() {
    Serial.println("\n[BLE] ━━━━━ Iniciando escaneo ━━━━━");
    
    BLEScan* pBLEScan = BLEDevice::getScan();
    if (pBLEScan == nullptr) {
        Serial.println("[BLE]  Error: Scanner no disponible");
        return;
    }
    
    // Limpiar resultados anteriores
    pBLEScan->clearResults();
    
    // Escanear por 2 segundos
    Serial.printf("[BLE] Escaneando por %d segundos...\n", SCAN_DURATION);
    pBLEScan->start(SCAN_DURATION, false);
    // Se realiza una pausa de 2s, en lo que el ecaneo finaliza
    Serial.printf("[BLE] Escaneo completado: %d beacons detectados\n", beacons.size());
}

// ==================== Obtener Beacons Actuales ====================
std::map<String, BeaconData> BLEScanner::getBeaconData() {
    return beacons;
}

// ==================== Limpiar Beacons ====================
void BLEScanner::clearBeacons() {
    beacons.clear();
    Serial.println("[BLE] Beacons limpiados");
}

// ==================== Calcular Distancia ====================
float BLEScanner::calculateDistance(int8_t rssi) {
    const int8_t txPower = -59;  // RSSI típico a 1 metro
    
    if (rssi == 0) {
        return -1.0;
    }
    
    float ratio = rssi * 1.0 / txPower;
    if (ratio < 1.0) {
        return pow(ratio, 10);
    } else {
        float distance = (0.89976) * pow(ratio, 7.7095) + 0.111;
        return distance;
    }
}

// ==================== Validar UUID ====================
bool matchesBeaconUUID(const uint8_t* uuid, size_t length) {
    if (length != 16) {
        return false;
    }
    
    // UUIDs esperados (sin guiones)
    const uint8_t expectedUuid1[] = {
        0xFD, 0xA5, 0x06, 0x93, 0xA4, 0xE2, 0x4F, 0xB1,
        0xAF, 0xCF, 0xC6, 0xEB, 0x07, 0x64, 0x78, 0x25
    };
    
    const uint8_t expectedUuid2[] = {
        0xD5, 0x46, 0xDF, 0x97, 0x47, 0x57, 0x47, 0xEF,
        0xBE, 0x09, 0x3E, 0x2D, 0xCB, 0xDD, 0x0C, 0x77
    };
    
    return (memcmp(uuid, expectedUuid1, 16) == 0) || (memcmp(uuid, expectedUuid2, 16) == 0);
}

// ==================== Procesar Dispositivo Detectado ====================
void BLEScanner::processDevice(BLEAdvertisedDevice advertisedDevice) {
    String mac = advertisedDevice.getAddress().toString().c_str();
    int8_t rssi = advertisedDevice.getRSSI();
    
    // ==================== FILTRO 1: RSSI MÍNIMO ====================
    if (rssi < MIN_RSSI_THRESHOLD) {
        return;  // Fuera de rango (> 1 metro aproximadamente)
    }
    
    // ==================== FILTRO 2: UUID DE iBeacon ====================
    if (!advertisedDevice.haveManufacturerData()) {
        return;
    }
    
    std::string mData = advertisedDevice.getManufacturerData();
    if (mData.length() < 25) {
        return;
    }
    
    uint8_t* data = (uint8_t*)mData.data();
    uint16_t companyId = data[0] | (data[1] << 8);
    
    // Verificar formato iBeacon (0x004C + tipo 0x02)
    if (companyId != 0x004C || data[2] != 0x02 || data[3] != 0x15) {
        return;
    }
    
    // UUID está en bytes 4-19
    const uint8_t* uuid = &data[4];
    
    if (!matchesBeaconUUID(uuid, 16)) {
        return;  // UUID no coincide
    }
    
    // ==================== EXTRAER DATOS DEL iBeacon ====================
    uint16_t major = (data[20] << 8) | data[21];
    uint16_t minor = (data[22] << 8) | data[23];
    uint32_t animalId = (major << 16) | minor;
    
    // Calcular distancia
    float distance = calculateDistance(rssi);
    
    // Obtener ubicación actual
    String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
    if (LOADED_SUB_LOCATION.length() > 0) {
        int slashPos = LOADED_SUB_LOCATION.indexOf('/');
        if (slashPos > 0) {
            currentLocation = LOADED_SUB_LOCATION.substring(slashPos + 1);
        }
    }
    
    // Crear BeaconData simplificado
    BeaconData beacon;
    beacon.animalId = animalId;
    beacon.macAddress = mac;
    beacon.rssi = rssi;
    beacon.distance = distance;
    beacon.detectedLocation = currentLocation;
    
    // Guardar con clave única
    String beaconKey = mac + "_" + String(animalId);
    beacons[beaconKey] = beacon;
    
    Serial.printf("[BLE] Beacon: ID=%u, MAC=%s, RSSI=%d dBm, Dist=%.2fm, Ubicacion=%s\n",
                 animalId, mac.c_str(), rssi, distance, currentLocation.c_str());
}

// ==================== Extraer Animal ID ====================
uint32_t BLEScanner::extractAnimalId(std::string manufacturerData) {
    if (manufacturerData.length() < 25) {
        return 0;
    }
    
    uint8_t* data = (uint8_t*)manufacturerData.data();
    uint16_t companyId = data[0] | (data[1] << 8);
    
    // iBeacon (0x004C)
    if (companyId == 0x004C && data[2] == 0x02 && data[3] == 0x15) {
        uint16_t major = (data[20] << 8) | data[21];
        uint16_t minor = (data[22] << 8) | data[23];
        return (major << 16) | minor;
    }
    
    return 0;
}

// ==================== Callback de BLE ====================
AnimalBeaconCallbacks::AnimalBeaconCallbacks(BLEScanner* scanner) : bleScanner(scanner) {}

void AnimalBeaconCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    bleScanner->processDevice(advertisedDevice);
}

// ==================== Modo Registro de Beacons ====================
void BLEScanner::startBeaconRegistrationMode() {
    Serial.println("[BLE] ==========================================");
    Serial.println("[BLE] MODO: REGISTRO DE BEACONS");
    Serial.println("[BLE] ==========================================");
    Serial.println("[BLE] Este modo permite enlazar beacons con animales");
    Serial.println("[BLE] Activo mientras el modo este en REGISTRO");
    Serial.println("[BLE] Presiona boton de nuevo para salir");
    Serial.println("[BLE] ==========================================");
    
    extern bool beaconRegistrationModeActive;
    extern bool isRegistrationModeActive();
    
    unsigned long lastScan = 0;
    unsigned long scanInterval = 2000;  // Escanear cada 2 segundos
    
    registeredBeaconsCache.clear();  // Limpiar cache
    
    while (isRegistrationModeActive() && beaconRegistrationModeActive) {
        // El loop continúa mientras el switch esté en posición IZQUIERDA
        
        if (millis() - lastScan >= scanInterval) {
            Serial.println("[BLE] Escaneando beacons para registro...");
            
            std::vector<String> detectedMacs;  // Acumular MACs detectadas en este escaneo
            
            BLEScan* pBLEScan = BLEDevice::getScan();
            if (pBLEScan) {
                pBLEScan->clearResults();
                BLEScanResults foundDevices = pBLEScan->start(SCAN_DURATION, false);
                
                int devicesFound = foundDevices.getCount();
                Serial.printf("[BLE] Dispositivos encontrados: %d\n", devicesFound);
                
                // Recolectar todas las MACs detectadas (sin caché, enviar siempre)
                for (int i = 0; i < devicesFound; i++) {
                    BLEAdvertisedDevice device = foundDevices.getDevice(i);
                    String mac = String(device.getAddress().toString().c_str());
                    mac.toUpperCase();
                    
                    // Aplicar filtros locales básicos
                    if (!shouldProcessBeacon(device)) {
                        continue;
                    }
                    
                    // Agregar todas las MACs sin verificar caché
                    detectedMacs.push_back(mac);
                }
                
                pBLEScan->clearResults();
                
                // Enviar todas las MACs detectadas al MQTT cada ciclo
                if (!detectedMacs.empty()) {
                    Serial.printf("[BLE] Enviando %d MACs al MQTT...\n", detectedMacs.size());
                    publishBeaconsToMQTT(detectedMacs);
                } else {
                    Serial.println("[BLE] No se detectaron beacons en este ciclo");
                }
            }
            
            lastScan = millis();
        }
        
        delay(100);
    }
    
    Serial.println("[BLE] ==========================================");
    Serial.println("[BLE] Modo registro finalizado");
    Serial.printf("[BLE] Beacons procesados: %d\n", registeredBeaconsCache.size());
    Serial.println("[BLE] Iniciando modo normal de sondeo...");
    Serial.println("[BLE] ==========================================");
}

bool BLEScanner::shouldProcessBeacon(BLEAdvertisedDevice& device) {
    // Verificar RSSI
    int8_t rssi = device.getRSSI();
    if (rssi < MIN_RSSI_THRESHOLD) {
        return false;  // Señal muy débil
    }
    
    // Verificar que tenga manufacturer data (iBeacon format)
    if (!device.haveManufacturerData()) {
        return false;
    }
    
    std::string mData = device.getManufacturerData();
    if (mData.length() < 25) {
        return false;
    }
    
    uint8_t* data = (uint8_t*)mData.data();
    uint16_t companyId = data[0] | (data[1] << 8);
    
    // Verificar formato iBeacon (0x004C + tipo 0x02)
    if (companyId != 0x004C || data[2] != 0x02 || data[3] != 0x15) {
        return false;
    }
    
    // UUID está en bytes 4-19
    const uint8_t* uuid = &data[4];
    
    // Usar la misma función de validación que processDevice
    return matchesBeaconUUID(uuid, 16);
}

void BLEScanner::publishBeaconsToMQTT(const std::vector<String>& macAddresses) {
    // Importar mqttClient desde main o donde esté definido
    extern MQTTClient mqttClient;
    
    if (!mqttClient.isConnected()) {
        Serial.println("[BLE] MQTT no conectado - no se puede publicar beacons");
        return;
    }
    
    // Crear payload JSON con zone_id y array de MACs
    DynamicJsonDocument doc(1024);
    doc["zone_id"] = LOADED_ZONE_ID;
    
    JsonArray macsArray = doc.createNestedArray("macs");
    for (const String& mac : macAddresses) {
        macsArray.add(mac);
    }
    
    String payload;
    serializeJson(doc, payload);
    
    const char* topic = "bovino_io/register_beacon";
    
    Serial.printf("[BLE] Publicando a MQTT topic: %s\n", topic);
    Serial.printf("[BLE] Payload: %s\n", payload.c_str());
    
    bool published = mqttClient.publish(topic, payload.c_str());
    
    if (published) {
        Serial.printf("[BLE] %d beacons publicados exitosamente a MQTT\n", macAddresses.size());
    } else {
        Serial.println("[BLE] Error al publicar beacons a MQTT");
    }
}