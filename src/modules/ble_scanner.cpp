#include "ble_scanner.h"
#include <cmath>
#include <cstdio>

// Definición de la instancia global
BLEScanner bleScanner;

// ==================== Constructor ====================
BLEScanner::BLEScanner() 
    : currentMode(MODE_NORMAL),
      lastScanTime(0),
      scansWithoutChange(0),
      recentChanges(0),
      totalScanCount(0) {
    // Constructor - El sistema inicia en modo NORMAL
    Serial.println("[BLE] Constructor: Sistema de monitoreo de ganado inicializado");
}

// ==================== Inicialización ====================
bool BLEScanner::initialize() {
    Serial.println("[BLE] ===============================================");
    Serial.println("[BLE] Sistema de Monitoreo de Ganado - BovinoIOT");
    Serial.println("[BLE] ===============================================");
    Serial.printf("[BLE] Zona: %s (Tipo: %d)\n", DEVICE_LOCATION, CURRENT_ZONE_TYPE);
    Serial.printf("[BLE] ID Dispositivo: %s\n", DEVICE_ID);
    
    try {
        // Inicializar BLE usando la biblioteca Arduino (más estable)
        Serial.println("[BLE] Inicializando Bluetooth...");
        BLEDevice::init(BLE_DEVICE_NAME);
    Serial.println("[BLE] [OK] Bluetooth inicializado");
        
        // Obtener objeto de escaneo
        BLEScan* pBLEScan = BLEDevice::getScan();
        
        if (pBLEScan == nullptr) {
            Serial.println("[BLE] [ERROR] No se pudo obtener objeto de escaneo");
            return false;
        }
        
        // Configurar callback para beacons detectados
        pBLEScan->setAdvertisedDeviceCallbacks(new AnimalBeaconCallbacks(this));
        
        // Configurar escaneo activo para obtener más información
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);  // Intervalo entre escaneos
        pBLEScan->setWindow(99);     // Ventana de escaneo
        
    Serial.println("[BLE] [OK] Sistema listo para detectar beacons de ganado");
        Serial.printf("[BLE] Modo inicial: %s\n", 
                     currentMode == MODE_ACTIVE ? "ACTIVO" : 
                     currentMode == MODE_NORMAL ? "NORMAL" : "ECO");
        
        return true;
        
    } catch (...) {
    Serial.println("[BLE] [ERROR] Error fatal durante inicialización");
        return false;
    }
}

// ==================== Escaneo Adaptativo ====================
void BLEScanner::performScan() {
    unsigned long currentTime = millis();
    
    // Verificar si es momento de escanear según el intervalo del modo actual
    if (currentTime - lastScanTime < getScanInterval()) {
        return;  // Aún no es tiempo de escanear
    }
    
    // Guardar conteo anterior para detectar cambios
    int previousAnimalCount = getAnimalCount();
    
    // Ejecutar escaneo
    int duration = getScanDuration();
    Serial.printf("\n[BLE] ━━━━━ Escaneo #%d [Modo: %s] ━━━━━\n", 
                  totalScanCount + 1,
                  currentMode == MODE_ACTIVE ? "ACTIVO" : 
                  currentMode == MODE_NORMAL ? "NORMAL" : "ECO");
    
    BLEDevice::getScan()->start(duration, false);
    
    lastScanTime = currentTime;
    totalScanCount++;
    
    // Verificar animales ausentes
    checkMissingAnimals();
    
    // Detectar cambios en la zona
    int newAnimalCount = getAnimalCount();
    if (newAnimalCount != previousAnimalCount) {
        recentChanges++;
        scansWithoutChange = 0;
    Serial.printf("[BLE] [WARNING] Cambio detectado: %d -> %d animales\n", 
                     previousAnimalCount, newAnimalCount);
    } else {
        scansWithoutChange++;
        if (recentChanges > 0) recentChanges--;  // Decaer cambios recientes
    }
    
    // Ajustar modo de escaneo según actividad
    adjustScanMode();
    
    Serial.printf("[BLE] Animales en zona: %d | Cambios recientes: %d | Sin cambios: %d\n",
                 newAnimalCount, recentChanges, scansWithoutChange);
}

