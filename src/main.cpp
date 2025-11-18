#include <Arduino.h>
#include "config.h"
#include "ble_scanner.h"
#include "wifi_manager.h"
#include "api_client.h"
#include "mqtt_client.h"
#include "display_manager.h"
#include "alerts.h"
#include "espnow_manager.h"

unsigned long lastSyncTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastESPNowSend = 0;
bool systemReady = false;

void displayZoneStatus() {
    int animalCount = bleScanner.getAnimalCount();
    
    String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
    String line1 = String(currentLocation);
    if (line1.length() > 10) {
        line1 = line1.substring(0, 10);
    }
    line1 += " ";
    line1 += String(animalCount);
    line1 += " 🐄";
    
    ScanMode mode = bleScanner.getCurrentMode();
    String line2 = "Modo: ";
    if (mode == MODE_ACTIVE) line2 += "ACTIVO ";
    else if (mode == MODE_NORMAL) line2 += "NORMAL ";
    else line2 += "ECO    ";
    
    displayManager.showMessage(line1.c_str(), line2.c_str());
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║     ESP32 - BovinoIOT v2.0               ║");
    Serial.println("║  Sistema de Monitoreo de Ganado         ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();
    
    String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
    String currentDeviceId = LOADED_DEVICE_ID.length() > 0 ? LOADED_DEVICE_ID : DEVICE_ID;
    Serial.printf("Zona: %s\n", currentLocation.c_str());
    Serial.printf("ID: %s\n", currentDeviceId.c_str());
    
    const char* zoneTypeName = "";
    switch (CURRENT_ZONE_TYPE) {
        case ZONE_FEEDER:  zoneTypeName = "Comedero"; break;
        case ZONE_WATERER: zoneTypeName = "Bebedero"; break;
        case ZONE_PASTURE: zoneTypeName = "Pastoreo"; break;
        case ZONE_REST:    zoneTypeName = "Descanso"; break;
        default:           zoneTypeName = "Genérica"; break;
    }
    Serial.printf("Tipo: %s\n\n", zoneTypeName);
    
    alertManager.initialize();
    alertManager.loaderOn();
    
    initResetButton();
    
    displayManager.initialize();
    displayManager.showMessage("BovinoIOT v2.0", "Iniciando...");
    delay(2000);
    
    wifiManager.begin();
    
    if (!wifiManager.isConfigured()) {
        Serial.println("[INIT] ============================================");
        Serial.println("[INIT] ⚠️  DISPOSITIVO NO CONFIGURADO");
        Serial.println("[INIT] Abriendo portal de configuración...");
        Serial.println("[INIT] ============================================");
        
        displayManager.showMessage("Sin Config", "Abriendo portal");
        delay(1000);
        
        wifiManager.startConfigPortal();
        
        if (!wifiManager.isPortalActive()) {
            Serial.println("[INIT] [ERROR] No se pudo activar el portal");
            displayManager.showMessage("Error portal", "Reinicie ESP32");
            while(1) { alertManager.showDanger(); delay(1000); }
        }
        
        displayManager.showMessage("Portal Config", "192.168.4.1");
        Serial.println("[INIT] ⏳ ESPERANDO configuración inicial...");
        Serial.println("[INIT] 1. Conéctate a: BovinoIOT-" + String(DEVICE_ID));
        Serial.println("[INIT] 2. Password: bovinoiot");
        Serial.println("[INIT] 3. Abre: http://192.168.4.1");
        Serial.println("[INIT] 4. Configura como MAESTRO o ESCLAVO");
        Serial.println("[INIT] ============================================");
        
        while (true) {
            wifiManager.loop();
            delay(100);
        }
    }
    
    Serial.println("[INIT] ✓ Dispositivo configurado previamente");
    
    const char* modeName = (CURRENT_DEVICE_MODE == DEVICE_MASTER) ? "MAESTRO" : "ESCLAVO";
    Serial.printf("[INIT] Modo: %s\n\n", modeName);
    
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        Serial.println("[INIT] Inicializando como MAESTRO...");
        displayManager.showMessage("Modo: MAESTRO", "Init red...");
        
        if (!espNowManager.initializeMaster()) {
            Serial.println("[INIT] [ERROR] Error al inicializar ESP-NOW");
            alertManager.showError();
        } else {
            Serial.println("[INIT] [OK] ESP-NOW maestro inicializado");
        }
        
        bool wifiConnected = false;
        if (ENABLE_WIFI_SYNC) {
            Serial.println("[INIT] Conectando WiFi...");
            String ssidLabel = wifiManager.getConfiguredSSID();
            if (ssidLabel.length() == 0) {
                ssidLabel = "Sin config";
            }
            displayManager.showMessage("Conectando WiFi", ssidLabel.c_str());

            const int MAX_ATTEMPTS = 3;
            int attempt = 0;
            
            while (!wifiConnected && attempt < MAX_ATTEMPTS) {
                attempt++;
                Serial.printf("[INIT] Intento %d/%d de conexión WiFi...\n", attempt, MAX_ATTEMPTS);
                wifiConnected = wifiManager.connect();
                
                if (wifiConnected) {
                    break;
                }
                
                if (attempt < MAX_ATTEMPTS) {
                    delay(1000);
                }
            }
            
            if (wifiConnected) {
                Serial.printf("[INIT] [OK] WiFi conectado: %s\n", wifiManager.getLocalIP().c_str());
                alertManager.showSuccess();
                delay(1000);
                apiClient.initializeTimeSync();
                
                if (ENABLE_MQTT) {
                    Serial.println("[INIT] Inicializando cliente MQTT...");
                    if (mqttClient.initialize()) {
                        Serial.println("[INIT] [OK] MQTT inicializado correctamente");
                    } else {
                        Serial.println("[INIT] [WARNING] MQTT no pudo inicializarse");
                    }
                }
                
                if (wifiManager.isPortalActive()) {
                    Serial.println("[INIT] Cerrando portal de configuración para liberar memoria...");
                    wifiManager.stopConfigPortal();
                    delay(500);
                    Serial.printf("[INIT] Memoria libre después de cerrar portal: %d bytes\n", ESP.getFreeHeap());
                }
            } else {
                Serial.printf("[INIT] [ERROR] WiFi falló después de %d intentos\n", MAX_ATTEMPTS);
                
                if (ENABLE_WIFI_PORTAL) {
                    Serial.println("[INIT] ============================================");
                    Serial.println("[INIT] ACTIVANDO PORTAL DE CONFIGURACIÓN");
                    Serial.println("[INIT] NO SE PUEDE CONECTAR A LA RED CONOCIDA");
                    Serial.println("[INIT] ============================================");
                    
                    wifiManager.startConfigPortal();
                    
                    if (!wifiManager.isPortalActive()) {
                        Serial.println("[INIT] [ERROR] No se pudo activar el portal");
                        displayManager.showMessage("Error portal", "Reinicie ESP32");
                        while(1) { alertManager.showDanger(); delay(1000); }
                    }
                    
                    displayManager.showMessage("Portal WiFi", "192.168.4.1");
                    alertManager.showError();
                    
                    Serial.println("[INIT] ⏳ ESPERANDO configuración WiFi...");
                    Serial.println("[INIT] 1. Conéctate a: BovinoIOT-IOT_ZONA_001");
                    Serial.println("[INIT] 2. Password: bovinoiot");
                    Serial.println("[INIT] 3. Abre: http://192.168.4.1");
                    Serial.println("[INIT] 4. Configura tu red WiFi");
                    Serial.println("[INIT] ============================================");
                    
                    unsigned long portalStartTime = millis();
                    const unsigned long PORTAL_TIMEOUT = 300000;
                    unsigned long lastAnnounce = 0;
                    
                    while (!wifiManager.isConnected() && millis() - portalStartTime < PORTAL_TIMEOUT) {
                        wifiManager.loop();
                        
                        if (millis() - lastAnnounce > 10000) {
                            lastAnnounce = millis();
                            unsigned long elapsed = (millis() - portalStartTime) / 1000;
                            Serial.printf("[INIT] ⏳ Esperando... %lu seg | http://192.168.4.1\n", elapsed);
                            displayManager.showMessage("Esperando WiFi", String(elapsed) + " seg");
                        }
                        
                        delay(100);
                    }
                    
                    if (wifiManager.isConnected()) {
                        Serial.println("[INIT] ============================================");
                        Serial.println("[INIT] ✅ WiFi CONFIGURADO Y CONECTADO!");
                        Serial.printf("[INIT] IP: %s\n", wifiManager.getLocalIP().c_str());
                        Serial.println("[INIT] ============================================");
                        wifiConnected = true;
                        wifiManager.stopConfigPortal();
                        alertManager.showSuccess();
                        apiClient.initializeTimeSync();
                        delay(2000);
                    } else {
                        Serial.println("[INIT] ============================================");
                        Serial.println("[INIT] ❌ TIMEOUT - No se configuró WiFi en 5 minutos");
                        Serial.println("[INIT] ❌ El ESP32 se reiniciará");
                        Serial.println("[INIT] ============================================");
                        displayManager.showMessage("WiFi timeout", "Reinicie ESP32");
                        alertManager.showError();
                        while (1) {
                            alertManager.showDanger();
                            delay(1000);
                        }
                    }
                } else {
                    Serial.println("[INIT] [WARNING] Portal deshabilitado - Sin WiFi");
                    displayManager.showMessage("Sin WiFi", "Portal OFF");
                    delay(2000);
                }
            }
        } else {
            Serial.println("[INIT] WiFi deshabilitado (ENABLE_WIFI_SYNC = false)");
            displayManager.showMessage("WiFi OFF", "Modo prueba");
            delay(2000);
        }
        
        Serial.println("[INIT] Red resuelta. Inicializando BLE...");
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
        
        Serial.println("[INIT] [OK] BLE inicializado correctamente");
        
    } else {
        Serial.println("[INIT] Inicializando como ESCLAVO...");
        displayManager.showMessage("Modo: ESCLAVO", "Init...");
        
        if (!espNowManager.initializeSlave()) {
            Serial.println("[INIT] [ERROR] Error al inicializar ESP-NOW");
            displayManager.showMessage("ERROR ESP-NOW", "Verificar MAC");
            alertManager.showError();
            while (1) {
                alertManager.showDanger();
                delay(1000);
            }
        }
        
        Serial.println("[INIT] [OK] ESP-NOW esclavo inicializado");
        delay(1000);
        
        Serial.println("[INIT] Inicializando BLE...");
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
        
        Serial.println("[INIT] [OK] BLE inicializado correctamente");
        Serial.println("[INIT] [OK] Esclavo listo para enviar a maestro");
    }
    
    alertManager.loaderOff();
    alertManager.showSuccess();
    
    displayManager.showMessage(DEVICE_LOCATION, "Sistema listo!");
    delay(2000);
    
    systemReady = true;
    lastSyncTime = millis();
    lastDisplayUpdate = millis();
    lastESPNowSend = millis();
    
    Serial.println("\n[INIT] ✅ Sistema inicializado - Comenzando monitoreo\n");
}

