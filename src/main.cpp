#include <Arduino.h>
#include "config.h"
#include "ble_scanner.h"
#include "wifi_manager.h"
#include "mqtt_client.h"
#include "display_manager.h"
#include "alerts.h"
#include "espnow_manager.h"
#include <Preferences.h>
#include <esp_system.h>
#include <ArduinoJson.h>

unsigned long lastCycleTime = 0;
bool systemReady = false;

void printWelcomeMessage();
void checkResetButtonOnStartup();
bool loadDeviceConfiguration();
void handleConfigurationPortal();
void initializeDisplay();
void initializeAlerts();
void initializeDeviceMode();
void initializeMasterMode();
void initializeSlaveMode();
void initializeBLE();
void finishSetup();
void processSlaveCycle();
void processMasterCycle();
void processRegistrationCycle();
void handleResetButtonInLoop();

void setup() {
    Serial.begin(115200);
    delay(1000);  
    printWelcomeMessage();
    checkResetButtonOnStartup();
    bool configurationExists = loadDeviceConfiguration();
    initializeDisplay();
    if (!configurationExists) {
        handleConfigurationPortal();
    }
    initializeAlerts();
    initializeDeviceMode();
    initializeBLE();
    finishSetup();
}

void loop() {
    unsigned long now = millis();
    
    // ==================== DETECCIÓN DE BOTONES (SIEMPRE ACTIVO) ====================
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        checkModeButtonPress();  // Detecta cambios de modo
    }
    handleResetButtonInLoop();  // Detecta botón de reset
    
    // ==================== CICLO CADA 2 SEGUNDOS ====================
    if (now - lastCycleTime >= SCAN_CYCLE_INTERVAL) {
        lastCycleTime = now;
        
        if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
            processSlaveCycle();
        } else {
            // En modo MAESTRO: verificar si estamos en modo REGISTRO o NORMAL
            if (isRegistrationModeActive()) {
                processRegistrationCycle();
            } else {
                processMasterCycle();
            }
        }
    }
    
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER && ENABLE_MQTT) {
        mqttClient.loop();
    }
    
    delay(10);  // Small delay para evitar watchdog
}

void printWelcomeMessage() {
    Serial.println("\n");
    Serial.println("==========================================");
    Serial.println("    ESP32 - BovinoIOT v2.0 SIMPLE");
    Serial.println("   Sistema de Monitoreo de Ganado");
    Serial.println("==========================================");
    Serial.println();
}

void checkResetButtonOnStartup() {
    initResetButton();
    Serial.println("[MAIN] Verificando botón de reset (3 segundos)...");
    unsigned long resetCheckStart = millis();
    while (millis() - resetCheckStart < 3000) {
        if (checkResetButton()) {
            Serial.println("[MAIN] Reiniciando configuración...");     
            Preferences prefsDevice, prefsLocation, prefsWifi;
            prefsDevice.begin("device_cfg", false);
            prefsDevice.clear();
            prefsDevice.end();
            delay(100);
            prefsLocation.begin("location_cfg", false);
            prefsLocation.clear();
            prefsLocation.end();
            delay(100);
            prefsWifi.begin("wifi_cfg", false);
            prefsWifi.clear();
            prefsWifi.end();
            delay(100);
            Serial.println("[MAIN] Configuración borrada. Reiniciando...");
            delay(1000);
            ESP.restart();
        }
        delay(10);
    }
}