// ==================== Cálculo de Distancia por RSSI ====================
float BLEScanner::calculateDistance(int8_t rssi) {
    /**
     * Fórmula de propagación de señal en espacio libre:
     * distance = 10 ^ ((RSSI_REFERENCE - rssi) / (10 * PATH_LOSS_EXPONENT))
     * 
     * Ejemplo: Si RSSI = -70 dBm
     * distance = 10^((-59 - (-70)) / (10 * 2.0)) = 10^(11/20) = 10^0.55 ≈ 3.55 metros
     */
    if (rssi == 0) {
        return -1.0;  // RSSI inválido
    }
    
    float distance = pow(10.0, (RSSI_REFERENCE - rssi) / (10.0 * PATH_LOSS_EXPONENT));
    return distance;
}

// ==================== Análisis de Comportamiento Animal ====================
void BLEScanner::updateBehavior(uint32_t animalId, const BeaconData& beacon) {
    unsigned long currentTime = millis();
    
    // Buscar o crear registro de comportamiento
    if (behaviors.find(animalId) == behaviors.end()) {
        // Nuevo animal detectado en la zona
        AnimalBehavior newBehavior;
        newBehavior.animalId = animalId;
        newBehavior.timeInZone = 0;
        newBehavior.entryTime = currentTime;
        newBehavior.exitTime = 0;
        newBehavior.visitCount = 1;
        newBehavior.isPresent = true;
        newBehavior.missingAlert = false;
        newBehavior.avgDistance = beacon.distance;
        newBehavior.avgRssi = beacon.rssi;
        
        behaviors[animalId] = newBehavior;
        
        Serial.printf("[BLE] [VACA DETECTADA] Nuevo animal detectado: ID=%u, Distancia=%.2fm\n", 
                     animalId, beacon.distance);
    } else {
        // Animal ya conocido - actualizar comportamiento
        AnimalBehavior& behavior = behaviors[animalId];
        
        // Si estaba ausente y ahora está presente = nueva visita
        if (!behavior.isPresent && beacon.isPresent) {
            behavior.visitCount++;
            behavior.entryTime = currentTime;
            Serial.printf("[BLE] [VACA] Animal regresó: ID=%u (Visita #%d)\n", 
                         animalId, behavior.visitCount);
        }
        
        // Si estaba presente y ahora está ausente = salida
        if (behavior.isPresent && !beacon.isPresent) {
            behavior.exitTime = currentTime;
            behavior.timeInZone += (currentTime - behavior.entryTime);
            Serial.printf("[BLE] [VACA] Animal salió: ID=%u (Tiempo en zona: %lu min)\n", 
                         animalId, behavior.timeInZone / 60000);
        }
        
        // Actualizar estado
        behavior.isPresent = beacon.isPresent;
        behavior.missingAlert = false;  // Animal detectado, quitar alerta
        
        // Actualizar promedios (promedio móvil simple)
        behavior.avgDistance = (behavior.avgDistance * 0.7) + (beacon.distance * 0.3);
        behavior.avgRssi = (behavior.avgRssi * 0.7) + (beacon.rssi * 0.3);
        
        // Actualizar tiempo en zona si está presente
        if (beacon.isPresent) {
            behavior.timeInZone += (currentTime - behavior.entryTime);
            behavior.entryTime = currentTime;  // Resetear para próximo cálculo
        }
    }
}

void BLEScanner::checkMissingAnimals() {
    unsigned long currentTime = millis();
    
    for (auto& pair : behaviors) {
        AnimalBehavior& behavior = pair.second;
        uint32_t animalId = pair.first;
        
        // Buscar beacon correspondiente
        if (beacons.find(animalId) != beacons.end()) {
            BeaconData& beacon = beacons[animalId];
            unsigned long timeSinceLastSeen = currentTime - beacon.lastSeen;
            
            // Si ha pasado mucho tiempo sin ver al animal
            if (timeSinceLastSeen > ANIMAL_MISSING_TIMEOUT && !behavior.missingAlert) {
                behavior.missingAlert = true;
                behavior.isPresent = false;
                Serial.printf("[BLE] [WARNING] ALERTA: Animal ID=%u no detectado hace %lu horas\n",
                             animalId, timeSinceLastSeen / 3600000);
            }
        }
    }
}

