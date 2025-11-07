#include "ble_scanner.h"
#include <cmath>

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
        Serial.println("[BLE] ✓ Bluetooth inicializado");
        
        // Obtener objeto de escaneo
        BLEScan* pBLEScan = BLEDevice::getScan();
        
        if (pBLEScan == nullptr) {
            Serial.println("[BLE] ❌ Error: No se pudo obtener objeto de escaneo");
            return false;
        }
        
        // Configurar callback para beacons detectados
        pBLEScan->setAdvertisedDeviceCallbacks(new AnimalBeaconCallbacks(this));
        
        // Configurar escaneo activo para obtener más información
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(100);  // Intervalo entre escaneos
        pBLEScan->setWindow(99);     // Ventana de escaneo
        
        Serial.println("[BLE] ✓ Sistema listo para detectar beacons de ganado");
        Serial.printf("[BLE] Modo inicial: %s\n", 
                     currentMode == MODE_ACTIVE ? "ACTIVO" : 
                     currentMode == MODE_NORMAL ? "NORMAL" : "ECO");
        
        return true;
        
    } catch (...) {
        Serial.println("[BLE] ❌ Error fatal durante inicialización");
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
        Serial.printf("[BLE] ⚠ Cambio detectado: %d → %d animales\n", 
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
            Serial.printf("[BLE] 🐄 Animal regresó: ID=%u (Visita #%d)\n", 
                         animalId, behavior.visitCount);
        }
        
        // Si estaba presente y ahora está ausente = salida
        if (behavior.isPresent && !beacon.isPresent) {
            behavior.exitTime = currentTime;
            behavior.timeInZone += (currentTime - behavior.entryTime);
            Serial.printf("[BLE] 🐄 Animal salió: ID=%u (Tiempo en zona: %lu min)\n", 
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
                Serial.printf("[BLE] ⚠️ ALERTA: Animal ID=%u no detectado hace %lu horas\n",
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
        Serial.printf("[BLE] 🔄 Cambio de modo: %s → %s\n",
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
    // SIEMPRE mostrar lo que se detecta (para debug)
    String mac = advertisedDevice.getAddress().toString().c_str();
    int8_t rssi = advertisedDevice.getRSSI();
    
    Serial.printf("[BLE] [DETECTADO] Detectado: MAC=%s, RSSI=%d dBm", mac.c_str(), rssi);
    
    uint32_t animalId = 0;
    
    // OPCIÓN 1: Intentar extraer de manufacturer data
    if (advertisedDevice.haveManufacturerData()) {
        std::string mData = advertisedDevice.getManufacturerData();
        uint8_t* data = (uint8_t*)mData.data();
        size_t length = mData.length();
        
        // Mostrar Company ID detectado
        if (length >= 2) {
            uint16_t companyId = data[0] | (data[1] << 8);
            Serial.printf(", CompanyID=0x%04X, Bytes=%d", companyId, length);
        }
        
        // Extraer ID del animal desde manufacturer data
        animalId = extractAnimalId(mData);
        
        if (animalId != 0) {
            Serial.printf(" - ✅ Animal ID=%u (desde manufacturer data)\n", animalId);
        }
    }
    
    // OPCIÓN 2: Si NO hay manufacturer data, usar MAC como ID
    if (animalId == 0) {
        Serial.print(" - Sin manufacturer data");
        
        // Convertir los últimos 4 bytes de la MAC a un ID único
        // Ejemplo: dc:0d:30:2c:e8:c6 → usamos los últimos 4 bytes
        BLEAddress addr = advertisedDevice.getAddress();
        const uint8_t* macBytes = *addr.getNative();  // Desreferenciar el puntero
        
        // Usar los últimos 4 bytes de la MAC como ID único
        animalId = (macBytes[2] << 24) | (macBytes[3] << 16) | 
                   (macBytes[4] << 8) | macBytes[5];
        
        Serial.printf(" → Usando MAC como ID=%u (0x%08X)\n", animalId, animalId);
    }
    
    // Si aún no hay ID válido, salir
    if (animalId == 0) {
        Serial.println(" - ❌ No se pudo generar ID");
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
        
        beacons[animalId] = newBeacon;
        
        Serial.printf("[BLE] 📡 Beacon: ID=%u, RSSI=%d dBm, Dist=%.2fm, MAC=%s\n",
                     animalId, rssi, distance, newBeacon.macAddress.c_str());
    } else {
        // Beacon existente - actualizar
        BeaconData& beacon = beacons[animalId];
        beacon.rssi = rssi;
        beacon.distance = distance;
        beacon.lastSeen = currentTime;
        beacon.isPresent = isPresent;
    }
    
    // Actualizar análisis de comportamiento
    updateBehavior(animalId, beacons[animalId]);
}

uint32_t BLEScanner::extractAnimalId(std::string manufacturerData) {
    /**
     * Formato manufacturer data del beacon:
     * Bytes 0-1: Company ID
     * Bytes 2-X: Payload con ID del animal
     * 
     * SOPORTA MÚLTIPLES FORMATOS:
     * - 0x004C (Apple iBeacon)
     * - 0x1234 (Formato anterior del proyecto)
     * - Otros Company IDs configurables
     */
    
    if (manufacturerData.length() < 6) {
        Serial.printf("[BLE] ⚠️ Datos insuficientes: solo %d bytes\n", manufacturerData.length());
        return 0;  // Datos insuficientes
    }
    
    uint8_t* data = (uint8_t*)manufacturerData.data();
    size_t length = manufacturerData.length();
    
    // Verificar Company ID (primeros 2 bytes en little-endian)
    uint16_t companyId = data[0] | (data[1] << 8);
    
    Serial.printf("[BLE] 🔍 Analizando beacon: CompanyID=0x%04X, Length=%d\n", companyId, length);
    
    // OPCIÓN 1: Aceptar el TARGET_COMPANY_ID configurado
    if (companyId == TARGET_COMPANY_ID) {
        Serial.println("[BLE]   ✓ Company ID coincide con TARGET_COMPANY_ID");
        
        // Verificar que hay suficientes bytes para el ID
        if (length < 11) {  // 2 (company) + 1 (type) + 8 (UUID) si es iBeacon
            // Formato simple: Company ID + 4 bytes de Animal ID
            if (length >= 6) {
                uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
                Serial.printf("[BLE]   ✓ Formato simple: Animal ID=%u\n", animalId);
                return animalId;
            }
        } else {
            // Formato iBeacon completo: extraer ID desde posición específica
            // iBeacon: 2 bytes company + 1 byte type + 1 byte length + 16 bytes UUID + 2 major + 2 minor
            uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
            Serial.printf("[BLE]   ✓ Formato iBeacon: Animal ID=%u\n", animalId);
            return animalId;
        }
    }
    
    // OPCIÓN 2: Aceptar 0x1234 (formato anterior del proyecto)
    if (companyId == 0x1234) {
        Serial.println("[BLE]   ✓ Company ID es 0x1234 (formato anterior)");
        
        if (length >= 11) {
            // Formato anterior: 2 bytes company + 3 bytes studentId + 8 bytes UUID
            uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16);
            Serial.printf("[BLE]   ✓ Animal ID=%u (3 bytes)\n", animalId);
            return animalId;
        } else if (length >= 6) {
            // Formato simplificado
            uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
            Serial.printf("[BLE]   ✓ Animal ID=%u (4 bytes)\n", animalId);
            return animalId;
        }
    }
    
    // OPCIÓN 3: Modo permisivo - aceptar CUALQUIER beacon (solo para debug)
    // Descomentar las siguientes líneas si quieres aceptar todos los beacons:
    /*
    Serial.println("[BLE]   ⚠️ Modo permisivo: aceptando beacon desconocido");
    if (length >= 6) {
        uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
        Serial.printf("[BLE]   Animal ID=%u (genérico)\n", animalId);
        return animalId;
    }
    */
    
    Serial.printf("[BLE]   ❌ Company ID 0x%04X no coincide (esperado: 0x%04X)\n", 
                 companyId, TARGET_COMPANY_ID);
    return 0;  // No es nuestro beacon
}

// ==================== Callback para Beacons Detectados ====================
AnimalBeaconCallbacks::AnimalBeaconCallbacks(BLEScanner* scanner) 
    : bleScanner(scanner) {
}

void AnimalBeaconCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
    // Delegar procesamiento al scanner principal
    bleScanner->processDevice(advertisedDevice);
}
