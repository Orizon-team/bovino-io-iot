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
        // Deinicializar BLE si ya estaba inicializado (por si es un reinicio)
        Serial.println("[BLE] Limpiando estado anterior de BLE...");
        BLEDevice::deinit(false);
        delay(100);  // Pequeña pausa para asegurar limpieza
        
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
        
        // Limpiar cualquier resultado anterior
        pBLEScan->clearResults();
        
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
        Serial.println("[BLE] ===============================================");
        Serial.printf("[BLE] 🔍 FILTRO ACTIVO: UUID = %s\n", BEACON_UUID_1);
        Serial.println("[BLE] ⚠️  Solo se procesarán beacons con este UUID");
        Serial.println("[BLE] ===============================================");
        
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
    
    // IMPORTANTE: Limpiar resultados anteriores antes de cada escaneo
    // Esto previene que el buffer se llene y deje de detectar beacons
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->clearResults();  // Limpiar resultados del escaneo anterior
    pBLEScan->start(duration, false);  // false = continuo, true = una sola vez
    
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
        
        // Buscar beacon correspondiente en el mapa (puede haber múltiples con diferentes MACs)
        bool found = false;
        for (const auto& beaconPair : beacons) {
            const BeaconData& beacon = beaconPair.second;
            if (beacon.animalId == animalId) {
                unsigned long timeSinceLastSeen = currentTime - beacon.lastSeen;
                
                // Si ha pasado mucho tiempo sin ver al animal
                if (timeSinceLastSeen > ANIMAL_MISSING_TIMEOUT && !behavior.missingAlert) {
                    behavior.missingAlert = true;
                    behavior.isPresent = false;
                    Serial.printf("[BLE] ⚠️ ALERTA: Animal ID=%u no detectado hace %lu horas\n",
                                 animalId, timeSinceLastSeen / 3600000);
                }
                found = true;
                break;  // Ya encontramos el beacon de este animal
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

std::map<String, BeaconData> BLEScanner::getBeaconData() {
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

// ==================== Funciones Auxiliares para iBeacon ====================
static bool matchesBeaconUUID(const uint8_t* uuid, size_t length) {
    if (length != 16) {
        Serial.println("[BLE]     ❌ UUID inválido: longitud incorrecta");
        return false;
    }
    
    // Convertir UUIDs configurados de string a bytes para comparación
    // UUID formato: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (32 hex chars + 4 guiones)
    const char* uuids[] = {BEACON_UUID_1, BEACON_UUID_2, BEACON_UUID_3};
    
    for (const char* uuidStr : uuids) {
        // Verificar que el UUID no sea el placeholder
        if (strcmp(uuidStr, "00000000-0000-0000-0000-000000000000") == 0) {
            continue;  // Saltar UUIDs no configurados
        }
        
        // Convertir string UUID a bytes
        uint8_t expectedUuid[16];
        int byteIndex = 0;
        for (int i = 0; i < strlen(uuidStr) && byteIndex < 16; i++) {
            if (uuidStr[i] == '-') continue;  // Saltar guiones
            
            char hex[3] = {uuidStr[i], uuidStr[i+1], 0};
            expectedUuid[byteIndex++] = (uint8_t)strtol(hex, NULL, 16);
            i++;  // Saltar el segundo carácter ya procesado
        }
        
        // Mostrar UUID detectado vs esperado (solo para debug)
        Serial.print("[BLE]     🔍 Comparando con UUID esperado: ");
        for (int i = 0; i < 16; i++) {
            Serial.printf("%02X", expectedUuid[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) Serial.print("-");
        }
        Serial.println();
        
        // Comparar UUIDs
        if (memcmp(uuid, expectedUuid, 16) == 0) {
            Serial.println("[BLE]     ✅✅✅ UUID COINCIDE - Beacon ACEPTADO ✅✅✅");
            return true;  // UUID coincide
        } else {
            Serial.println("[BLE]     ❌ UUID diferente - Beacon rechazado");
        }
    }
    
    return false;  // Ningún UUID coincidió
}

// ==================== Procesamiento de Beacons ====================
void BLEScanner::processDevice(BLEAdvertisedDevice advertisedDevice) {
    String mac = advertisedDevice.getAddress().toString().c_str();
    int8_t rssi = advertisedDevice.getRSSI();
    
    // 🔍 DEBUG: Mostrar TODOS los dispositivos detectados (temporal)
    Serial.printf("[BLE] 🔍 Dispositivo detectado: MAC=%s, RSSI=%d dBm\n", mac.c_str(), rssi);
    
    // ==================== FILTRO 1: RSSI MÍNIMO ====================
    if (rssi < MIN_RSSI_THRESHOLD) {
        Serial.printf("[BLE]   ❌ RSSI muy débil (< %d dBm), ignorado\n", MIN_RSSI_THRESHOLD);
        return;  // Señal muy débil, ignorar
    }
    
    // ==================== FILTRO 2: SEGÚN MODO CONFIGURADO ====================
    bool passesFilter = false;
    
    switch (BEACON_FILTER_MODE) {
        case FILTER_BY_UUID:
            // Filtrar por UUID de iBeacon
            if (advertisedDevice.haveManufacturerData()) {
                std::string mData = advertisedDevice.getManufacturerData();
                
                if (mData.length() >= 25) {  // Tamaño mínimo de iBeacon
                    uint8_t* data = (uint8_t*)mData.data();
                    uint16_t companyId = data[0] | (data[1] << 8);
                    
                    // Verificar que sea formato iBeacon (0x004C + tipo 0x02)
                    if (companyId == 0x004C && data[2] == 0x02 && data[3] == 0x15) {
                        // ENCONTRAMOS UN iBeacon - Mostrar información detallada
                        Serial.printf("\n[BLE] ╔══════════════════════════════════════════════════╗\n");
                        Serial.printf("[BLE] ║  📡 iBeacon DETECTADO                           ║\n");
                        Serial.printf("[BLE] ╠══════════════════════════════════════════════════╣\n");
                        Serial.printf("[BLE] ║  MAC:  %s                       ║\n", mac.c_str());
                        Serial.printf("[BLE] ║  RSSI: %d dBm                                  ║\n", rssi);
                        
                        // UUID está en bytes 4-19
                        const uint8_t* uuid = &data[4];
                        Serial.print("[BLE] ║  UUID: ");
                        for (int i = 0; i < 16; i++) {
                            Serial.printf("%02X", uuid[i]);
                            if (i == 3 || i == 5 || i == 7 || i == 9) Serial.print("-");
                        }
                        Serial.println(" ║");
                        
                        // Extraer Major y Minor
                        uint16_t major = (data[20] << 8) | data[21];
                        uint16_t minor = (data[22] << 8) | data[23];
                        int8_t txPower = (int8_t)data[24];
                        Serial.printf("[BLE] ║  Major: %-5u  Minor: %-5u  TxPower: %d      ║\n", 
                                     major, minor, txPower);
                        Serial.printf("[BLE] ╚══════════════════════════════════════════════════╝\n\n");
                        
                        // Ahora validar UUID
                        passesFilter = matchesBeaconUUID(uuid, 16);
                        if (!passesFilter) {
                            return;  // UUID no coincide
                        }
                    } else {
                        return;  // No es iBeacon válido, rechazar silenciosamente
                    }
                } else {
                    return;  // Datos insuficientes, rechazar silenciosamente
                }
            } else {
                return;  // No hay manufacturer data, rechazar silenciosamente
            }
            break;
            
        case FILTER_BY_MAC_PREFIX: {
            // Convertir a minúsculas para comparación
            String macLower = mac;
            macLower.toLowerCase();
            
            String prefixLower = String(BEACON_MAC_PREFIX);
            prefixLower.toLowerCase();
            
            // Verificar si la MAC comienza con el prefijo configurado
            if (!macLower.startsWith(prefixLower)) {
                return;  // No es uno de nuestros beacons
            }
            passesFilter = true;
            break;
        }
        
        case FILTER_BY_NAME_PREFIX:
            if (advertisedDevice.haveName()) {
                String name = advertisedDevice.getName().c_str();
                // Aquí podrías configurar un prefijo de nombre
                passesFilter = true;  // Por ahora acepta todos con nombre
            } else {
                return;  // Sin nombre, rechazar
            }
            break;
            
        case FILTER_BY_COMPANY_ID:
            if (advertisedDevice.haveManufacturerData()) {
                std::string mData = advertisedDevice.getManufacturerData();
                if (mData.length() >= 2) {
                    uint8_t* data = (uint8_t*)mData.data();
                    uint16_t companyId = data[0] | (data[1] << 8);
                    if (companyId == TARGET_COMPANY_ID) {
                        passesFilter = true;
                    } else {
                        return;  // Company ID no coincide
                    }
                }
            } else {
                return;  // Sin manufacturer data
            }
            break;
            
        case FILTER_DISABLED:
            passesFilter = true;  // Acepta todos
            break;
    }
    
    if (!passesFilter) {
        return;  // No pasó el filtro
    }
    
    // ✅ Este beacon pasa los filtros, procesarlo
    Serial.printf("[BLE] [DETECTADO] MAC=%s, RSSI=%d dBm", mac.c_str(), rssi);
    
    uint32_t animalId = 0;
    String beaconUUID = "";
    uint16_t beaconMajor = 0;
    uint16_t beaconMinor = 0;
    
    // OPCIÓN 1: Intentar extraer de manufacturer data
    if (advertisedDevice.haveManufacturerData()) {
        std::string mData = advertisedDevice.getManufacturerData();
        uint8_t* data = (uint8_t*)mData.data();
        size_t length = mData.length();
        
        // Mostrar Company ID detectado
        if (length >= 2) {
            uint16_t companyId = data[0] | (data[1] << 8);
            Serial.printf(", CompanyID=0x%04X, Bytes=%d", companyId, length);
            
            // Si es iBeacon (0x004C + tipo 0x02), extraer UUID, Major y Minor
            if (companyId == 0x004C && length >= 25 && data[2] == 0x02 && data[3] == 0x15) {
                // Extraer UUID (bytes 4-19)
                for (int i = 4; i < 20; i++) {
                    if (beaconUUID.length() > 0 && (i == 8 || i == 10 || i == 12 || i == 14)) {
                        beaconUUID += "-";
                    }
                    char hex[3];
                    sprintf(hex, "%02X", data[i]);
                    beaconUUID += hex;
                }
                
                // Extraer Major y Minor
                beaconMajor = (data[20] << 8) | data[21];
                beaconMinor = (data[22] << 8) | data[23];
            }
        }
        
        // Extraer ID del animal desde manufacturer data
        animalId = extractAnimalId(mData);
        
        if (animalId != 0) {
            Serial.printf(" - ✅ Animal ID=%u\n", animalId);
        }
    }
    
    // OPCIÓN 2: Si NO hay manufacturer data válido, usar MAC como ID
    if (animalId == 0) {
        Serial.print(" - Sin manufacturer data válido");
        
        // Convertir los últimos 4 bytes de la MAC a un ID único
        BLEAddress addr = advertisedDevice.getAddress();
        const uint8_t* macBytes = *addr.getNative();
        
        // Usar los últimos 4 bytes de la MAC como ID único
        animalId = (macBytes[2] << 24) | (macBytes[3] << 16) | 
                   (macBytes[4] << 8) | macBytes[5];
        
        Serial.printf(" → ID desde MAC=%u (0x%08X)\n", animalId, animalId);
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
    
    // Crear clave única: MAC + AnimalID para soportar múltiples iBeacons por MAC
    String beaconKey = mac + "_" + String(animalId);
    
    // Actualizar o crear datos del beacon
    if (beacons.find(beaconKey) == beacons.end()) {
        // Nuevo beacon detectado
        BeaconData newBeacon;
        newBeacon.animalId = animalId;
        newBeacon.macAddress = mac;
        newBeacon.uuid = beaconUUID;
        newBeacon.major = beaconMajor;
        newBeacon.minor = beaconMinor;
        newBeacon.rssi = rssi;
        newBeacon.distance = distance;
        newBeacon.firstSeen = currentTime;
        newBeacon.lastSeen = currentTime;
        newBeacon.isPresent = isPresent;
        
        beacons[beaconKey] = newBeacon;
        
        Serial.printf("[BLE] 📡 Beacon: ID=%u (Major=%u,Minor=%u), RSSI=%d dBm, Dist=%.2fm, MAC=%s\n",
                     animalId, beaconMajor, beaconMinor, rssi, distance, mac.c_str());
        Serial.printf("[BLE] 🔑 Clave única: %s\n", beaconKey.c_str());
        Serial.printf("[BLE] [VACA DETECTADA] Nuevo animal detectado: ID=%u, Distancia=%.2fm\n",
                     animalId, distance);
    } else {
        // Beacon existente - actualizar
        BeaconData& beacon = beacons[beaconKey];
        beacon.rssi = rssi;
        beacon.distance = distance;
        beacon.lastSeen = currentTime;
        beacon.isPresent = isPresent;
    }
    
    // Actualizar análisis de comportamiento
    updateBehavior(animalId, beacons[beaconKey]);
}

uint32_t BLEScanner::extractAnimalId(std::string manufacturerData) {
    /**
     * Formato manufacturer data del beacon:
     * 
     * FORMATO iBeacon (FEASYBeacon y compatibles):
     * Bytes 0-1:   Company ID (0x004C para Apple/iBeacon)
     * Byte 2:      Tipo de beacon (0x02 para iBeacon)
     * Byte 3:      Longitud (0x15 = 21 bytes)
     * Bytes 4-19:  UUID (16 bytes)
     * Bytes 20-21: Major (2 bytes, big-endian)
     * Bytes 22-23: Minor (2 bytes, big-endian)
     * Byte 24:     TX Power
     * 
     * SOPORTA TAMBIÉN:
     * - 0x1234 (Formato anterior del proyecto)
     * - Otros formatos personalizados
     */
    
    if (manufacturerData.length() < 6) {
        Serial.printf("[BLE] ⚠️ Datos insuficientes: solo %d bytes\n", manufacturerData.length());
        return 0;
    }
    
    uint8_t* data = (uint8_t*)manufacturerData.data();
    size_t length = manufacturerData.length();
    
    uint16_t companyId = data[0] | (data[1] << 8);
    
    Serial.printf("[BLE] 🔍 Analizando beacon: CompanyID=0x%04X, Length=%d\n", companyId, length);
    
    // ==================== FORMATO iBeacon (0x004C) ====================
    if (companyId == 0x004C && length >= 25) {
        // Verificar que sea iBeacon válido
        if (data[2] == 0x02 && data[3] == 0x15) {
            Serial.println("[BLE]   ✓ Formato iBeacon detectado");
            
            // Extraer UUID (solo para mostrar)
            Serial.print("[BLE]   UUID: ");
            for (int i = 4; i < 20; i++) {
                Serial.printf("%02X", data[i]);
                if (i == 7 || i == 9 || i == 11 || i == 13) Serial.print("-");
            }
            Serial.println();
            
            // Extraer Major y Minor (big-endian)
            uint16_t major = (data[20] << 8) | data[21];
            uint16_t minor = (data[22] << 8) | data[23];
            int8_t txPower = (int8_t)data[24];
            
            Serial.printf("[BLE]   Major=%u, Minor=%u, TxPower=%d dBm\n", major, minor, txPower);
            
            // Extraer ID del animal según configuración
            uint32_t animalId = 0;
            
            switch (ANIMAL_ID_SOURCE) {
                case USE_MAJOR_MINOR:
                    // Combinar Major (16 bits altos) + Minor (16 bits bajos)
                    animalId = ((uint32_t)major << 16) | minor;
                    Serial.printf("[BLE]   ✓ Animal ID=%u (Major+Minor)\n", animalId);
                    break;
                    
                case USE_MAJOR_ONLY:
                    // Solo Major
                    animalId = major;
                    Serial.printf("[BLE]   ✓ Animal ID=%u (Major)\n", animalId);
                    break;
                    
                case USE_MINOR_ONLY:
                    // Solo Minor
                    animalId = minor;
                    Serial.printf("[BLE]   ✓ Animal ID=%u (Minor)\n", animalId);
                    break;
                    
                case USE_MAC_ADDRESS:
                    // Se manejará fuera de esta función
                    Serial.println("[BLE]   ⚠️ Usar MAC como ID (se procesará después)");
                    return 0;
            }
            
            return animalId;
        }
    }
    
    // ==================== FORMATO LEGACY (0x1234) ====================
    if (companyId == 0x1234) {
        Serial.println("[BLE]   ✓ Company ID es 0x1234 (formato anterior)");
        
        if (length >= 11) {
            uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16);
            Serial.printf("[BLE]   ✓ Animal ID=%u (3 bytes)\n", animalId);
            return animalId;
        } else if (length >= 6) {
            uint32_t animalId = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
            Serial.printf("[BLE]   ✓ Animal ID=%u (4 bytes)\n", animalId);
            return animalId;
        }
    }
    
    Serial.printf("[BLE]   ❌ Formato no reconocido (CompanyID=0x%04X)\n", companyId);
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