void BLEScanner::adjustScanMode() {
    ScanMode previousMode = currentMode;
    
    // Cambiar a modo ACTIVO si hay muchos cambios
    if (recentChanges >= ANIMALS_CHANGE_THRESHOLD) {
        currentMode = MODE_ACTIVE;
    }
    // Cambiar a modo ECO si no hay cambios hace tiempo
    else if (scansWithoutChange >= STABLE_SCANS_FOR_ECO) {
        currentMode = MODE_ECO;
    }
    // Modo NORMAL para actividad moderada
    else {
        currentMode = MODE_NORMAL;
    }
    
    // Notificar si cambió el modo
    if (previousMode != currentMode) {
    Serial.printf("[BLE] [MODE] Cambio de modo: %s -> %s\n",
                     previousMode == MODE_ACTIVE ? "ACTIVO" : 
                     previousMode == MODE_NORMAL ? "NORMAL" : "ECO",
                     currentMode == MODE_ACTIVE ? "ACTIVO" : 
                     currentMode == MODE_NORMAL ? "NORMAL" : "ECO");
    }
}

// ==================== Métodos Auxiliares ====================
std::vector<uint32_t> BLEScanner::getCurrentAnimals() {
    std::vector<uint32_t> currentAnimals;
    
    for (const auto& pair : behaviors) {
        if (pair.second.isPresent) {
            currentAnimals.push_back(pair.first);
        }
    }
    
    return currentAnimals;
}

std::vector<uint32_t> BLEScanner::getMissingAnimals() {
    std::vector<uint32_t> missingAnimals;
    
    for (const auto& pair : behaviors) {
        if (pair.second.missingAlert) {
            missingAnimals.push_back(pair.first);
        }
    }
    
    return missingAnimals;
}

std::map<uint32_t, BeaconData> BLEScanner::getBeaconData() {
    return beacons;
}

std::map<uint32_t, AnimalBehavior> BLEScanner::getBehaviorData() {
    return behaviors;
}

ScanMode BLEScanner::getCurrentMode() {
    return currentMode;
}

int BLEScanner::getAnimalCount() {
    int count = 0;
    for (const auto& pair : behaviors) {
        if (pair.second.isPresent) {
            count++;
        }
    }
    return count;
}

void BLEScanner::getStats(int& totalScans, int& changesDetected) {
    totalScans = totalScanCount;
    changesDetected = recentChanges;
}

int BLEScanner::getScanDuration() {
    switch (currentMode) {
        case MODE_ACTIVE: return SCAN_DURATION_ACTIVE;
        case MODE_NORMAL: return SCAN_DURATION_NORMAL;
        case MODE_ECO:    return SCAN_DURATION_ECO;
        default:          return SCAN_DURATION_NORMAL;
    }
}

unsigned long BLEScanner::getScanInterval() {
    switch (currentMode) {
        case MODE_ACTIVE: return SCAN_INTERVAL_ACTIVE;
        case MODE_NORMAL: return SCAN_INTERVAL_NORMAL;
        case MODE_ECO:    return SCAN_INTERVAL_ECO;
        default:          return SCAN_INTERVAL_NORMAL;
    }
}

