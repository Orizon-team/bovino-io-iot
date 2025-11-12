#include "wifi_manager.h"
#include "alerts.h"
#include "display_manager.h"
#include <Preferences.h>
#include <cstring>

// Definición de la instancia global
WiFiManager wifiManager;

// Sublocalizaciones hardcode (temporalmente)
const String SUB_LOCATIONS[] = {
    "Bebedero Norte",
    "Bebedero Sur",
    "Comedero Este",
    "Comedero Oeste",
    "Área de Descanso",
    "Zona de Pastoreo"
};
const int SUB_LOCATIONS_COUNT = 6;

// ==================== Constructor ====================
WiFiManager::WiFiManager()
        : configServer(80),
            lastConnectionAttempt(0),
            wasConnected(false),
            portalActive(false),
            pendingReconnect(false),
            lastPortalAnnounce(0),
            reconnectRequestTime(0),
            connectionAttempts(0) {
}

// ==================== Inicialización ====================
void WiFiManager::begin() {
    loadStoredCredentials();
    loadDeviceConfig();  // Cargar configuración del dispositivo (modo y MAC)
    
    // NO iniciar portal automáticamente; solo si falla conexión
}

bool WiFiManager::connect() {
    if (currentSSID.length() == 0) {
        Serial.println("[WiFi] SSID vacio. Configure credenciales desde el portal web.");
        if (ENABLE_WIFI_PORTAL && !portalActive) {  
            startConfigPortal();
        }
        return false;
    }

    Serial.println("\n[WiFi] Iniciando conexion WiFi...");
    Serial.printf("[WiFi] SSID: %s\n", currentSSID.c_str());

    displayManager.showWiFiStatus(true, currentSSID.c_str());

    WiFi.disconnect(false);
    delay(100);

    WiFi.mode(WIFI_STA);
    WiFi.begin(currentSSID.c_str(), currentPassword.c_str());

    bool connected = attemptConnection(WIFI_TIMEOUT);

    if (connected) {
        Serial.println("\n[WiFi] [OK] Conexion exitosa");
        Serial.printf("[WiFi] IP Local: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());

        displayManager.showIP(getLocalIP());
        alertManager.showSuccess();
        delay(2000);

        wasConnected = true;
        
        // Detener portal si estaba activo
        if (portalActive) {
            stopConfigPortal();
        }
        
        return true;
    }

    Serial.println("\n[WiFi] [ERROR] No se pudo conectar");
    displayManager.showWiFiError();
    alertManager.showError();
    delay(2000);

    wasConnected = false;

    return false;
}

bool WiFiManager::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool WiFiManager::reconnect() {
    // NO reconectar si el portal está activo
    if (portalActive) {
        Serial.println("[WiFi] Portal activo - No se intentara reconectar");
        return false;
    }
    
    Serial.println("[WiFi] Intentando reconexion...");
    return connect();
}

// ==================== Información de Red ====================
String WiFiManager::getLocalIP() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int WiFiManager::getRSSI() {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

// ==================== Desconexión ====================
void WiFiManager::disconnect() {
    Serial.println("[WiFi] Desconectando...");
    WiFi.disconnect(false);
    wasConnected = false;
}

void WiFiManager::stopConfigPortal() {
    if (!portalActive) {
        return;
    }
    
    Serial.println("[Portal] Deteniendo portal de configuracion...");
    
    // Detener DNS Server
    dnsServer.stop();
    
    // Detener Web Server
    configServer.stop();
    
    // Desconectar AP
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    
    portalActive = false;
    connectionAttempts = 0;  // Resetear intentos cuando se detiene el portal
    
    Serial.println("[Portal] Portal detenido. Recursos liberados.");
}

// ==================== Loop del Portal ====================
void WiFiManager::loop() {
    if (portalActive) {
        dnsServer.processNextRequest();  // Procesar peticiones DNS (Captive Portal)
        configServer.handleClient();     // Procesar peticiones HTTP

        // Anunciar el portal periódicamente (cada 30 segundos)
        if (lastPortalAnnounce == 0 || millis() - lastPortalAnnounce > 30000) {
            lastPortalAnnounce = millis();
            Serial.println("[Portal] ==========================================");
            Serial.printf("[Portal] URL: http://%s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("[Portal] Clientes conectados: %d\n", WiFi.softAPgetStationNum());
            Serial.println("[Portal] DNS activo: Redirigiendo todas las URLs");
            Serial.println("[Portal] Acceso manual: http://192.168.4.1");
            Serial.println("[Portal] ==========================================");
        }
    }

    if (pendingReconnect && millis() - reconnectRequestTime > 500) {
        pendingReconnect = false;
        
        if (ENABLE_WIFI_SYNC) {
            Serial.println("[Portal] Intentando conectar con nuevas credenciales...");
            
            // Detener portal antes de reconectar
            stopConfigPortal();
            delay(1000);  // Esperar a que el portal se detenga completamente
            
            // Resetear contador de intentos para permitir reconexión
            connectionAttempts = 0;
            
            // Intentar conectar
            if (connect()) {
                Serial.println("[Portal] [OK] Conectado con nuevas credenciales!");
            } else {
                Serial.println("[Portal] [ERROR] No se pudo conectar con nuevas credenciales");
            }
        } else {
            Serial.println("[Portal] Credenciales guardadas. WiFi deshabilitado (modo prueba).");
        }
    }
}

// ==================== Métodos Privados ====================
void WiFiManager::loadStoredCredentials() {
    // Si es ESCLAVO, no necesita WiFi
    if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
        currentSSID = "";
        currentPassword = "";
        Serial.println("[WiFi] Modo ESCLAVO - WiFi no requerido");
        return;
    }
    
    currentSSID = String(WIFI_SSID);
    currentPassword = String(WIFI_PASSWORD);

    Preferences prefs;
    if (prefs.begin("wifi_cfg", true)) {
        String storedSsid = prefs.getString("ssid", "");
        String storedPass = prefs.getString("pass", "");
        prefs.end();

        if (storedSsid.length() > 0) {
            currentSSID = storedSsid;
            currentPassword = storedPass;
            Serial.printf("[WiFi] Usando SSID guardado: %s\n", currentSSID.c_str());
        } else {
            Serial.println("[WiFi] Sin credenciales guardadas. Se usaran las de compilacion.");
        }
    } else {
        Serial.println("[WiFi] No se pudo abrir almacenamiento de preferencias.");
    }
}

void WiFiManager::saveCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    if (prefs.begin("wifi_cfg", false)) {
        prefs.putString("ssid", ssid);
        prefs.putString("pass", password);
        prefs.end();
    }

    currentSSID = ssid;
    currentPassword = password;
}

// ==================== Configuración del Dispositivo ====================
void WiFiManager::loadDeviceConfig() {
    Preferences prefs;
    if (prefs.begin("device_cfg", true)) {
        // Cargar modo del dispositivo (0=SLAVE por defecto, 1=MASTER)
        uint8_t mode = prefs.getUChar("mode", 0);
        CURRENT_DEVICE_MODE = (mode == 1) ? DEVICE_MASTER : DEVICE_SLAVE;
        
        // Cargar MAC del maestro si es esclavo
        if (CURRENT_DEVICE_MODE == DEVICE_SLAVE) {
            String macStr = prefs.getString("master_mac", "");
            if (macStr.length() > 0) {
                stringToMac(macStr, MASTER_MAC_ADDRESS);
                Serial.printf("[Config] Modo ESCLAVO - MAC Maestro: %s\n", macStr.c_str());
            } else {
                Serial.println("[Config] Modo ESCLAVO - MAC Maestro no configurada");
            }
        } else {
            Serial.println("[Config] Modo MAESTRO");
        }
        
        prefs.end();
    }
}

void WiFiManager::saveDeviceConfig(DeviceMode mode, const String& masterMac) {
    Preferences prefs;
    if (prefs.begin("device_cfg", false)) {
        // Guardar modo (0=SLAVE, 1=MASTER)
        prefs.putUChar("mode", (mode == DEVICE_MASTER) ? 1 : 0);
        
        // Guardar MAC del maestro solo si es esclavo
        if (mode == DEVICE_SLAVE && masterMac.length() > 0) {
            prefs.putString("master_mac", masterMac);
            stringToMac(masterMac, MASTER_MAC_ADDRESS);
        }
        
        prefs.end();
        Serial.printf("[Config] Configuración guardada - Modo: %s\n", 
                     (mode == DEVICE_MASTER) ? "MAESTRO" : "ESCLAVO");
    }
    
    CURRENT_DEVICE_MODE = mode;
}

String WiFiManager::macToString(const uint8_t* mac) {
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

void WiFiManager::stringToMac(const String& macStr, uint8_t* mac) {
    int values[6];
    if (sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) {
            mac[i] = (uint8_t)values[i];
        }
    }
}

// ==================== Verificación de Configuración ====================
bool WiFiManager::isConfigured() {
    Preferences prefs;
    bool configured = false;
    
    if (prefs.begin("device_cfg", true)) {
        uint8_t mode = prefs.getUChar("mode", 0);
        
        if (mode == 1) {
            // MAESTRO: verificar que tenga WiFi configurado
            prefs.end();
            if (prefs.begin("wifi_cfg", true)) {
                String ssid = prefs.getString("ssid", "");
                configured = (ssid.length() > 0);
                prefs.end();
            }
        } else {
            // ESCLAVO: verificar que tenga MAC del maestro
            String macStr = prefs.getString("master_mac", "");
            configured = (macStr.length() >= 17);
            prefs.end();
        }
    }
    
    return configured;
}

void WiFiManager::clearAllConfig() {
    Serial.println("[Config] Limpiando TODA la configuración...");
    
    Preferences prefs;
    
    // Limpiar configuración WiFi
    if (prefs.begin("wifi_cfg", false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Config] ✓ WiFi limpiado");
    }
    
    // Limpiar configuración del dispositivo
    if (prefs.begin("device_cfg", false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Config] ✓ Dispositivo limpiado");
    }
    
    // Limpiar configuración de ubicación
    if (prefs.begin("location_cfg", false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Config] ✓ Ubicación limpiada");
    }
    
    // Resetear variables
    currentSSID = "";
    currentPassword = "";
    CURRENT_DEVICE_MODE = DEVICE_SLAVE;  // Por defecto ESCLAVO
    
    Serial.println("[Config] ✓ Configuración limpiada completamente");
}

// ==================== GraphQL - Obtener Zonas ====================
String WiFiManager::fetchZonesFromGraphQL(int userId) {
    Serial.printf("[GraphQL] Obteniendo zonas del usuario %d...\n", userId);
    
    HTTPClient http;
    http.setTimeout(10000);
    
    // URL de GraphQL
    const char* graphqlUrl = "https://bovino-io-backend.onrender.com/graphql";
    
    if (!http.begin(graphqlUrl)) {
        Serial.println("[GraphQL] Error: No se pudo iniciar conexión HTTPS");
        return "[]";
    }
    
    // Preparar query GraphQL
    String query = "{\"query\":\"query($userId:Int!){ zonesByUser(userId:$userId){ id name user{ id_user name } } }\",\"variables\":{\"userId\":" + String(userId) + "}}";
    
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(query);
    
    String result = "[]";
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[GraphQL] Respuesta HTTP: %d\n", httpCode);
        
        if (httpCode == 200) {
            // Parsear respuesta GraphQL
            const size_t capacity = JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(50);
            DynamicJsonDocument doc(capacity);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                // Extraer zonas del response
                if (doc.containsKey("data")) {
                    JsonObject dataObj = doc["data"];
                    if (dataObj.containsKey("zonesByUser")) {
                        JsonArray zones = dataObj["zonesByUser"];
                        
                        // Construir resultado como array JSON
                        DynamicJsonDocument resultDoc(JSON_ARRAY_SIZE(10) + JSON_OBJECT_SIZE(20));
                        JsonArray resultArray = resultDoc.createNestedArray();
                        
                        int zoneCount = 0;
                        for (JsonVariant item : zones) {
                            if (item.is<JsonObject>()) {
                                JsonObject zoneObj = resultArray.createNestedObject();
                                if (item.containsKey("id")) {
                                    zoneObj["id"] = item["id"];
                                }
                                if (item.containsKey("name")) {
                                    zoneObj["name"] = item["name"];
                                }
                                zoneCount++;
                            }
                        }
                        
                        serializeJson(resultArray, result);
                        Serial.printf("[GraphQL] ✓ Zonas obtenidas: %d\n", zoneCount);
                    } else {
                        Serial.println("[GraphQL] Error: Campo 'zonesByUser' no encontrado");
                    }
                } else {
                    Serial.println("[GraphQL] Error: Campo 'data' no encontrado");
                }
            } else {
                Serial.printf("[GraphQL] Error JSON: %s\n", error.c_str());
            }
        } else {
            Serial.printf("[GraphQL] Error HTTP: %d\n", httpCode);
        }
    } else {
        Serial.printf("[GraphQL] Error de conexión: %d\n", httpCode);
    }
    
    http.end();
    return result;
}