bool loadDeviceConfiguration() {
    Serial.println("[MAIN] Cargando configuración guardada...");
    
    Preferences prefsDevice;
    bool configurationExists = false;
    
    if (prefsDevice.begin("device_cfg", true)) {  
        uint8_t savedMode = prefsDevice.getUChar("mode", 255);
        
        if (savedMode == 1) {
            CURRENT_DEVICE_MODE = DEVICE_MASTER;
            configurationExists = true;
            Serial.println("[MAIN] Modo cargado: MAESTRO");
        } else if (savedMode == 0) {
            CURRENT_DEVICE_MODE = DEVICE_SLAVE;
            configurationExists = true;
            Serial.println("[MAIN] Modo cargado: ESCLAVO");
            
            String masterMac = prefsDevice.getString("master_mac", "");
            if (masterMac.length() > 0) {
                Serial.printf("[MAIN] MAC Maestro: %s\n", masterMac.c_str());
                
                // Convertir string de MAC a array de bytes
                int values[6];
                if (sscanf(masterMac.c_str(), "%x:%x:%x:%x:%x:%x",
                           &values[0], &values[1], &values[2],
                           &values[3], &values[4], &values[5]) == 6) {
                    for (int i = 0; i < 6; i++) {
                        MASTER_MAC_ADDRESS[i] = (uint8_t)values[i];
                    }
                    Serial.println("[MAIN] ✓ MAC del maestro cargada correctamente");
                } else {
                    Serial.println("[MAIN] ⚠️ Error al parsear MAC del maestro");
                }
            }
        } else {
            Serial.println("[MAIN]   Sin configuración guardada");
        }
        prefsDevice.end();
    } else {
        Serial.println("[MAIN]   Sin configuración guardada");
    }
    
    Preferences prefsLocation;
    if (prefsLocation.begin("location_cfg", true)) {
        LOADED_ZONE_NAME = prefsLocation.getString("zone_name", "");
        LOADED_SUB_LOCATION = prefsLocation.getString("sub_location", "");
        LOADED_ZONE_ID = prefsLocation.getInt("zone_id", 0);
        
        if (LOADED_ZONE_NAME.length() > 0) {
            Serial.printf("[MAIN] Zona cargada: %s (ID: %d)\n", LOADED_ZONE_NAME.c_str(), LOADED_ZONE_ID);
        }
        if (LOADED_SUB_LOCATION.length() > 0) {
            Serial.printf("[MAIN] Sub-ubicación: %s\n", LOADED_SUB_LOCATION.c_str());
            
            String deviceType, locationName;
            int slashPos = LOADED_SUB_LOCATION.indexOf('/');
            
            if (slashPos > 0) {
                deviceType = LOADED_SUB_LOCATION.substring(0, slashPos);
                locationName = LOADED_SUB_LOCATION.substring(slashPos + 1);
            } else {
                locationName = LOADED_SUB_LOCATION;
            }
            
            deviceType.toUpperCase();
            locationName.toUpperCase();
            locationName.replace(" ", "_");
            locationName.replace("-", "_");
            
            LOADED_DEVICE_ID = "IOT_" + deviceType + "_" + locationName;
            Serial.printf("[MAIN] ID Dispositivo: %s\n", LOADED_DEVICE_ID.c_str());
        }
        prefsLocation.end();
    }
    
    return configurationExists;
}

void handleConfigurationPortal() {
    Serial.println("\n[MAIN] ========================================");
    Serial.println("[MAIN]   NO HAY CONFIGURACION");
    Serial.println("[MAIN]   Iniciando portal de configuracion...");
    Serial.println("[MAIN] ========================================\n");
    Serial.println("[MAIN]      BLE deshabilitado durante portal para liberar memoria");
    
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[18];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    Serial.println("\n===============================================");
    Serial.printf("  MAC: %s\n", macStr);
    Serial.println("===============================================\n");
    
    displayManager.showMessage("Portal Config", macStr);
    wifiManager.startConfigPortal();

    while (wifiManager.isPortalActive()) {
        wifiManager.loop();
        delay(10);
    }
    
    Serial.println("[MAIN] Configuración completada. Reiniciando...");
    delay(1000);
    ESP.restart();
}

void initializeDisplay() {
    displayManager.initialize();
}

void initializeAlerts() {
    displayManager.showMessage("BovinoIOT", "Iniciando...");
    alertManager.initialize();
    alertManager.showSuccess();
}

void initializeDeviceMode() {
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        initializeMasterMode();
    } else {
        initializeSlaveMode();
    }
}

