#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ble_scanner.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "display_manager.h"
#include "alerts.h"
#include "espnow_manager.h"

unsigned long lastSyncTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastESPNowSend = 0;
bool systemReady = false;
bool deviceRegistrationComplete = false;
bool deviceAlreadyConfigured = false;

static const char* zoneTypeToCode(ZoneType type) {
    switch (type) {
        case ZONE_FEEDER:  return "FEEDER";
        case ZONE_WATERER: return "WATERER";
        case ZONE_PASTURE: return "PASTURE";
        case ZONE_REST:    return "REST";
        default:           return "GENERIC";
    }
}

void displayZoneStatus() {
    int animalCount = bleScanner.getAnimalCount();
    
    // Línea 1: Nombre de la zona y conteo
    String line1 = String(DEVICE_LOCATION);
    if (line1.length() > 10) {
        line1 = line1.substring(0, 10);  // Truncar si es muy largo
    }
    line1 += " ";
    line1 += String(animalCount);
    line1 += " [ANIMAL]";
    
    // Línea 2: Modo de escaneo
    ScanMode mode = bleScanner.getCurrentMode();
    String line2 = "Modo: ";
    if (mode == MODE_ACTIVE) line2 += "ACTIVO ";
    else if (mode == MODE_NORMAL) line2 += "NORMAL ";
    else line2 += "ECO    ";
    
    displayManager.showMessage(line1.c_str(), line2.c_str());
}