String WiFiManager::saveDeviceLocation(const String& zoneName, const String& subLocation) {
    Preferences prefs;
    if (prefs.begin("location_cfg", false)) {
        prefs.putString("zone_name", zoneName);
        prefs.putString("sub_location", subLocation);
        prefs.end();
        Serial.printf("[Config] Ubicación guardada - Zona: %s, Sublocalización: %s\n", 
                     zoneName.c_str(), subLocation.c_str());
        return "OK";
    }
    return "ERROR";
}

// ==================== Portal de Configuración ====================
void WiFiManager::startConfigPortal() {
    if (portalActive) {
        Serial.println("[Portal] Portal ya activo. Ignorando solicitud.");
        return;
    }

    Serial.println("[Portal] ========================================");
    Serial.println("[Portal] Iniciando portal de configuracion...");
    
    // Detener COMPLETAMENTE cualquier actividad WiFi previa
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);
    
    String apSsid = String("BovinoIOT-") + DEVICE_ID;

    // Configurar modo AP puro (sin STA)
    WiFi.mode(WIFI_AP);
    delay(500);

    bool apStarted;
    if (CONFIG_PORTAL_PASSWORD != nullptr && strlen(CONFIG_PORTAL_PASSWORD) >= 8) {
        apStarted = WiFi.softAP(apSsid.c_str(), CONFIG_PORTAL_PASSWORD);
    } else {
        apStarted = WiFi.softAP(apSsid.c_str());
    }

    if (!apStarted) {
        Serial.println("[Portal] [ERROR] No se pudo iniciar el portal de configuracion");
        Serial.println("[Portal] ========================================");
        return;
    }

    // Esperar a que el AP esté completamente activo
    delay(1000);
    
    // Configurar IP estática para el AP
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    delay(500);

    // Iniciar servidor DNS para Captive Portal
    // Redirige TODAS las peticiones DNS a la IP del ESP32
    const byte DNS_PORT = 53;
    bool dnsStarted = dnsServer.start(DNS_PORT, "*", local_IP);
    if (dnsStarted) {
        Serial.printf("[Portal] DNS Server iniciado en puerto %d (Captive Portal activo)\n", DNS_PORT);
        Serial.printf("[Portal] Redirigiendo todas las DNS queries a %s\n", local_IP.toString().c_str());
    } else {
        Serial.println("[Portal] [ADVERTENCIA] No se pudo iniciar DNS Server");
    }
    
    delay(100);

    // Configurar rutas del servidor
    setupPortalRoutes();
    
    // Iniciar servidor web
    configServer.begin();
    portalActive = true;
    lastPortalAnnounce = 0;

    Serial.println("[Portal] =================================");
    Serial.printf("[Portal] SSID: %s\n", apSsid.c_str());
    Serial.printf("[Portal] URL: http://%s\n", WiFi.softAPIP().toString().c_str());
    if (CONFIG_PORTAL_PASSWORD != nullptr && strlen(CONFIG_PORTAL_PASSWORD) >= 8) {
        Serial.printf("[Portal] Password: %s\n", CONFIG_PORTAL_PASSWORD);
    } else {
        Serial.println("[Portal] Red abierta (sin password)");
    }
    Serial.println("[Portal] Captive Portal: Redirección automática ACTIVA");
    Serial.println("[Portal] Portal web iniciado correctamente");
    Serial.println("[Portal] =================================");
}