void loop() {
    if (!systemReady) {
        delay(100);
        return;
    }
    
    unsigned long now = millis();

    if (checkResetButton()) {
        Serial.println("\n[RESET] ╔════════════════════════════════════════╗");
        Serial.println("[RESET] ║  🔴 RESETEO DE CONFIGURACIÓN        ║");
        Serial.println("[RESET] ╚════════════════════════════════════════╝");
        
        displayManager.showMessage("RESETEANDO", "Espere...");
        alertManager.showDanger();
        
        wifiManager.clearAllConfig();
        
        Serial.println("[RESET] ✓ Configuración borrada");
        Serial.println("[RESET] 🔄 Reiniciando ESP32...");
        
        displayManager.showMessage("Config borrada", "Reiniciando...");
        delay(2000);
        
        ESP.restart();
    }
    
    bleScanner.performScan();
    
    if (ENABLE_MQTT && CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        mqttClient.loop();
    }
    
    if (now - lastDisplayUpdate > 3000) {
        displayZoneStatus();
        lastDisplayUpdate = now;
        
        std::vector<uint32_t> missing = bleScanner.getMissingAnimals();
        if (missing.size() > 0) {
            Serial.printf("[ALERTA] ⚠️ %d animales no detectados hace 24h\n", missing.size());
            alertManager.showDanger();
            
            String alertMsg = String(missing.size()) + " animales";
            displayManager.showMessage("ALERTA!", alertMsg.c_str());
            delay(2000);
        }
    }
    
    if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
        if (now - lastESPNowSend > ESPNOW_SEND_INTERVAL) {
            lastESPNowSend = now;
            
            std::map<String, BeaconData> beacons = bleScanner.getBeaconData();
            
            if (beacons.size() > 0) {
                Serial.printf("\n[ESP-NOW] ━━━━━ Enviando a Maestro ━━━━━\n");
                Serial.printf("[ESP-NOW] Animales detectados: %d\n", beacons.size());
                
                displayManager.showMessage("Enviando...", "A maestro");
                
                for (const auto& pair : beacons) {
                    const BeaconData& beacon = pair.second;
                    
                    StaticJsonDocument<256> doc;
                    String currentDeviceId = LOADED_DEVICE_ID.length() > 0 ? LOADED_DEVICE_ID : DEVICE_ID;
                    String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
                    doc["device_id"] = currentDeviceId;
                    doc["device_location"] = currentLocation;
                    doc["zone_type"] = CURRENT_ZONE_TYPE;
                    doc["animal_id"] = beacon.animalId;
                    doc["rssi"] = beacon.rssi;
                    doc["distance"] = beacon.distance;
                    doc["is_present"] = beacon.isPresent;
                    
                    String jsonMsg;
                    serializeJson(doc, jsonMsg);
                    
                    if (espNowManager.sendToMaster(jsonMsg)) {
                        alertManager.flashLED(LED_SUCCESS, 1, 50);
                    } else {
                        alertManager.flashLED(LED_ERROR, 1, 50);
                    }
                    
                    delay(50);
                }
                
                Serial.println("[ESP-NOW] ✓ Datos enviados al maestro\n");
                displayManager.showMessage("Enviado OK", "");
                delay(1000);
            }
        }
    }
    
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        std::vector<ESPNowMessage> receivedMsgs = espNowManager.getReceivedMessages();
        
        if (receivedMsgs.size() > 0) {
            Serial.printf("\n[MAESTRO] 📨 Mensajes de esclavos: %d\n", receivedMsgs.size());
            
            for (const auto& msg : receivedMsgs) {
                Serial.printf("[MAESTRO]   Esclavo: %s (%s)\n", msg.deviceId, msg.location);
                Serial.printf("[MAESTRO]   Animal ID=%u, RSSI=%d, Dist=%.2fm\n",
                             msg.animalId, msg.rssi, msg.distance);
            }
            
            espNowManager.clearReceivedMessages();
        }
        
        if (now - lastSyncTime > SYNC_INTERVAL) {
            lastSyncTime = now;
            
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
            
            std::map<String, BeaconData> beacons = bleScanner.getBeaconData();
            
            if (beacons.size() == 0) {
                Serial.println("[SYNC] No hay datos para sincronizar");
                return;
            }
            
            Serial.printf("\n[SYNC] ━━━━━ Sincronización ━━━━━\n");
            String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
            String currentDeviceId = LOADED_DEVICE_ID.length() > 0 ? LOADED_DEVICE_ID : DEVICE_ID;
            Serial.printf("[SYNC] Zona local: %s (%s)\n", currentLocation.c_str(), currentDeviceId.c_str());
            Serial.printf("[SYNC] Animales detectados localmente: %d\n", beacons.size());
            
            displayManager.showMessage("Sincronizando", "Espere...");
            alertManager.loaderOn();
            
            for (const auto& pair : beacons) {
                const BeaconData& beacon = pair.second;
                Serial.printf("[SYNC]   🐄 ID=%u, Dist=%.2fm, RSSI=%d dBm\n",
                             beacon.animalId, beacon.distance, beacon.rssi);
            }
            
            bool mqttSuccess = false;
            
            if (ENABLE_MQTT && mqttClient.isConnected()) {
                Serial.println("[SYNC] Enviando por MQTT...");
                mqttSuccess = mqttClient.sendDetections(beacons);
            } else {
                Serial.println("[SYNC] ⚠️ MQTT no disponible");
            }
            
            delay(500);
            alertManager.loaderOff();
            
            if (mqttSuccess) {
                alertManager.showSuccess();
                displayManager.showMessage("Enviado MQTT", "OK!");
            } else {
                alertManager.showError();
                displayManager.showMessage("Error MQTT", "Reintentando...");
            }
            
            delay(1500);
            
            Serial.println("[SYNC] ✓ Ciclo de sincronización completado\n");
        }
    }
    
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        static unsigned long lastWifiCheck = 0;
        if (now - lastWifiCheck > 30000) {
            lastWifiCheck = now;
            
            if (!wifiManager.isConnected()) {
                Serial.println("[WiFi] Reconectando...");
                wifiManager.reconnect();
            }
        }
    }
    
    delay(100);
}