void initializeMasterMode() {
    Serial.println("\n[MAIN] =======================================");
    Serial.println("[MAIN]   DISPOSITIVO MAESTRO");
    Serial.println("[MAIN] =======================================");
    
    wifiManager.begin();
    displayManager.showMessage("WiFi...", "Conectando");
    
    // Verificar si es primera ejecución después de configurar
    Preferences prefsFirstRun;
    bool firstRunAfterConfig = false;
    if (prefsFirstRun.begin("first_run", true)) {
        firstRunAfterConfig = prefsFirstRun.getBool("pending", false);
        prefsFirstRun.end();
    }
    
    if (!wifiManager.connect()) {
        Serial.println("[MAIN]     WiFi no disponible. Esperando configuración...");
        Serial.println("[MAIN] BLE deshabilitado durante portal para liberar memoria");
        
        while (wifiManager.isPortalActive()) {
            wifiManager.loop();
            delay(10);
        }
        
        Serial.println("\n[MAIN] ========================================");
        Serial.println("[MAIN] Configuración WiFi completada");
        Serial.println("[MAIN] ========================================");
        
        // Marcar que hay modo registro pendiente
        if (prefsFirstRun.begin("first_run", false)) {
            prefsFirstRun.putBool("pending", true);
            prefsFirstRun.end();
        }
        
        // Inicializar botón de modo ANTES de entrar al modo registro
        initModeButton();
        
        // Inicializar MQTT
        if (ENABLE_MQTT) {
            mqttClient.initialize();
        }
        
        // Inicializar BLE
        if (!bleScanner.initialize()) {
            Serial.println("[MAIN] Error al inicializar BLE");
        }
        
        // ACTIVAR MODO REGISTRO AUTOMÁTICAMENTE después de configurar
        enterRegistrationMode();
        
    } else if (firstRunAfterConfig) {
        // Ya tenía WiFi configurado, pero es primera ejecución después de configurar
        Serial.println("\n[MAIN] ========================================");
        Serial.println("[MAIN] PRIMERA EJECUCIÓN DESPUÉS DE CONFIGURAR");
        Serial.println("[MAIN] ========================================");
        
        // Limpiar el flag
        if (prefsFirstRun.begin("first_run", false)) {
            prefsFirstRun.putBool("pending", false);
            prefsFirstRun.end();
        }
        
        // Inicializar botón de modo
        initModeButton();
        
        // ACTIVAR MODO REGISTRO AUTOMÁTICAMENTE
        enterRegistrationMode();
    }
    
    if (ENABLE_MQTT && !mqttClient.isConnected()) {
        mqttClient.initialize();
        displayManager.showMessage("MQTT...", "Conectando");
    }
    
    if (!espNowManager.initializeMaster()) {
        Serial.println("[MAIN]   Error al inicializar ESP-NOW");
    }
}

void initializeSlaveMode() {
    Serial.println("\n[MAIN] =======================================");
    Serial.println("[MAIN]   DISPOSITIVO ESCLAVO");
    Serial.println("[MAIN] =======================================");
    
    if (!espNowManager.initializeSlave()) {
        Serial.println("[MAIN]   Error al inicializar ESP-NOW");
        displayManager.showMessage("ERROR", "ESP-NOW");
        alertManager.showError();
        while (1) delay(1000);
    }
}

void initializeBLE() {
    if (!bleScanner.initialize()) {
        Serial.println("[MAIN]  Error al inicializar BLE");
        displayManager.showMessage("ERROR", "BLE fallido");
        alertManager.showError();
        while (1) delay(1000);
    }
}

void finishSetup() {
    systemReady = true;
    displayManager.showMessage("Sistema", "Listo");
    alertManager.showSuccess();
    delay(2000);
    
    Serial.println("\n[MAIN] Sistema inicializado correctamente");
    Serial.println("[MAIN] Iniciando ciclo de escaneo...\n");
}