void WiFiManager::setupPortalRoutes() {
    // Ruta principal
    configServer.on("/", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET / desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/html", renderPortalPage(""));
    });

    // Ruta para obtener zonas desde GraphQL
    configServer.on("/getZones", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET /getZones desde %s\n", clientIP.toString().c_str());
        
        String zonesJson = fetchZonesFromGraphQL(3);
        configServer.send(200, "application/json", "{\"zones\":" + zonesJson + "}");
    });

    // Ruta para guardar configuración
    configServer.on("/save", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /save desde %s\n", clientIP.toString().c_str());
        
        String deviceMode = configServer.arg("device_mode");
        String ssid = configServer.arg("ssid");
        String password = configServer.arg("password");
        String masterMac = configServer.arg("master_mac");
        String zoneName = configServer.arg("zone_name");
        String subLocation = configServer.arg("sub_location");

        deviceMode.trim();
        ssid.trim();
        password.trim();
        masterMac.trim();
        zoneName.trim();
        subLocation.trim();

        // Validar zona y sublocalización (requeridas para TODOS)
        if (zoneName.length() == 0) {
            Serial.println("[Portal] Error: Zona requerida");
            configServer.send(200, "text/html", renderPortalPage("Debes seleccionar una Zona."));
            return;
        }
        
        if (subLocation.length() == 0) {
            Serial.println("[Portal] Error: Sublocalización requerida");
            configServer.send(200, "text/html", renderPortalPage("Debes seleccionar una Sublocalización."));
            return;
        }

        // Determinar el modo del dispositivo
        DeviceMode mode = (deviceMode == "master") ? DEVICE_MASTER : DEVICE_SLAVE;
        
        // Validación según el modo
        if (mode == DEVICE_MASTER) {
            // MAESTRO: requiere WiFi
            if (ssid.length() == 0) {
                Serial.println("[Portal] Error: MAESTRO requiere SSID");
                configServer.send(200, "text/html", renderPortalPage("El modo MAESTRO requiere configurar WiFi."));
                return;
            }
            
            Serial.printf("[Portal] Configurando MAESTRO\n");
            Serial.printf("[Portal] WiFi SSID: %s\n", ssid.c_str());
            Serial.printf("[Portal] Zona: %s, Sublocalización: %s\n", zoneName.c_str(), subLocation.c_str());
            
            saveCredentials(ssid, password);
            saveDeviceConfig(mode, "");  // MAESTRO no necesita MAC
            saveDeviceLocation(zoneName, subLocation);  // Guardar ubicación
            
            String message = "✓ MAESTRO configurado<br>";
            message += "WiFi: " + ssid + "<br>";
            message += "Zona: " + zoneName + "<br>";
            message += "El dispositivo se reiniciara...";
            
            configServer.send(200, "text/html", renderPortalPage(message));
            
            delay(2000);
            ESP.restart();  // Reiniciar para aplicar configuración
            
        } else {
            // ESCLAVO: solo requiere MAC del maestro
            if (masterMac.length() == 0 || masterMac.length() < 17) {
                Serial.println("[Portal] Error: ESCLAVO requiere MAC del maestro");
                configServer.send(200, "text/html", renderPortalPage("El modo ESCLAVO requiere la MAC del Maestro."));
                return;
            }
            
            Serial.printf("[Portal] Configurando ESCLAVO\n");
            Serial.printf("[Portal] MAC Maestro: %s\n", masterMac.c_str());
            Serial.printf("[Portal] Zona: %s, Sublocalización: %s\n", zoneName.c_str(), subLocation.c_str());
            
            saveDeviceConfig(mode, masterMac);
            saveDeviceLocation(zoneName, subLocation);  // Guardar ubicación
            
            String message = "✓ ESCLAVO configurado<br>";
            message += "MAC Maestro: " + masterMac + "<br>";
            message += "Zona: " + zoneName + "<br>";
            message += "El dispositivo se reiniciara...";
            
            configServer.send(200, "text/html", renderPortalPage(message));
            
            delay(2000);
            ESP.restart();  // Reiniciar para aplicar configuración
        }
    });
    
    // Captive Portal - Redirigir cualquier petición a la página principal
    configServer.on("/generate_204", HTTP_GET, [this]() {  // Android
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Android) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Location", "http://192.168.4.1", true);
        configServer.send(302, "text/plain", "");
    });
    
    configServer.on("/hotspot-detect.html", HTTP_GET, [this]() {  // iOS
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (iOS) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Location", "http://192.168.4.1", true);
        configServer.send(302, "text/plain", "");
    });
    
    configServer.on("/connecttest.txt", HTTP_GET, [this]() {  // Windows
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Windows) desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/plain", "Microsoft Connect Test");
    });

    // Manejar cualquier ruta no encontrada
    configServer.onNotFound([this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Ruta no encontrada desde %s: %s - Redirigiendo a /\n", 
                     clientIP.toString().c_str(), 
                     configServer.uri().c_str());
        configServer.send(200, "text/html", renderPortalPage(""));
    });
}