void setup() {
    // Inicializar serial
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║     ESP32 - BovinoIOT v2.0               ║");
    Serial.println("║  Sistema de Monitoreo de Ganado         ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();
    
    // Mostrar información de la zona
    Serial.printf("Zona: %s\n", DEVICE_LOCATION);
    Serial.printf("ID: %s\n", DEVICE_ID);
    
    const char* zoneTypeName = "";
    switch (CURRENT_ZONE_TYPE) {
        case ZONE_FEEDER:  zoneTypeName = "Comedero"; break;
        case ZONE_WATERER: zoneTypeName = "Bebedero"; break;
        case ZONE_PASTURE: zoneTypeName = "Pastoreo"; break;
        case ZONE_REST:    zoneTypeName = "Descanso"; break;
        default:           zoneTypeName = "Genérica"; break;
    }
    Serial.printf("Tipo: %s\n\n", zoneTypeName);
    
    // Inicializar hardware
    alertManager.initialize();
    alertManager.loaderOn();
    
    displayManager.initialize();
    displayManager.showMessage("BovinoIOT v2.0", "Iniciando...");
    delay(2000);

    // Mostrar ID del dispositivo antes de validar registro
    String deviceIdLabel = String("ID: ") + DEVICE_ID;
    displayManager.showMessage("Dispositivo", deviceIdLabel.c_str());
    Serial.printf("[REG] Mostrando ID del dispositivo: %s\n", DEVICE_ID);
    delay(5000);

    Serial.println("[REG] Iniciando verificación de registro con backend (simulado)...");
    deviceAlreadyConfigured = apiClient.checkDeviceRegistration(DEVICE_ID);

    if (!deviceAlreadyConfigured) {
        displayManager.showMessage("Registrando...", deviceIdLabel.c_str());
        if (apiClient.registerDevice(DEVICE_ID, DEVICE_LOCATION, CURRENT_ZONE_TYPE)) {
            deviceRegistrationComplete = true;
            alertManager.showSuccess();
            displayManager.showMessage("Registro listo", deviceIdLabel.c_str());
            delay(1500);
        }
    } else {
        deviceRegistrationComplete = true;
        alertManager.showSuccess();
        displayManager.showMessage("ID verificado", deviceIdLabel.c_str());
        delay(1500);
    }

    if (!deviceRegistrationComplete) {
    Serial.println("[REG] [ERROR] No se pudo registrar el dispositivo. Reiniciar requerido.");
        displayManager.showMessage("Registro fallido", "Reinicie");
        alertManager.showError();
        while (true) {
            alertManager.showDanger();
            delay(1000);
        }
    }

    Serial.printf("[REG] [OK] Dispositivo %s\n",
                 deviceAlreadyConfigured ? "ya estaba registrado" : "registrado por primera vez");
    
    // Inicializar BLE
    Serial.println("[INIT] Inicializando Bluetooth...");
    displayManager.showMessage("Iniciando BLE", "Espere...");
    
    if (!bleScanner.initialize()) {
    Serial.println("[INIT] [ERROR] ERROR FATAL: BLE no inicializado");
        displayManager.showMessage("ERROR BLE", "Reiniciar ESP32");
        alertManager.showError();
        while (1) {
            alertManager.showDanger();
            delay(1000);
        }
    }
    
    Serial.println("[INIT] [OK] BLE inicializado correctamente\n");
    
    // Mostrar modo de operación
    const char* modeName = (CURRENT_DEVICE_MODE == DEVICE_MASTER) ? "MAESTRO" : "ESCLAVO";
    Serial.printf("[INIT] Modo: %s\n\n", modeName);
    
    // Inicializar según modo
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        // MODO MAESTRO: WiFi + ESP-NOW
        Serial.println("[INIT] Inicializando como MAESTRO...");
        displayManager.showMessage("Modo: MAESTRO", "Init...");
        
        // Inicializar ESP-NOW primero
        if (!espNowManager.initializeMaster()) {
            Serial.println("[INIT] [ERROR] Error al inicializar ESP-NOW");
            alertManager.showError();
        }
        
        // Conectar WiFi
        Serial.println("[INIT] Conectando WiFi...");
        displayManager.showMessage("Conectando WiFi", WIFI_SSID);
        
        if (wifiManager.connect()) {
            Serial.printf("[INIT] [OK] WiFi conectado: %s\n", wifiManager.getLocalIP().c_str());
            alertManager.showSuccess();
            delay(1000);
            apiClient.initializeTimeSync();
        } else {
            Serial.println("[INIT] [WARNING] Sin WiFi - Solo ESP-NOW");
            displayManager.showMessage("Sin WiFi", "Solo ESP-NOW");
            delay(2000);
        }
        
    } else {
        // MODO ESCLAVO: Solo ESP-NOW (sin WiFi)
        Serial.println("[INIT] Inicializando como ESCLAVO...");
        displayManager.showMessage("Modo: ESCLAVO", "Sin WiFi");
        
        if (!espNowManager.initializeSlave()) {
        Serial.println("[INIT] [ERROR] Error al inicializar ESP-NOW");
            displayManager.showMessage("ERROR ESP-NOW", "Verificar config");
            alertManager.showError();
            while (1) {
                alertManager.showDanger();
                delay(1000);
            }
        }
        
    Serial.println("[INIT] [OK] Esclavo listo para enviar a maestro");
        alertManager.showSuccess();
        delay(1000);
    }
    
    alertManager.loaderOff();
    alertManager.showSuccess();
    
    displayManager.showMessage(DEVICE_LOCATION, "Sistema listo!");
    delay(2000);
    
    systemReady = true;
    lastSyncTime = millis();
    lastDisplayUpdate = millis();
    lastESPNowSend = millis();
    
    Serial.println("\n[INIT] [SUCCESS] Sistema inicializado - Comenzando monitoreo\n");
}