void processSlaveCycle() {
    static unsigned long cycleNumber = 0;
    cycleNumber++;
    
    Serial.printf("\n[ESCLAVO] ━━━━━ Ciclo Esclavo #%lu ━━━━━\n", cycleNumber);
    Serial.printf("[ESCLAVO] Canal WiFi actual: %d\n", WiFi.channel());
    
    bleScanner.performScan();
    std::map<String, BeaconData> beacons = bleScanner.getBeaconData();
    
    if (beacons.size() > 0) {
        Serial.printf("[ESCLAVO] Beacons detectados: %d\n", beacons.size());
        int sentCount = 0;
        int failCount = 0;
        
        for (const auto& pair : beacons) {
            const BeaconData& beacon = pair.second;
            
            StaticJsonDocument<256> doc;
            String currentDeviceId = LOADED_DEVICE_ID.length() > 0 ? LOADED_DEVICE_ID : LOADED_DEVICE_ID;
            String currentLocation = beacon.detectedLocation;
            
            doc["device_id"] = currentDeviceId;
            doc["device_location"] = currentLocation;
            doc["animal_id"] = beacon.animalId;
            doc["rssi"] = beacon.rssi;
            doc["distance"] = beacon.distance;
            
            String jsonMessage;
            serializeJson(doc, jsonMessage);
            
            bool sent = espNowManager.sendToMaster(jsonMessage);
            if (sent) {
                sentCount++;
                Serial.printf("[ESCLAVO] ✓ Enviado: ID=%u, RSSI=%d, Dist=%.2fm\n",
                             beacon.animalId, beacon.rssi, beacon.distance);
            } else {
                failCount++;
                Serial.printf("[ESCLAVO] ✗ FALLÓ envío: ID=%u\n", beacon.animalId);
            }
            
            // Pequeño delay para garantizar que el mensaje se transmita antes del siguiente
            delay(15);
        }
        
        Serial.printf("[ESCLAVO] Resumen envíos: %d exitosos, %d fallidos\n", sentCount, failCount);
    } else {
        Serial.println("[ESCLAVO] Sin beacons detectados");
    }
    
    bleScanner.clearBeacons();
    displayManager.showMessage("Esclavo", String(beacons.size()) + " vacas");
}

void processMasterCycle() {
    // Log de modo actual (cada 10 ciclos para no saturar)
    static int cycleCount = 0;
    if (cycleCount % 10 == 0) {
        extern bool beaconRegistrationModeActive;
        extern bool isRegistrationModeActive();
        
        bool switchState = isRegistrationModeActive();
        int masterChannel = WiFi.channel();
        Serial.printf("\n[MAESTRO] Estado - Switch: %s, Flag: %s, Canal WiFi: %d\n",
                     switchState ? "REGISTRO" : "NORMAL",
                     beaconRegistrationModeActive ? "REGISTRO" : "NORMAL",
                     masterChannel);
        Serial.printf("[MAESTRO] MAC: %s\n", WiFi.macAddress().c_str());
    }
    cycleCount++;
    
    Serial.println("\n[MAESTRO] ━━━━━ Ciclo Maestro ━━━━━");
    
    // Leer y limpiar mensajes ESP-NOW INMEDIATAMENTE para no perder paquetes
    std::vector<ESPNowMessage> receivedMsgs = espNowManager.getReceivedMessages();
    int msgCount = receivedMsgs.size();
    espNowManager.clearReceivedMessages();  // Limpiar AHORA para capturar nuevos mensajes
    Serial.printf("[MAESTRO] Mensajes de esclavos: %d\n", msgCount);
    
    // DEBUG: Mostrar detalles de cada mensaje recibido
    if (msgCount > 0) {
        Serial.println("[MAESTRO] ━━━ Detalles de mensajes ESP-NOW ━━━");
        for (size_t i = 0; i < receivedMsgs.size(); i++) {
            const ESPNowMessage& msg = receivedMsgs[i];
            Serial.printf("  [%d] ID=%u, Ubicación=%s, RSSI=%d, Dist=%.2fm\n",
                         i + 1, msg.animalId, msg.location, msg.rssi, msg.distance);
        }
    }
    
    bleScanner.performScan();
    std::map<String, BeaconData> localBeacons = bleScanner.getBeaconData();
    Serial.printf("[MAESTRO] Beacons locales: %d\n", localBeacons.size());
    
    std::map<String, BeaconData> allBeacons = localBeacons;
    
    for (const auto& msg : receivedMsgs) {
        BeaconData remoteBeacon;
        remoteBeacon.animalId = msg.animalId;
        remoteBeacon.rssi = msg.rssi;
        remoteBeacon.distance = msg.distance;
        remoteBeacon.detectedLocation = String(msg.location);
        remoteBeacon.macAddress = String(msg.deviceId) + "_remote";
        
        String beaconKey = remoteBeacon.macAddress + "_" + String(msg.animalId);
        allBeacons[beaconKey] = remoteBeacon;
        
        Serial.printf("[MAESTRO] → Remoto: ID=%u, RSSI=%d, Ubicación=%s\n",
                     msg.animalId, msg.rssi, msg.location);
    }
    
    Serial.printf("[MAESTRO] Total beacons: %d\n", allBeacons.size());
    
    if (ENABLE_MQTT && mqttClient.isConnected()) {
        if (allBeacons.size() > 0) {
            bool mqttSuccess = mqttClient.sendDetections(allBeacons);
            if (mqttSuccess) {
                Serial.println("[MAESTRO] Datos enviados a MQTT");
            } else {
                Serial.println("[MAESTRO] Error al enviar MQTT");
            }
        } else {
            Serial.println("[MAESTRO] Sin datos para enviar a MQTT");
        }
    }
    
    bleScanner.clearBeacons();
    displayManager.showMessage("Maestro", String(allBeacons.size()) + " vacas");
}