String WiFiManager::renderPortalPage(const String& statusMessage) {
    String message = statusMessage;
    String page;
    
    // Obtener la MAC del ESP32
    uint8_t localMac[6];
    WiFi.macAddress(localMac);
    String localMacStr = macToString(localMac);

    page  = "<!DOCTYPE html><html lang='es'><head>";
    page += "<meta charset='UTF-8'>";
    page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    page += "<title>BovinoIOT - Configuración</title>";
    page += "<style>";
    page += "body{font-family:Arial,sans-serif;background:#f5f5f5;margin:0;padding:20px;}";
    page += ".container{max-width:450px;margin:0 auto;background:#fff;border-radius:8px;";
    page += "box-shadow:0 2px 10px rgba(0,0,0,0.1);padding:30px;}";
    page += "h1{color:#1976d2;font-size:24px;margin:0 0 10px;text-align:center;}";
    page += "p{color:#666;font-size:14px;margin:0 0 20px;text-align:center;}";
    page += "label{display:block;margin:15px 0 5px;font-weight:bold;color:#333;font-size:14px;}";
    page += "input{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;";
    page += "box-sizing:border-box;font-size:16px;}";
    page += "input:focus{outline:none;border-color:#1976d2;}";
    page += "select{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;";
    page += "box-sizing:border-box;font-size:16px;background:#fff;cursor:pointer;}";
    page += "select:focus{outline:none;border-color:#1976d2;}";
    page += ".radio-group{display:flex;gap:20px;margin:15px 0;justify-content:center;}";
    page += ".radio-option{display:flex;align-items:center;padding:10px 20px;";
    page += "border:2px solid #ddd;border-radius:6px;cursor:pointer;transition:all 0.3s;}";
    page += ".radio-option:hover{border-color:#1976d2;background:#f0f7ff;}";
    page += ".radio-option input{margin-right:8px;cursor:pointer;}";
    page += ".radio-option label{margin:0;cursor:pointer;font-weight:bold;font-size:15px;}";
    page += ".radio-option input:checked + label{color:#1976d2;}";
    page += "button{width:100%;margin-top:20px;padding:14px;background:#1976d2;";
    page += "color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;font-weight:bold;}";
    page += "button:hover{background:#1565c0;}";
    page += ".status{margin-top:15px;padding:12px;border-radius:4px;background:#e3f2fd;";
    page += "color:#0d47a1;font-size:14px;text-align:center;}";
    page += ".info{background:#f5f5f5;color:#666;margin-top:15px;padding:10px;";
    page += "border-radius:4px;font-size:12px;text-align:center;}";
    page += ".mac-display{background:#e8f5e9;color:#2e7d32;padding:15px;border-radius:4px;";
    page += "text-align:center;font-family:monospace;font-size:16px;margin:10px 0;font-weight:bold;}";
    page += ".section{margin:20px 0;padding:20px;background:#f9f9f9;border-radius:6px;}";
    page += ".section-title{font-size:16px;font-weight:bold;color:#1976d2;margin-bottom:10px;}";
    page += ".hidden{display:none !important;}";
    page += ".help-text{font-size:12px;color:#888;margin-top:5px;font-style:italic;}";
    page += "</style>";
    page += "<script>";
    page += "var subLocations=['Bebedero Norte','Bebedero Sur','Comedero Este','Comedero Oeste','Área de Descanso','Zona de Pastoreo'];";
    page += "function toggleFields(){";
    page += "var mode=document.querySelector('input[name=\"device_mode\"]:checked').value;";
    page += "var wifiSection=document.getElementById('wifi_section');";
    page += "var macSection=document.getElementById('mac_section');";
    page += "var macDisplay=document.getElementById('mac_display_section');";
    page += "if(mode=='master'){";
    page += "wifiSection.classList.remove('hidden');";
    page += "macSection.classList.add('hidden');";
    page += "macDisplay.classList.remove('hidden');";
    page += "document.getElementById('ssid').required=true;";
    page += "document.getElementById('master_mac').required=false;";
    page += "}else{";
    page += "wifiSection.classList.add('hidden');";
    page += "macSection.classList.remove('hidden');";
    page += "macDisplay.classList.add('hidden');";
    page += "document.getElementById('ssid').required=false;";
    page += "document.getElementById('master_mac').required=true;";
    page += "}}";
    page += "async function loadZones(){";
    page += "var zoneSelect=document.getElementById('zone_name');";
    page += "if(!zoneSelect)return;";
    page += "zoneSelect.innerHTML='<option value=\"\">Cargando zonas...</option>';";
    page += "try{";
    page += "var response=await fetch('/getZones');";
    page += "if(!response.ok){throw new Error('Error al cargar zonas');}";
    page += "var data=await response.json();";
    page += "zoneSelect.innerHTML='<option value=\"\">-- Selecciona una Zona --</option>';";
    page += "if(data.zones && Array.isArray(data.zones)){";
    page += "data.zones.forEach(function(zone){";
    page += "var option=document.createElement('option');";
    page += "option.value=zone.id;";
    page += "option.textContent=zone.name;";
    page += "zoneSelect.appendChild(option);";
    page += "});";
    page += "}";
    page += "}catch(error){";
    page += "console.error('Error:',error);";
    page += "zoneSelect.innerHTML='<option value=\"\">-- Error al cargar zonas --</option>';";
    page += "}}";
    page += "function populateSublocations(){";
    page += "var subSelect=document.getElementById('sub_location');";
    page += "subSelect.innerHTML='<option value=\"\">-- Selecciona una Sublocalidad --</option>';";
    page += "subLocations.forEach(function(subLoc){";
    page += "var option=document.createElement('option');";
    page += "option.value=subLoc;";
    page += "option.textContent=subLoc;";
    page += "subSelect.appendChild(option);";
    page += "});";
    page += "}";
    page += "window.onload=function(){toggleFields();loadZones();populateSublocations();};";
    page += "</script>";
    page += "</head><body>";
    page += "<div class='container'>";
    page += "<h1>BovinoIOT</h1>";
    page += "<p>Configuración Inicial del Dispositivo</p>";
    page += "<form method='POST' action='/save'>";
    
    // Device Mode Selection
    page += "<div class='section'>";
    page += "<div class='section-title'>Selecciona el Modo del Dispositivo</div>";
    page += "<div class='radio-group'>";
    page += "<div class='radio-option'>";
    page += "<input type='radio' id='mode_slave' name='device_mode' value='slave' checked onchange='toggleFields()'>";
    page += "<label for='mode_slave'>📡 Esclavo</label>";
    page += "</div>";
    page += "<div class='radio-option'>";
    page += "<input type='radio' id='mode_master' name='device_mode' value='master' onchange='toggleFields()'>";
    page += "<label for='mode_master'>🌐 Maestro</label>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    
    // WiFi Section (solo MAESTRO)
    page += "<div id='wifi_section' class='section hidden'>";
    page += "<div class='section-title'>Configuración WiFi (Maestro)</div>";
    page += "<label for='ssid'>Nombre de Red WiFi (SSID)</label>";
    page += "<input type='text' id='ssid' name='ssid' maxlength='32' placeholder='Mi_Red_WiFi'>";
    page += "<label for='password'>Contraseña WiFi</label>";
    page += "<input type='password' id='password' name='password' maxlength='64' placeholder='Dejar vacío si es red abierta'>";
    page += "<p class='help-text'>El maestro necesita WiFi para sincronizar datos con el servidor</p>";
    page += "</div>";
    
    // MAC Display (solo MAESTRO)
    page += "<div id='mac_display_section' class='section hidden'>";
    page += "<div class='section-title'>MAC de este Dispositivo Maestro</div>";
    page += "<div class='mac-display'>" + localMacStr + "</div>";
    page += "<p class='help-text'>⚠️ Copia esta MAC y úsala en los dispositivos esclavos</p>";
    page += "</div>";
    
    // MAC Input Section (solo ESCLAVO)
    page += "<div id='mac_section' class='section'>";
    page += "<div class='section-title'>Configuración del Esclavo</div>";
    page += "<label for='master_mac'>MAC del Dispositivo Maestro</label>";
    page += "<input type='text' id='master_mac' name='master_mac' maxlength='17' ";
    page += "placeholder='AA:BB:CC:DD:EE:FF' pattern='[A-Fa-f0-9:]{17}'>";
    page += "<p class='help-text'>Ingresa la MAC del dispositivo maestro configurado</p>";
    page += "</div>";
    
    // Location Selection (AMBOS: Maestro y Esclavo)
    page += "<div class='section'>";
    page += "<div class='section-title'>📍 Ubicación del Dispositivo</div>";
    page += "<label for='zone_name'>Zona</label>";
    page += "<select id='zone_name' name='zone_name' required>";
    page += "<option value=''>Cargando zonas...</option>";
    page += "</select>";
    page += "<p class='help-text'>Selecciona la zona donde se encuentra el dispositivo</p>";
    page += "<label for='sub_location'>Sublocalidad</label>";
    page += "<select id='sub_location' name='sub_location' required>";
    page += "<option value=''>-- Selecciona una Sublocalidad --</option>";
    page += "</select>";
    page += "<p class='help-text'>Ej: Bebedero Norte, Comedero Este, etc.</p>";
    page += "</div>";
    
    page += "<button type='submit'>💾 Guardar y Reiniciar</button>";
    page += "</form>";

    if (message.length() > 0) {
        page += "<div class='status'>" + message + "</div>";
    }

    page += "<div class='info'>IP: " + WiFi.softAPIP().toString() + "<br>";
    page += "ID Dispositivo: " + String(DEVICE_ID) + "</div>";
    page += "</div></body></html>";

    return page;
}

bool WiFiManager::attemptConnection(unsigned long timeout) {
    unsigned long startTime = millis();
    unsigned long lastToggle = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
        if (millis() - lastToggle >= 250) {
            alertManager.loaderToggle();
            lastToggle = millis();
            Serial.print(".");
        }
        delay(50);
    }

    alertManager.loaderOff();

    return (WiFi.status() == WL_CONNECTED);
}
