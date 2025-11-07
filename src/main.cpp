#include <Arduino.h>
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

void displayZoneStatus() {
    int animalCount = bleScanner.getAnimalCount();
    
    // Línea 1: Nombre de la zona y conteo
    String line1 = String(DEVICE_LOCATION);
    if (line1.length() > 10) {
        line1 = line1.substring(0, 10);  // Truncar si es muy largo
    }
    line1 += " ";
    line1 += String(animalCount);
    line1 += " 🐄";
    
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
    
    // Mostrar modo de operación
    const char* modeName = (CURRENT_DEVICE_MODE == DEVICE_MASTER) ? "MAESTRO" : "ESCLAVO";
    Serial.printf("[INIT] Modo: %s\n\n", modeName);
    
    // ==================== INICIALIZACIÓN SEGÚN MODO ====================
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        // ============ MODO MAESTRO: WiFi PRIMERO, luego BLE ============
        Serial.println("[INIT] Inicializando como MAESTRO...");
        displayManager.showMessage("Modo: MAESTRO", "Init red...");
        
        // 1. Inicializar ESP-NOW
        if (!espNowManager.initializeMaster()) {
            Serial.println("[INIT] [ERROR] Error al inicializar ESP-NOW");
            alertManager.showError();
        } else {
            Serial.println("[INIT] [OK] ESP-NOW maestro inicializado");
        }
        
        // 2. Inicializar WiFiManager (carga credenciales)
        wifiManager.begin();
        
        // 3. Intentar conectar WiFi (ENABLE_WIFI_SYNC controla si se usa WiFi)
        bool wifiConnected = false;
        if (ENABLE_WIFI_SYNC) {
            Serial.println("[INIT] Conectando WiFi...");
            String ssidLabel = wifiManager.getConfiguredSSID();
            if (ssidLabel.length() == 0) {
                ssidLabel = "Sin config";
            }
            displayManager.showMessage("Conectando WiFi", ssidLabel.c_str());

            // Intentar conectar HASTA 3 VECES
            const int MAX_ATTEMPTS = 3;
            int attempt = 0;
            
            while (!wifiConnected && attempt < MAX_ATTEMPTS) {
                attempt++;
                Serial.printf("[INIT] Intento %d/%d de conexión WiFi...\n", attempt, MAX_ATTEMPTS);
                wifiConnected = wifiManager.connect();
                
                if (wifiConnected) {
                    break;  // Conectado exitosamente
                }
                
                // Si no conectó, esperar un poco antes del siguiente intento
                if (attempt < MAX_ATTEMPTS) {
                    delay(1000);
                }
            }
            
            if (wifiConnected) {
                Serial.printf("[INIT] [OK] WiFi conectado: %s\n", wifiManager.getLocalIP().c_str());
                alertManager.showSuccess();
                delay(1000);
                apiClient.initializeTimeSync();
            } else {
                // Falló después de 3 intentos
                Serial.printf("[INIT] [ERROR] WiFi falló después de %d intentos\n", MAX_ATTEMPTS);
                // Falló después de 3 intentos
                Serial.printf("[INIT] [ERROR] WiFi falló después de %d intentos\n", MAX_ATTEMPTS);
                
                // Si portal está habilitado, activarlo y BLOQUEAR
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
                    
                    // ============ BLOQUEAR AQUÍ HASTA QUE SE CONFIGURE ============
                    Serial.println("[INIT] ⏳ ESPERANDO configuración WiFi...");
                    Serial.println("[INIT] 1. Conéctate a: BovinoIOT-IOT_ZONA_001");
                    Serial.println("[INIT] 2. Password: bovinoiot");
                    Serial.println("[INIT] 3. Abre: http://192.168.4.1");
                    Serial.println("[INIT] 4. Configura tu red WiFi");
                    Serial.println("[INIT] ============================================");
                    
                    unsigned long portalStartTime = millis();
                    const unsigned long PORTAL_TIMEOUT = 300000; // 5 minutos
                    unsigned long lastAnnounce = 0;
                    
                    // LOOP BLOQUEANTE - NO SALE HASTA CONECTAR O TIMEOUT
                    while (!wifiManager.isConnected() && millis() - portalStartTime < PORTAL_TIMEOUT) {
                        wifiManager.loop(); // Procesar peticiones HTTP del portal
                        
                        // Anunciar cada 10 segundos
                        if (millis() - lastAnnounce > 10000) {
                            lastAnnounce = millis();
                            unsigned long elapsed = (millis() - portalStartTime) / 1000;
                            Serial.printf("[INIT] ⏳ Esperando... %lu seg | http://192.168.4.1\n", elapsed);
                            displayManager.showMessage("Esperando WiFi", String(elapsed) + " seg");
                        }
                        
                        delay(100);
                    }
                    
                    // Verificar resultado
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
                    // Portal deshabilitado
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
        
        // 4. AHORA SI, inicializar BLE (después de resolver WiFi)
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
        // ============ MODO ESCLAVO: ESP-NOW + BLE (sin WiFi) ============
        Serial.println("[INIT] Inicializando como ESCLAVO...");
        displayManager.showMessage("Modo: ESCLAVO", "Init...");
        
        // 1. Inicializar ESP-NOW (debe detectar al maestro)
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
        
        // 2. Inicializar BLE
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

    // ==================== 0. MANTENER PORTAL ACTIVO (SOLO MAESTRO) ====================
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        wifiManager.loop(); // Procesar peticiones del portal si está activo
    }
    
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
            Serial.printf("[ALERTA] ⚠️ %d animales no detectados hace 24h\n", missing.size());
            alertManager.showDanger();
            
            // Mostrar alerta en LCD brevemente
            String alertMsg = String(missing.size()) + " animales";
            displayManager.showMessage("ALERTA!", alertMsg.c_str());
            delay(2000);
        }
    }
    
    // ==================== 3. ESP-NOW: ESCLAVO ENVÍA A MAESTRO ====================
    if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
        if (now - lastESPNowSend > ESPNOW_SEND_INTERVAL) {
            lastESPNowSend = now;
            
            std::map<uint32_t, BeaconData> beacons = bleScanner.getBeaconData();
            
            if (beacons.size() > 0) {
                Serial.printf("\n[ESP-NOW] ━━━━━ Enviando a Maestro ━━━━━\n");
                Serial.printf("[ESP-NOW] Animales detectados: %d\n", beacons.size());
                
                displayManager.showMessage("Enviando...", "A maestro");
                
                for (const auto& pair : beacons) {
                    const BeaconData& beacon = pair.second;
                    
                    // Crear JSON para enviar al maestro
                    StaticJsonDocument<256> doc;
                    doc["device_id"] = DEVICE_ID;
                    doc["device_location"] = DEVICE_LOCATION;
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
    
    // ==================== 4. ESP-NOW: MAESTRO PROCESA Y SINCRONIZA ====================
    if (CURRENT_DEVICE_MODE == DEVICE_MASTER) {
        // Procesar mensajes recibidos de esclavos
        std::vector<ESPNowMessage> receivedMsgs = espNowManager.getReceivedMessages();
        
        if (receivedMsgs.size() > 0) {
            Serial.printf("\n[MAESTRO] 📨 Mensajes de esclavos: %d\n", receivedMsgs.size());
            
            for (const auto& msg : receivedMsgs) {
                Serial.printf("[MAESTRO]   Esclavo: %s (%s)\n", msg.deviceId, msg.location);
                Serial.printf("[MAESTRO]   Animal ID=%u, RSSI=%d, Dist=%.2fm\n",
                             msg.animalId, msg.rssi, msg.distance);
            }
            
            // TODO: Agregar datos de esclavos al buffer para enviar al backend
            espNowManager.clearReceivedMessages();
        }
        
        // Sincronizar con backend cada SYNC_INTERVAL
        if (now - lastSyncTime > SYNC_INTERVAL) {
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
            
            if (beacons.size() == 0) {
                Serial.println("[SYNC] No hay datos para sincronizar");
                return;
            }
            
            Serial.printf("\n[SYNC] ━━━━━ Sincronización ━━━━━\n");
            Serial.printf("[SYNC] Zona local: %s (%s)\n", DEVICE_LOCATION, DEVICE_ID);
            Serial.printf("[SYNC] Animales detectados localmente: %d\n", beacons.size());
            
            displayManager.showMessage("Sincronizando", "Espere...");
            alertManager.loaderOn();
            
            for (const auto& pair : beacons) {
                const BeaconData& beacon = pair.second;
                Serial.printf("[SYNC]   🐄 ID=%u, Dist=%.2fm, RSSI=%d dBm\n",
                             beacon.animalId, beacon.distance, beacon.rssi);
            }
            
            // TODO: apiClient.sendZoneData(beacons, behaviors);
            
            delay(500);
            alertManager.loaderOff();
            alertManager.showSuccess();
            displayManager.showMessage("Sincronizado", "OK!");
            delay(1500);
            
            Serial.println("[SYNC] ✓ Datos enviados al backend\n");
        }
    }
    
    // ==================== 4. MANTENIMIENTO ====================
    // Verificar periódicamente conexión WiFi
    static unsigned long lastWifiCheck = 0;
    if (now - lastWifiCheck > 30000) {  // Cada 30 segundos
        lastWifiCheck = now;
        
        if (!wifiManager.isConnected()) {
            Serial.println("[WiFi] Reconectando...");
            wifiManager.reconnect();
        }
    }
    
    // Pequeña pausa para no saturar el CPU
    delay(100);
}