void loop() {
    if (!systemReady) {
        delay(100);
        return;
    }
    
    unsigned long now = millis();
    
    // ==================== 1. ESCANEO ADAPTATIVO ====================
    // El scanner maneja internamente los intervalos según el modo (ACTIVE/NORMAL/ECO)
    bleScanner.performScan();
    
    // ==================== 2. ACTUALIZAR DISPLAY ====================
    // Actualizar cada 3 segundos para no saturar
    if (now - lastDisplayUpdate > 3000) {
        displayZoneStatus();
        lastDisplayUpdate = now;
        
        // Verificar alertas de animales ausentes
        std::vector<uint32_t> missing = bleScanner.getMissingAnimals();
        if (missing.size() > 0) {
            Serial.printf("[ALERTA] [WARNING] %d animales no detectados hace 24h\n", missing.size());
            alertManager.showDanger();
            
            // Mostrar alerta en LCD brevemente
            String alertMsg = String(missing.size()) + " animales";
            displayManager.showMessage("ALERTA!", alertMsg.c_str());
            delay(2000);
        }
    }
    
    // ==================== 3. ESP-NOW: ESCLAVO ENVÍA A MAESTRO ====================
    if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
        if (now - lastESPNowSend > SLAVE_SEND_INTERVAL) {
            lastESPNowSend = now;
            
            std::map<uint32_t, BeaconData> beacons = bleScanner.getBeaconData();
            
            if (beacons.size() > 0) {
                Serial.printf("\n[ESP-NOW] ━━━━━ Enviando a Maestro (cada 10s) ━━━━━\n");
                Serial.printf("[ESP-NOW] Animales detectados: %d\n", beacons.size());
                
                alertManager.loaderOn();
                displayManager.showMessage("Enviando...", "A maestro");
                
                for (const auto& pair : beacons) {
                    const BeaconData& beacon = pair.second;
                    
                    ESPNowMessage msg;
                    strncpy(msg.deviceId, DEVICE_ID, sizeof(msg.deviceId) - 1);
                    strncpy(msg.location, DEVICE_LOCATION, sizeof(msg.location) - 1);
                    msg.zoneType = CURRENT_ZONE_TYPE;
                    msg.animalId = beacon.animalId;
                    msg.rssi = beacon.rssi;
                    msg.distance = beacon.distance;
                    msg.isPresent = beacon.isPresent;
                    msg.timestamp = millis();
                    
                    if (espNowManager.sendToMaster(msg)) {
                        alertManager.flashLED(LED_SUCCESS, 1, 50);
                    } else {
                        alertManager.flashLED(LED_ERROR, 1, 50);
                    }
                    
                    delay(50);
                }
                
                alertManager.loaderOff();
                alertManager.showSuccess();
                Serial.println("[ESP-NOW] [OK] Datos enviados al maestro\n");
                displayManager.showMessage("Enviado OK", String(beacons.size()) + " animales");
                delay(1500);
            }
        }
    }
    
    // ==================== 4. ESP-NOW: MAESTRO PROCESA Y SINCRONIZA ====================
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        // Procesar mensajes recibidos de esclavos
        std::vector<ESPNowMessage> receivedMsgs = espNowManager.getReceivedMessages();
        
        if (receivedMsgs.size() > 0) {
            Serial.printf("\n[MAESTRO] [INBOX] Mensajes de esclavos: %d\n", receivedMsgs.size());
            
            alertManager.loaderOn();
            displayManager.showMessage("Recibiendo...", "Datos esclavo");
            
            for (const auto& msg : receivedMsgs) {
                Serial.printf("[MAESTRO]   Esclavo: %s (%s)\n", msg.deviceId, msg.location);
                Serial.printf("[MAESTRO]   Animal ID=%u, RSSI=%d, Dist=%.2fm\n",
                             msg.animalId, msg.rssi, msg.distance);

                BeaconData beacon;
                beacon.animalId = msg.animalId;
                beacon.macAddress = String(msg.deviceId);
                beacon.rssi = msg.rssi;
                beacon.distance = msg.distance;
                beacon.firstSeen = msg.timestamp;
                beacon.lastSeen = msg.timestamp;
                beacon.isPresent = msg.isPresent;
                slaveBeaconData[msg.animalId] = beacon;
            }
            
            alertManager.loaderOff();
            alertManager.flashLED(LED_SUCCESS, 2, 100);
            displayManager.showMessage("Recibido OK", String(receivedMsgs.size()) + " mensajes");
            delay(1500);
            
            espNowManager.clearReceivedMessages();
        }
        
        // Sincronizar con backend cada 1 minuto (simulado)
        if (now - lastSyncTime > MASTER_SYNC_INTERVAL) {
            lastSyncTime = now;
            
            // Verificar conexión WiFi
            if (!wifiManager.isConnected()) {
                Serial.println("[SYNC] WiFi desconectado, intentando reconectar...");
                displayManager.showMessage("Conectando", "WiFi...");
                wifiManager.reconnect();
                
                if (!wifiManager.isConnected()) {
                    Serial.println("[SYNC] Sin WiFi - Datos en buffer offline");
                    displayManager.showMessage("Sin WiFi", "Modo offline");
                    delay(2000);
                    return;
                }
            }
            
            // Obtener datos locales + datos de esclavos
            std::map<uint32_t, BeaconData> beacons = bleScanner.getBeaconData();
            std::map<uint32_t, BeaconData> allBeacons = beacons;

            for (const auto& pair : slaveBeaconData) {
                allBeacons[pair.first] = pair.second;
            }
            
            if (allBeacons.size() == 0) {
                Serial.println("[SYNC] No hay datos para sincronizar");
                return;
            }
            
            Serial.printf("\n[SYNC] ━━━━━ Sincronización con API (cada 1 min) ━━━━━\n");
            Serial.printf("[SYNC] Zona local: %s (%s)\n", DEVICE_LOCATION, DEVICE_ID);
            Serial.printf("[SYNC] Animales detectados (total): %d\n", allBeacons.size());
            
            displayManager.showMessage("Enviando API", "Espere...");
            alertManager.loaderOn();
            
            DynamicJsonDocument payloadDoc(1024 + allBeacons.size() * 128);
            payloadDoc["device_id"] = DEVICE_ID;
            payloadDoc["location"] = DEVICE_LOCATION;
            payloadDoc["zone_type"] = zoneTypeToCode(CURRENT_ZONE_TYPE);
            payloadDoc["timestamp"] = static_cast<uint64_t>(now);

            JsonArray animalsArray = payloadDoc.createNestedArray("animals");

            for (const auto& pair : allBeacons) {
                const BeaconData& beacon = pair.second;
                Serial.printf("[SYNC]   [ANIMAL] ID=%u, Dist=%.2fm, RSSI=%d dBm\n",
                             beacon.animalId, beacon.distance, beacon.rssi);

                JsonObject animal = animalsArray.createNestedObject();
                animal["tag_id"] = beacon.animalId;
                animal["distance"] = beacon.distance;
                animal["rssi"] = beacon.rssi;
                animal["is_present"] = beacon.isPresent;
                animal["timestamp"] = beacon.lastSeen;
            }

            String payload;
            serializeJson(payloadDoc, payload);
            
            Serial.println("[SYNC] [HTTP-POST] Simulando envío HTTP POST...");
            Serial.printf("[SYNC] URL: %s\n", API_URL);
            Serial.printf("[SYNC] [PAYLOAD] %s\n", payload.c_str());
            
            // Simulación de envío (pausa para simular latencia de red)
            delay(800);
            
            alertManager.loaderOff();
            alertManager.showSuccess();
            displayManager.showMessage("API OK!", String(allBeacons.size()) + " animales");
            delay(2000);
            
            Serial.println("[SYNC] [SUCCESS] Datos enviados al backend (simulado)\n");

            slaveBeaconData.clear();
        }
    }
    
    // ==================== 4. MANTENIMIENTO ====================
    // Verificar periódicamente conexión WiFi
    if(CURRENT_DEVICE_MODE == DEVICE_MASTER)
    {
        static unsigned long lastWifiCheck = 0;
        if (now - lastWifiCheck > 30000) {  // Cada 30 segundos
            lastWifiCheck = now;
            
            if (!wifiManager.isConnected()) {
                Serial.println("[WiFi] Reconectando...");
                wifiManager.reconnect();
            }
        }
    }
    
    // Pequeña pausa para no saturar el CPU
    delay(100);
}