void processRegistrationCycle() {
    static bool firstTime = true;
    
    if (firstTime) {
        Serial.println("\n[REGISTRO] ==========================================");
        Serial.println("[REGISTRO] MODO: REGISTRO DE BEACONS ACTIVO");
        Serial.println("[REGISTRO] Presiona botón para volver a NORMAL");
        Serial.println("[REGISTRO] ==========================================\n");
        firstTime = false;
    }
    
    // Escanear beacons
    bleScanner.performScan();
    std::map<String, BeaconData> beacons = bleScanner.getBeaconData();
    
    if (!beacons.empty()) {
        std::vector<String> macAddresses;
        Serial.printf("[REGISTRO] Beacons detectados: %d\n", beacons.size());
        
        for (const auto& pair : beacons) {
            macAddresses.push_back(pair.second.macAddress);
        }
        
        // Publicar a MQTT
        extern MQTTClient mqttClient;
        if (mqttClient.isConnected()) {
            DynamicJsonDocument doc(1024);
            doc["zone_id"] = LOADED_ZONE_ID;
            
            JsonArray macsArray = doc.createNestedArray("macs");
            for (const String& mac : macAddresses) {
                macsArray.add(mac);
            }
            
            String payload;
            serializeJson(doc, payload);
            
            Serial.printf("[REGISTRO] Enviando %d MACs al MQTT...\n", macAddresses.size());
            Serial.printf("[REGISTRO] Payload: %s\n", payload.c_str());
            
            mqttClient.publish("bovino_io/register_beacon", payload.c_str());
        }
    } else {
        Serial.println("[REGISTRO] No se detectaron beacons en este ciclo");
    }
    
    bleScanner.clearBeacons();
    displayManager.showMessage("REGISTRO", String(beacons.size()) + " beacons");
    
    // Si salimos del modo registro, resetear flag
    static bool wasInRegistrationMode = true;
    if (!isRegistrationModeActive() && wasInRegistrationMode) {
        Serial.println("\n[REGISTRO] ==========================================");
        Serial.println("[REGISTRO] VOLVIENDO A MODO NORMAL");
        Serial.println("[REGISTRO] ==========================================\n");
        firstTime = true;  // Reset para próxima entrada
    }
    wasInRegistrationMode = isRegistrationModeActive();
}

void handleResetButtonInLoop() {
    if (checkResetButton()) {
        Serial.println("[MAIN] Reset solicitado. Borrando configuración...");
        
        Preferences prefsDevice, prefsLocation, prefsWifi;
        
        prefsDevice.begin("device_cfg", false);
        prefsDevice.clear();
        prefsDevice.end();
        delay(100);
        
        prefsLocation.begin("location_cfg", false);
        prefsLocation.clear();
        prefsLocation.end();
        delay(100);
        
        prefsWifi.begin("wifi_cfg", false);
        prefsWifi.clear();
        prefsWifi.end();
        delay(100);
        
        Serial.println("[MAIN] Configuración borrada. Reiniciando en 2 segundos...");
        displayManager.showMessage("RESET", "Reiniciando");
        alertManager.showWarning();
        delay(2000);
        ESP.restart();
    }
}