// ==================== Procesamiento de Beacons ====================
void BLEScanner::processDevice(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveManufacturerData()) {
        return;
    }

    std::string mData = advertisedDevice.getManufacturerData();
    if (mData.length() < 2) {
        return;
    }

    uint8_t* data = (uint8_t*)mData.data();
    uint16_t companyId = data[0] | (data[1] << 8);

    if (companyId != TARGET_COMPANY_ID) {
        return;
    }

    String mac = advertisedDevice.getAddress().toString().c_str();
    int8_t rssi = advertisedDevice.getRSSI();
    Serial.printf("[BLE] [DETECTADO] MAC=%s, RSSI=%d dBm, CompanyID=0x%04X, Bytes=%d\n",
                 mac.c_str(), rssi, companyId, static_cast<int>(mData.length()));

    // Si el payload corresponde al formato iBeacon, mostrar UUID, Major y Minor para debug
    if (mData.length() >= 25 && data[2] == 0x02 && data[3] == 0x15) {
        char uuidStr[37];
        snprintf(uuidStr, sizeof(uuidStr),
                 "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                 data[4], data[5], data[6], data[7],
                 data[8], data[9], data[10], data[11],
                 data[12], data[13], data[14], data[15],
                 data[16], data[17], data[18], data[19]);

        uint16_t major = (static_cast<uint16_t>(data[20]) << 8) | data[21];
        uint16_t minor = (static_cast<uint16_t>(data[22]) << 8) | data[23];

        Serial.printf("[BLE] [IBEACON] UUID=%s | Major=%u | Minor=%u\n",
                     uuidStr, major, minor);
    }

    uint32_t animalId = extractAnimalId(mData);

    if (animalId == 0) {
    Serial.println("[BLE]   [ERROR] Datos insuficientes para obtener ID");
        return;
    }
    
    // Calcular distancia
    float distance = calculateDistance(rssi);
    
    // Determinar si el animal está presente en la zona
    bool isPresent = (distance >= 0 && distance <= DISTANCE_MEDIUM);
    
    unsigned long currentTime = millis();
    
    // Actualizar o crear datos del beacon
    if (beacons.find(animalId) == beacons.end()) {
        // Nuevo beacon detectado
        BeaconData newBeacon;
        newBeacon.animalId = animalId;
        newBeacon.macAddress = advertisedDevice.getAddress().toString().c_str();
        newBeacon.rssi = rssi;
        newBeacon.distance = distance;
        newBeacon.firstSeen = currentTime;
        newBeacon.lastSeen = currentTime;
        newBeacon.isPresent = isPresent;
        newBeacon.detectedLocation = DEVICE_LOCATION;
        
        beacons[animalId] = newBeacon;
        
    Serial.printf("[BLE] [BEACON] Beacon: ID=%u, RSSI=%d dBm, Dist=%.2fm, MAC=%s\n",
                     animalId, rssi, distance, newBeacon.macAddress.c_str());
    } else {
        // Beacon existente - actualizar
        BeaconData& beacon = beacons[animalId];
        beacon.rssi = rssi;
        beacon.distance = distance;
        beacon.lastSeen = currentTime;
        beacon.isPresent = isPresent;
        if (beacon.detectedLocation.length() == 0) {
            beacon.detectedLocation = DEVICE_LOCATION;
        }
    }
    
    // Actualizar análisis de comportamiento
    updateBehavior(animalId, beacons[animalId]);
}

uint32_t BLEScanner::extractAnimalId(std::string manufacturerData) {
    /**
     * Formato manufacturer data del beacon iBeacon:
     * Bytes 0-1: Company ID (0x004C para Apple/iBeacon)
     * Bytes 2-3: iBeacon prefix (0x02 0x15)
     * Bytes 4-19: UUID (16 bytes)
     * Bytes 20-21: Major (2 bytes, big-endian)
     * Bytes 22-23: Minor (2 bytes, big-endian)
     * Byte 24: TX Power
     * 
     * Usamos Minor como tag_id único para identificar cada beacon.
     */
    
    if (manufacturerData.length() < 25) {
        Serial.printf("[BLE] [WARNING] Datos insuficientes: solo %d bytes\n", manufacturerData.length());
        return 0;
    }
    
    uint8_t* data = (uint8_t*)manufacturerData.data();
    size_t length = manufacturerData.length();
    
    // Verificar Company ID (primeros 2 bytes en little-endian)
    uint16_t companyId = data[0] | (data[1] << 8);
    
    Serial.printf("[BLE] [INSPECT] Analizando beacon: CompanyID=0x%04X, Length=%d\n", companyId, length);
    
    if (companyId == TARGET_COMPANY_ID) {
        Serial.println("[BLE]   [OK] Company ID coincide con TARGET_COMPANY_ID");
        
        // Verificar formato iBeacon (0x02 0x15)
        if (length >= 25 && data[2] == 0x02 && data[3] == 0x15) {
            // Extraer Minor (bytes 22-23, big-endian)
            uint16_t minor = (static_cast<uint16_t>(data[22]) << 8) | data[23];
            uint32_t animalId = static_cast<uint32_t>(minor);
            
            Serial.printf("[BLE]   [OK] Formato iBeacon: Animal ID=%u (Minor=%u)\n", animalId, minor);
            return animalId;
        } else {
            Serial.println("[BLE]   [WARNING] No es formato iBeacon valido");
            return 0;
        }
    }
    
    Serial.printf("[BLE]   [ERROR] Company ID 0x%04X no coincide (esperado: 0x%04X)\n", 
                 companyId, TARGET_COMPANY_ID);
    return 0;
}

// ==================== Callback para Beacons Detectados ====================
AnimalBeaconCallbacks::AnimalBeaconCallbacks(BLEScanner* scanner) 
    : bleScanner(scanner) {
}

void AnimalBeaconCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    // Delegar procesamiento al scanner principal
    bleScanner->processDevice(advertisedDevice);
}
