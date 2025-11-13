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
    loadDeviceConfig();        // ← PRIMERO: Cargar modo del dispositivo (MASTER/SLAVE)
    loadDeviceLocation();      // ← SEGUNDO: Cargar ubicación (zona, sublocalización, device_id)
    loadStoredCredentials();   // ← TERCERO: Cargar WiFi (solo si es MASTER)
    
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

void WiFiManager::loadDeviceLocation() {
    Preferences prefs;
    if (prefs.begin("location_cfg", true)) {
        // Cargar zona y sublocalización
        LOADED_ZONE_NAME = prefs.getString("zone_name", "");
        LOADED_SUB_LOCATION = prefs.getString("sub_location", "");
        
        if (LOADED_ZONE_NAME.length() > 0 && LOADED_SUB_LOCATION.length() > 0) {
            // Generar DEVICE_ID desde sublocalización (formato: tipo/nombre)
            // Ejemplo: "master/Corral Norte" -> "IOT_MASTER_CORRAL_NORTE"
            String deviceType = "";
            String locationName = LOADED_SUB_LOCATION;
            
            int slashPos = LOADED_SUB_LOCATION.indexOf('/');
            if (slashPos > 0) {
                deviceType = LOADED_SUB_LOCATION.substring(0, slashPos);
                locationName = LOADED_SUB_LOCATION.substring(slashPos + 1);
            }
            
            // Convertir a ID válido: mayúsculas, espacios -> guiones bajos
            deviceType.toUpperCase();
            locationName.toUpperCase();
            locationName.replace(" ", "_");
            locationName.replace("-", "_");
            
            LOADED_DEVICE_ID = "IOT_" + deviceType + "_" + locationName;
            
            Serial.printf("[Config] Ubicación cargada:\n");
            Serial.printf("  - Zona: %s\n", LOADED_ZONE_NAME.c_str());
            Serial.printf("  - Sublocalización: %s\n", LOADED_SUB_LOCATION.c_str());
            Serial.printf("  - Device ID: %s\n", LOADED_DEVICE_ID.c_str());
        } else {
            Serial.println("[Config] No hay ubicación guardada, usando valores por defecto");
            LOADED_ZONE_NAME = DEVICE_LOCATION;  // Usar valor hardcoded como fallback
            LOADED_SUB_LOCATION = DEVICE_LOCATION;
            LOADED_DEVICE_ID = DEVICE_ID;
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
    Serial.printf("[GraphQL] Memoria libre antes: %d bytes\n", ESP.getFreeHeap());
    
    // Verificar conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GraphQL] Error: WiFi no conectado");
        return "[]";
    }
    
    // Esperar estabilización
    delay(1000);
    
    Serial.printf("[GraphQL] IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Cliente WiFi seguro
    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) {
        Serial.println("[GraphQL] Error: No se pudo crear cliente SSL");
        return "[]";
    }
    
    client->setInsecure();
    client->setTimeout(20);
    
    HTTPClient http;
    http.setTimeout(25000);
    http.setReuse(false);
    
    const char* graphqlUrl = "https://bovino-io-backend.onrender.com/graphql";
    
    Serial.printf("[GraphQL] Conectando a: %s\n", graphqlUrl);
    
    if (!http.begin(*client, graphqlUrl)) {
        Serial.println("[GraphQL] Error: No se pudo iniciar HTTPS");
        delete client;
        return "[]";
    }
    
    String query = "{\"query\":\"query($userId:Int!){ zonesByUser(userId:$userId){ id name } }\",\"variables\":{\"userId\":" + String(userId) + "}}";
    
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(query);
    
    String result = "[]";
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[GraphQL] Respuesta HTTP: %d\n", httpCode);
        Serial.printf("[GraphQL] Respuesta completa: %s\n", response.c_str());
        
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
                        
                        result = "";  // Limpiar el string antes de serializar
                        serializeJson(resultArray, result);
                        Serial.printf("[GraphQL] ✓ Zonas obtenidas: %d\n", zoneCount);
                        Serial.printf("[GraphQL] JSON resultado: %s\n", result.c_str());
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
    delete client;
    
    Serial.printf("[GraphQL] Memoria libre después: %d bytes\n", ESP.getFreeHeap());
    
    return result;
}

// ==================== GraphQL - Obtener Sublocalidades por Zona ====================
String WiFiManager::fetchSublocationsFromGraphQL(int zoneId) {
    Serial.printf("[GraphQL] Obteniendo sublocalidades de la zona %d...\n", zoneId);
    
    // Verificar conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GraphQL] Error: WiFi no conectado");
        return "[]";
    }
    
    // Cliente WiFi seguro para HTTPS
    WiFiClientSecure client;
    client.setInsecure();  // Deshabilitar verificación SSL
    client.setTimeout(15);  // 15 segundos timeout
    
    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    
    const char* graphqlUrl = "https://bovino-io-backend.onrender.com/graphql";
    
    if (!http.begin(client, graphqlUrl)) {
        Serial.println("[GraphQL] Error: No se pudo iniciar conexión HTTPS");
        return "[]";
    }
    
    Serial.println("[GraphQL] Conexión HTTPS iniciada");
    
    // Query GraphQL para dispositivos por zona
    String query = "{\"query\":\"query DevicesByZone($id_zone: Int!){ dispositivosByZone(id_zone: $id_zone){ id type location battery_level status zone { id name } } }\",\"variables\":{\"id_zone\":" + String(zoneId) + "}}";
    
    Serial.printf("[GraphQL] Query: %s\n", query.c_str());
    
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(query);
    
    String result = "[]";
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[GraphQL] Respuesta HTTP: %d\n", httpCode);
        Serial.printf("[GraphQL] Respuesta completa: %s\n", response.c_str());
        
        if (httpCode == 200) {
            const size_t capacity = JSON_ARRAY_SIZE(20) + JSON_OBJECT_SIZE(100);
            DynamicJsonDocument doc(capacity);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                if (doc.containsKey("data")) {
                    JsonObject dataObj = doc["data"];
                    if (dataObj.containsKey("dispositivosByZone")) {
                        JsonArray devices = dataObj["dispositivosByZone"];
                        
                        DynamicJsonDocument resultDoc(JSON_ARRAY_SIZE(20) + JSON_OBJECT_SIZE(50));
                        JsonArray resultArray = resultDoc.createNestedArray();
                        
                        int deviceCount = 0;
                        for (JsonVariant item : devices) {
                            if (item.is<JsonObject>()) {
                                JsonObject sublocObj = resultArray.createNestedObject();
                                if (item.containsKey("id")) {
                                    sublocObj["id"] = item["id"];
                                }
                                // Combinar type/location
                                String sublocation = "";
                                if (item.containsKey("type") && item.containsKey("location")) {
                                    sublocation = String(item["type"].as<const char*>()) + "/" + String(item["location"].as<const char*>());
                                    sublocObj["name"] = sublocation;
                                }
                                deviceCount++;
                            }
                        }
                        
                        result = "";
                        serializeJson(resultArray, result);
                        Serial.printf("[GraphQL] ✓ Sublocalidades obtenidas: %d\n", deviceCount);
                        Serial.printf("[GraphQL] JSON resultado: %s\n", result.c_str());
                    } else {
                        Serial.println("[GraphQL] Error: Campo 'dispositivosByZone' no encontrado");
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

    // Ruta para conectar WiFi y verificar conexión
    configServer.on("/connectWifi", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /connectWifi desde %s\n", clientIP.toString().c_str());
        
        String ssid = configServer.arg("ssid");
        String password = configServer.arg("password");
        
        ssid.trim();
        password.trim();
        
        if (ssid.length() == 0) {
            configServer.send(200, "application/json", "{\"success\":false,\"message\":\"SSID vacío\"}");
            return;
        }
        
        Serial.printf("[Portal] Intentando conectar a WiFi: %s\n", ssid.c_str());
        
        // Cambiar a modo DUAL (AP + STA) para conectar a WiFi externo
        WiFi.mode(WIFI_AP_STA);
        delay(100);
        
        // Conectar al WiFi externo
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Esperar hasta 15 segundos por conexión
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[Portal] ✓ WiFi conectado exitosamente");
            Serial.printf("[Portal] IP obtenida: %s\n", WiFi.localIP().toString().c_str());
            
            // ⚡ CRÍTICO: Dar tiempo para que la memoria se estabilice después de conectar WiFi
            // El modo AP+STA causa fragmentación de memoria. Este delay permite que el heap
            // se reorganice antes del primer request HTTPS/SSL que requiere memoria contigua
            delay(1000);
            
            // Forzar garbage collection
            Serial.printf("[Portal] Memoria libre post-WiFi: %d bytes\n", ESP.getFreeHeap());
            
            configServer.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi conectado\",\"ip\":\"" + WiFi.localIP().toString() + "\"}");
        } else {
            Serial.println("\n[Portal] ✗ Error al conectar WiFi");
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);  // Volver a modo AP puro
            configServer.send(200, "application/json", "{\"success\":false,\"message\":\"No se pudo conectar al WiFi\"}");
        }
    });

    // Ruta para obtener zonas desde GraphQL
    configServer.on("/getZones", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET /getZones desde %s\n", clientIP.toString().c_str());
        
        // Verificar si WiFi está conectado
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Portal] ✗ WiFi NO conectado. No se pueden obtener zonas.");
            configServer.send(200, "application/json", "{\"zones\":[],\"error\":\"WiFi no conectado\"}");
            return;
        }
        
        Serial.printf("[Portal] ✓ WiFi conectado - IP: %s\n", WiFi.localIP().toString().c_str());
        
        // ⚡ CRÍTICO: Pequeño delay adicional para asegurar estabilidad de memoria
        // antes de la primera llamada HTTPS que requiere ~8-12KB de heap contiguo
        delay(500);
        
        Serial.println("[Portal] Llamando a fetchZonesFromGraphQL...");
        
        String zonesJson = fetchZonesFromGraphQL(3);
        
        Serial.printf("[Portal] Zonas JSON recibido (longitud: %d): %s\n", zonesJson.length(), zonesJson.c_str());
        
        String response = "{\"zones\":" + zonesJson + "}";
        Serial.printf("[Portal] Enviando respuesta: %s\n", response.c_str());
        
        configServer.send(200, "application/json", response);
    });

    // Ruta para obtener sublocalidades por zona
    configServer.on("/getSublocations", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET /getSublocations desde %s\n", clientIP.toString().c_str());
        
        String zoneIdStr = configServer.arg("zone_id");
        int zoneId = zoneIdStr.toInt();
        
        if (zoneId == 0) {
            Serial.println("[Portal] ✗ zone_id inválido o no proporcionado");
            configServer.send(200, "application/json", "{\"sublocations\":[],\"error\":\"zone_id requerido\"}");
            return;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Portal] ✗ WiFi NO conectado");
            configServer.send(200, "application/json", "{\"sublocations\":[],\"error\":\"WiFi no conectado\"}");
            return;
        }
        
        Serial.printf("[Portal] ✓ Obteniendo sublocalidades para zona ID: %d\n", zoneId);
        
        String sublocationsJson = fetchSublocationsFromGraphQL(zoneId);
        
        Serial.printf("[Portal] Sublocalidades JSON (longitud: %d): %s\n", sublocationsJson.length(), sublocationsJson.c_str());
        
        String response = "{\"sublocations\":" + sublocationsJson + "}";
        Serial.printf("[Portal] Enviando respuesta: %s\n", response.c_str());
        
        configServer.send(200, "application/json", response);
    });

    // Ruta para guardar configuración
    configServer.on("/save", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /save desde %s\n", clientIP.toString().c_str());
        
        String deviceMode = configServer.arg("device_mode");
        String tempSSID = configServer.arg("temp_ssid");
        String tempPassword = configServer.arg("temp_password");
        String masterMac = configServer.arg("master_mac");
        String zoneName = configServer.arg("zone_name");
        String subLocation = configServer.arg("sub_location");

        deviceMode.trim();
        tempSSID.trim();
        tempPassword.trim();
        masterMac.trim();
        zoneName.trim();
        subLocation.trim();

        // Validar zona y sublocalización (requeridas para TODOS)
        if (zoneName.length() == 0) {
            Serial.println("[Portal] Error: Zona requerida");
            configServer.send(200, "text/html", renderPortalPage("❌ Debes seleccionar una Zona."));
            return;
        }
        
        if (subLocation.length() == 0) {
            Serial.println("[Portal] Error: Sublocalización requerida");
            configServer.send(200, "text/html", renderPortalPage("❌ Debes seleccionar una Sublocalización."));
            return;
        }

        // Determinar el modo del dispositivo
        DeviceMode mode = (deviceMode == "master") ? DEVICE_MASTER : DEVICE_SLAVE;
        
        // Validación según el modo
        if (mode == DEVICE_MASTER) {
            // MAESTRO: Guardar WiFi persistentemente
            if (tempSSID.length() == 0) {
                Serial.println("[Portal] Error: MAESTRO requiere SSID");
                configServer.send(200, "text/html", renderPortalPage("❌ El modo MAESTRO requiere configurar WiFi."));
                return;
            }
            
            Serial.printf("[Portal] Configurando MAESTRO\n");
            Serial.printf("[Portal] WiFi SSID: %s (se guardará persistentemente)\n", tempSSID.c_str());
            Serial.printf("[Portal] Zona: %s, Sublocalización: %s\n", zoneName.c_str(), subLocation.c_str());
            
            saveCredentials(tempSSID, tempPassword);  // GUARDAR WiFi para MAESTRO
            saveDeviceConfig(mode, "");  // MAESTRO no necesita MAC
            saveDeviceLocation(zoneName, subLocation);  // Guardar ubicación
            
            String message = "✅ MAESTRO configurado correctamente<br><br>";
            message += "📶 WiFi: " + tempSSID + " (guardado)<br>";
            message += "📍 Zona: " + zoneName + "<br>";
            message += "📍 Sublocalidad: " + subLocation + "<br><br>";
            message += "🔄 El dispositivo se reiniciará en 3 segundos...";
            
            configServer.send(200, "text/html", renderPortalPage(message));
            
            delay(3000);
            ESP.restart();  // Reiniciar para aplicar configuración
            
        } else {
            // ESCLAVO: NO guardar WiFi (solo se usó temporalmente)
            if (masterMac.length() == 0 || masterMac.length() < 17) {
                Serial.println("[Portal] Error: ESCLAVO requiere MAC del maestro");
                configServer.send(200, "text/html", renderPortalPage("❌ El modo ESCLAVO requiere la MAC del Maestro."));
                return;
            }
            
            Serial.printf("[Portal] Configurando ESCLAVO\n");
            Serial.printf("[Portal] MAC Maestro: %s\n", masterMac.c_str());
            Serial.printf("[Portal] Zona: %s, Sublocalización: %s\n", zoneName.c_str(), subLocation.c_str());
            Serial.println("[Portal] WiFi NO se guardará (solo para configuración inicial)");
            
            // NO llamar saveCredentials() - El esclavo NO guarda WiFi
            saveDeviceConfig(mode, masterMac);  // Guardar modo ESCLAVO + MAC del maestro
            saveDeviceLocation(zoneName, subLocation);  // Guardar ubicación
            
            String message = "✅ ESCLAVO configurado correctamente<br><br>";
            message += "📡 MAC Maestro: " + masterMac + "<br>";
            message += "📍 Zona: " + zoneName + "<br>";
            message += "📍 Sublocalidad: " + subLocation + "<br><br>";
            message += "ℹ️ WiFi NO guardado (solo usado para configuración)<br>";
            message += "🔄 El dispositivo se reiniciará en 3 segundos...";
            
            configServer.send(200, "text/html", renderPortalPage(message));
            
            delay(3000);
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
    page += "var currentStep=1;";
    page += "var wifiSSID='';";
    page += "var wifiPassword='';";
    page += "var selectedZoneId=null;";
    page += "function showStep(step){";
    page += "document.getElementById('step1').classList.add('hidden');";
    page += "document.getElementById('step2').classList.add('hidden');";
    page += "document.getElementById('step3').classList.add('hidden');";
    page += "document.getElementById('step'+step).classList.remove('hidden');";
    page += "currentStep=step;";
    page += "}";
    page += "async function connectWifi(){";
    page += "var ssid=document.getElementById('ssid').value;";
    page += "var password=document.getElementById('password').value;";
    page += "if(!ssid){alert('Ingresa el SSID del WiFi');return;}";
    page += "wifiSSID=ssid;";
    page += "wifiPassword=password;";
    page += "document.getElementById('connect_btn').disabled=true;";
    page += "document.getElementById('connect_btn').innerText='⏳ Conectando...';";
    page += "try{";
    page += "var response=await fetch('/connectWifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(password)});";
    page += "var data=await response.json();";
    page += "if(data.success){";
    page += "document.getElementById('wifi_status').innerHTML='<div style=\"color:green;font-weight:bold;margin:10px 0;\">✓ WiFi Conectado - IP: '+data.ip+'</div>';";
    page += "await loadZones();";
    page += "showStep(2);";
    page += "}else{";
    page += "alert('❌ Error al conectar WiFi: '+data.message);";
    page += "document.getElementById('connect_btn').disabled=false;";
    page += "document.getElementById('connect_btn').innerText='🔌 Conectar WiFi';";
    page += "}";
    page += "}catch(error){";
    page += "alert('❌ Error de conexión: '+error.message);";
    page += "document.getElementById('connect_btn').disabled=false;";
    page += "document.getElementById('connect_btn').innerText='🔌 Conectar WiFi';";
    page += "}";
    page += "}";
    page += "function selectDeviceMode(){";
    page += "var mode=document.querySelector('input[name=\"device_mode\"]:checked').value;";
    page += "if(mode=='master'){";
    page += "document.getElementById('master_fields').classList.remove('hidden');";
    page += "document.getElementById('slave_fields').classList.add('hidden');";
    page += "}else{";
    page += "document.getElementById('master_fields').classList.add('hidden');";
    page += "document.getElementById('slave_fields').classList.remove('hidden');";
    page += "}";
    page += "showStep(3);";
    page += "}";
    page += "async function loadZones(){";
    page += "var zoneSelect=document.getElementById('zone_name_master');";
    page += "var zoneSelectSlave=document.getElementById('zone_name_slave');";
    page += "if(zoneSelect)zoneSelect.innerHTML='<option value=\"\">⏳ Cargando zonas...</option>';";
    page += "if(zoneSelectSlave)zoneSelectSlave.innerHTML='<option value=\"\">⏳ Cargando zonas...</option>';";
    page += "try{";
    page += "var response=await fetch('/getZones');";
    page += "if(!response.ok){throw new Error('Error HTTP '+response.status);}";
    page += "var data=await response.json();";
    page += "console.log('Respuesta /getZones:',data);";
    page += "var optionsHTML='<option value=\"\">-- Selecciona una Zona --</option>';";
    page += "if(data.zones && Array.isArray(data.zones) && data.zones.length>0){";
    page += "data.zones.forEach(function(zone){";
    page += "if(zone.name && zone.id){";
    page += "optionsHTML+='<option value=\"'+zone.name+'\" data-zone-id=\"'+zone.id+'\">'+zone.name+'</option>';";
    page += "}";
    page += "});";
    page += "console.log('✓ Zonas cargadas:',data.zones.length);";
    page += "}else{";
    page += "console.warn('⚠️ No hay zonas');";
    page += "optionsHTML='<option value=\"\">-- No hay zonas disponibles --</option>';";
    page += "}";
    page += "if(zoneSelect){zoneSelect.innerHTML=optionsHTML;zoneSelect.onchange=function(){onZoneChangeMaster();};}";
    page += "if(zoneSelectSlave){zoneSelectSlave.innerHTML=optionsHTML;zoneSelectSlave.onchange=function(){onZoneChangeSlave();};}";
    page += "}catch(error){";
    page += "console.error('❌ Error loadZones:',error);";
    page += "var errorHTML='<option value=\"\">-- Error: '+error.message+'</option>';";
    page += "if(zoneSelect)zoneSelect.innerHTML=errorHTML;";
    page += "if(zoneSelectSlave)zoneSelectSlave.innerHTML=errorHTML;";
    page += "}";
    page += "}";
    page += "async function loadSublocations(zoneId,isMaster){";
    page += "var subSelectMaster=document.getElementById('sub_location_master');";
    page += "var subSelectSlave=document.getElementById('sub_location_slave');";
    page += "var targetSelect=isMaster?subSelectMaster:subSelectSlave;";
    page += "if(targetSelect)targetSelect.innerHTML='<option value=\"\">⏳ Cargando sublocalidades...</option>';";
    page += "try{";
    page += "var response=await fetch('/getSublocations?zone_id='+zoneId);";
    page += "if(!response.ok){throw new Error('Error HTTP '+response.status);}";
    page += "var data=await response.json();";
    page += "console.log('Respuesta /getSublocations:',data);";
    page += "var optionsHTML='<option value=\"\">-- Selecciona una Sublocalidad --</option>';";
    page += "if(data.sublocations && Array.isArray(data.sublocations) && data.sublocations.length>0){";
    page += "data.sublocations.forEach(function(subloc){";
    page += "if(subloc.name){";
    page += "optionsHTML+='<option value=\"'+subloc.name+'\">'+subloc.name+'</option>';";
    page += "}";
    page += "});";
    page += "console.log('✓ Sublocalidades cargadas:',data.sublocations.length);";
    page += "}else{";
    page += "console.warn('⚠️ No hay dispositivos');";
    page += "optionsHTML='<option value=\"\">-- No hay dispositivos en esta zona --</option>';";
    page += "}";
    page += "if(targetSelect)targetSelect.innerHTML=optionsHTML;";
    page += "}catch(error){";
    page += "console.error('❌ Error loadSublocations:',error);";
    page += "var errorHTML='<option value=\"\">-- Error: '+error.message+'</option>';";
    page += "if(targetSelect)targetSelect.innerHTML=errorHTML;";
    page += "}";
    page += "}";
    page += "function onZoneChangeMaster(){";
    page += "var zoneSelect=document.getElementById('zone_name_master');";
    page += "var selectedOption=zoneSelect.options[zoneSelect.selectedIndex];";
    page += "var zoneId=selectedOption.getAttribute('data-zone-id');";
    page += "if(zoneId){";
    page += "console.log('Zona seleccionada (Master):',zoneId);";
    page += "loadSublocations(zoneId,true);";
    page += "}";
    page += "}";
    page += "function onZoneChangeSlave(){";
    page += "var zoneSelect=document.getElementById('zone_name_slave');";
    page += "var selectedOption=zoneSelect.options[zoneSelect.selectedIndex];";
    page += "var zoneId=selectedOption.getAttribute('data-zone-id');";
    page += "if(zoneId){";
    page += "console.log('Zona seleccionada (Slave):',zoneId);";
    page += "loadSublocations(zoneId,false);";
    page += "}";
    page += "}";
    page += "function prepareSubmit(isMaster){";
    page += "var form=document.getElementById(isMaster?'form_master':'form_slave');";
    page += "var hiddenSSID=document.createElement('input');";
    page += "hiddenSSID.type='hidden';";
    page += "hiddenSSID.name='temp_ssid';";
    page += "hiddenSSID.value=wifiSSID;";
    page += "form.appendChild(hiddenSSID);";
    page += "var hiddenPass=document.createElement('input');";
    page += "hiddenPass.type='hidden';";
    page += "hiddenPass.name='temp_password';";
    page += "hiddenPass.value=wifiPassword;";
    page += "form.appendChild(hiddenPass);";
    page += "return true;";
    page += "}";
    page += "</script>";
    page += "</head><body>";
    page += "<div class='container'>";
    page += "<h1>BovinoIOT</h1>";
    page += "<p>Configuración Inicial del Dispositivo</p>";
    
    // PASO 1: Conexión WiFi (TODOS)
    page += "<div id='step1'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>Paso 1: Conectar a WiFi</div>";
    page += "<p class='help-text'>⚠️ Necesitas WiFi temporalmente para obtener las zonas del servidor</p>";
    page += "<label for='ssid'>Nombre de Red WiFi (SSID)</label>";
    page += "<input type='text' id='ssid' maxlength='32' placeholder='Mi_Red_WiFi'>";
    page += "<label for='password'>Contraseña WiFi</label>";
    page += "<input type='password' id='password' maxlength='64' placeholder='Dejar vacío si es red abierta'>";
    page += "<div id='wifi_status'></div>";
    page += "<button type='button' id='connect_btn' onclick='connectWifi()'>🔌 Conectar WiFi</button>";
    page += "</div>";
    page += "</div>";
    
    // PASO 2: Selección de tipo de dispositivo
    page += "<div id='step2' class='hidden'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>Paso 2: Selecciona el Tipo de Dispositivo</div>";
    page += "<div class='radio-group'>";
    page += "<div class='radio-option'>";
    page += "<input type='radio' id='mode_master' name='device_mode' value='master' checked>";
    page += "<label for='mode_master'>🌐 Maestro</label>";
    page += "</div>";
    page += "<div class='radio-option'>";
    page += "<input type='radio' id='mode_slave' name='device_mode' value='slave'>";
    page += "<label for='mode_slave'>📡 Esclavo</label>";
    page += "</div>";
    page += "</div>";
    page += "<p class='help-text'>El Maestro usa WiFi y envía datos al servidor. Los Esclavos solo se comunican con el Maestro vía ESP-NOW.</p>";
    page += "<button type='button' onclick='selectDeviceMode()'>➡️ Continuar</button>";
    page += "</div>";
    page += "</div>";
    
    // PASO 3: Configuración específica (por defecto muestra MAESTRO)
    page += "<div id='step3' class='hidden'>";
    
    // MAESTRO (visible por defecto porque mode_master está checked)
    page += "<div id='master_fields'>";
    page += "<form id='form_master' method='POST' action='/save' onsubmit='return prepareSubmit(true)'>";
    page += "<input type='hidden' name='device_mode' value='master'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>🌐 Configuración del Maestro</div>";
    page += "<div class='mac-display'>" + localMacStr + "</div>";
    page += "<p class='help-text'>⚠️ Esta es tu MAC. Cópiala para configurar los esclavos.</p>";
    page += "</div>";
    page += "<div class='section'>";
    page += "<div class='section-title'>📍 Ubicación del Dispositivo</div>";
    page += "<label for='zone_name_master'>Zona</label>";
    page += "<select id='zone_name_master' name='zone_name' required>";
    page += "<option value=''>-- Selecciona una Zona --</option>";
    page += "</select>";
    page += "<label for='sub_location_master'>Sublocalidad</label>";
    page += "<select id='sub_location_master' name='sub_location' required>";
    page += "<option value=''>-- Selecciona una Sublocalidad --</option>";
    page += "</select>";
    page += "</div>";
    page += "<button type='submit'>💾 Guardar y Reiniciar</button>";
    page += "</form>";
    page += "</div>";
    
    // ESCLAVO (oculto por defecto)
    page += "<div id='slave_fields' class='hidden'>";
    page += "<form id='form_slave' method='POST' action='/save' onsubmit='return prepareSubmit(false)'>";
    page += "<input type='hidden' name='device_mode' value='slave'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>📡 Configuración del Esclavo</div>";
    page += "<label for='master_mac'>MAC del Dispositivo Maestro</label>";
    page += "<input type='text' id='master_mac' name='master_mac' maxlength='17' ";
    page += "placeholder='AA:BB:CC:DD:EE:FF' pattern='[A-Fa-f0-9:]{17}' required>";
    page += "<p class='help-text'>Ingresa la MAC del dispositivo maestro ya configurado</p>";
    page += "</div>";
    page += "<div class='section'>";
    page += "<div class='section-title'>📍 Ubicación del Dispositivo</div>";
    page += "<label for='zone_name_slave'>Zona</label>";
    page += "<select id='zone_name_slave' name='zone_name' required>";
    page += "<option value=''>-- Selecciona una Zona --</option>";
    page += "</select>";
    page += "<label for='sub_location_slave'>Sublocalidad</label>";
    page += "<select id='sub_location_slave' name='sub_location' required>";
    page += "<option value=''>-- Selecciona una Sublocalidad --</option>";
    page += "</select>";
    page += "</div>";
    page += "<button type='submit'>💾 Guardar y Reiniciar</button>";
    page += "</form>";
    page += "</div>";
    page += "</div>";
    page += "</div>";
    page += "</div>";

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
