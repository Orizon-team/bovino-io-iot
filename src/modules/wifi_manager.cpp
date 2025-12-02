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
            connectionAttempts(0),
            loggedUserId(0),
            dnsServerActive(false),
            backendIPCached(false) {
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
    dnsServerActive = false;
    
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
        // Solo procesar DNS si está activo
        if (dnsServerActive) {
            dnsServer.processNextRequest();  // Procesar peticiones DNS (Captive Portal)
        }
        configServer.handleClient();     // Procesar peticiones HTTP

        // Anunciar el portal periódicamente (cada 30 segundos)
        if (lastPortalAnnounce == 0 || millis() - lastPortalAnnounce > 30000) {
            lastPortalAnnounce = millis();
            Serial.println("[Portal] ==========================================");
            Serial.printf("[Portal] URL: http://%s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("[Portal] Clientes conectados: %d\n", WiFi.softAPgetStationNum());
            if (dnsServerActive) {
                Serial.println("[Portal] DNS activo: Redirigiendo todas las URLs");
            }
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
        // Cargar zona, sublocalización y zone_id
        LOADED_ZONE_NAME = prefs.getString("zone_name", "");
        LOADED_SUB_LOCATION = prefs.getString("sub_location", "");
        LOADED_ZONE_ID = prefs.getInt("zone_id", 0);
        
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
            Serial.printf("  - Zona: %s (ID: %d)\n", LOADED_ZONE_NAME.c_str(), LOADED_ZONE_ID);
            Serial.printf("  - Sublocalización: %s\n", LOADED_SUB_LOCATION.c_str());
            Serial.printf("  - Device ID: %s\n", LOADED_DEVICE_ID.c_str());
        } else {
            Serial.println("[Config] No hay ubicación guardada, usando valores por defecto");
            LOADED_ZONE_NAME = LOADED_ZONE_NAME;  // Usar valor hardcoded como fallback
            LOADED_SUB_LOCATION = LOADED_SUB_LOCATION;
            LOADED_DEVICE_ID = LOADED_DEVICE_ID;
            LOADED_ZONE_ID = 0;
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
        Serial.println("[Config] WiFi limpiado");
    }
    
    // Limpiar configuración del dispositivo
    if (prefs.begin("device_cfg", false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Config] Dispositivo limpiado");
    }
    
    // Limpiar configuración de ubicación
    if (prefs.begin("location_cfg", false)) {
        prefs.clear();
        prefs.end();
        Serial.println("[Config] Ubicacion limpiada");
    }
    
    // Resetear variables
    currentSSID = "";
    currentPassword = "";
    CURRENT_DEVICE_MODE = DEVICE_SLAVE;  // Por defecto ESCLAVO
    
    Serial.println("[Config] Configuracion limpiada completamente");
}

// ==================== Resolver DNS con Retry ====================
bool WiFiManager::resolveBackendDNS(IPAddress& ip) {
    const char* hostname = "bovino-io-backend.onrender.com";
    const int MAX_DNS_RETRIES = 3;
    const int DNS_RETRY_DELAY = 2000;
    
    // Si ya tenemos IP en cache, usar ese
    if (backendIPCached) {
        ip = cachedBackendIP;
        Serial.printf("[DNS] Usando IP en cache: %s\n", ip.toString().c_str());
        return true;
    }
    
    // Intentar resolver DNS con retries
    for (int i = 0; i < MAX_DNS_RETRIES; i++) {
        Serial.printf("[DNS] Intento %d/%d resolviendo: %s\n", i + 1, MAX_DNS_RETRIES, hostname);
        
        if (WiFi.hostByName(hostname, ip)) {
            Serial.printf("[DNS] Resuelto exitosamente: %s\n", ip.toString().c_str());
            // Guardar en cache
            cachedBackendIP = ip;
            backendIPCached = true;
            return true;
        }
        
        Serial.printf("[DNS] Fallo intento %d\n", i + 1);
        
        if (i < MAX_DNS_RETRIES - 1) {
            Serial.printf("[DNS] Esperando %dms antes de reintentar...\n", DNS_RETRY_DELAY);
            delay(DNS_RETRY_DELAY);
        }
    }
    
    Serial.println("[DNS] ERROR: No se pudo resolver DNS despues de todos los intentos");
    return false;
}

// ==================== GraphQL - Login Usuario ====================
String WiFiManager::loginUser(const String& email, const String& password) {
    Serial.printf("[GraphQL] Intentando login con email: %s\n", email.c_str());
    Serial.printf("[GraphQL] Memoria libre antes: %d bytes\n", ESP.getFreeHeap());
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GraphQL] Error: WiFi no conectado");
        return "{\"success\":false,\"message\":\"WiFi no conectado\"}";
    }
    
    delay(500);
    
    // Resolver DNS con retry
    IPAddress serverIP;
    if (!resolveBackendDNS(serverIP)) {
        return "{\"success\":false,\"message\":\"No se pudo resolver DNS del servidor\"}";
    }
    
    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) {
        Serial.println("[GraphQL] Error: No se pudo crear cliente SSL");
        return "{\"success\":false,\"message\":\"Error de memoria\"}";
    }
    
    Serial.println("[GraphQL] Configurando cliente SSL...");
    client->setInsecure();
    client->setTimeout(90);
    Serial.printf("[GraphQL] Cliente configurado - Timeout: 90s\n");
    
    HTTPClient http;
    http.setTimeout(60000);  // 60 segundos
    http.setReuse(false);
    
    const char* graphqlUrl = "https://bovino-io-backend.onrender.com/graphql";
    
    Serial.printf("[GraphQL] Conectando a: %s\n", graphqlUrl);
    Serial.printf("[GraphQL] Email a autenticar: %s\n", email.c_str());
    
    Serial.println("[GraphQL] Iniciando conexion HTTPS...");
    unsigned long startConnect = millis();
    
    if (!http.begin(*client, graphqlUrl)) {
        Serial.println("[GraphQL] Error: http.begin() fallo");
        delete client;
        return "{\"success\":false,\"message\":\"Error de conexión\"}";
    }
    
    unsigned long connectTime = millis() - startConnect;
    Serial.printf("[GraphQL] Conexion establecida en %lu ms\n", connectTime);
    
    // Construir mutation de login (formato exacto como Postman)
    String mutation = String("{") +
        "\"query\":\"mutation Login($input: LoginUserInput!){ login(input:$input){ id_user email name } }\"," +
        "\"variables\":{" +
            "\"input\":{" +
                "\"email\":\"" + email + "\"," +
                "\"password\":\"" + password + "\"" +
            "}" +
        "}" +
    "}";
    
    Serial.printf("[GraphQL] Mutation JSON:\n%s\n", mutation.c_str());
    
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(mutation);
    
    String result = "{\"success\":false,\"message\":\"Error desconocido\"}";
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[GraphQL] Respuesta HTTP: %d\n", httpCode);
        Serial.printf("[GraphQL] Respuesta: %s\n", response.c_str());
        
        if (httpCode == 200) {
            const size_t capacity = JSON_OBJECT_SIZE(10) + 200;
            DynamicJsonDocument doc(capacity);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                if (doc.containsKey("data")) {
                    JsonObject dataObj = doc["data"];
                    if (dataObj.containsKey("login")) {
                        JsonObject loginObj = dataObj["login"];
                        if (loginObj.containsKey("id_user")) {
                            int userId = loginObj["id_user"].as<int>();
                            if (userId > 0) {
                                loggedUserId = userId;
                                String userName = loginObj["name"] | "Usuario";
                                Serial.printf("[GraphQL] Login exitoso - User ID: %d, Nombre: %s\n", loggedUserId, userName.c_str());
                                result = "{\"success\":true,\"user_id\":" + String(loggedUserId) + ",\"name\":\"" + userName + "\"}";
                            } else {
                                Serial.printf("[GraphQL] Login falló - User ID inválido: %d\n", userId);
                                result = "{\"success\":false,\"message\":\"Credenciales incorrectas\"}";
                            }
                        } else {
                            result = "{\"success\":false,\"message\":\"Respuesta inválida del servidor\"}";
                        }
                    } else {
                        result = "{\"success\":false,\"message\":\"Credenciales incorrectas\"}";
                    }
                } else if (doc.containsKey("errors")) {
                    JsonArray errors = doc["errors"];
                    String errorMsg = "Error de autenticación";
                    if (errors.size() > 0) {
                        errorMsg = errors[0]["message"] | "Error desconocido";
                    }
                    Serial.printf("[GraphQL] Error GraphQL: %s\n", errorMsg.c_str());
                    result = "{\"success\":false,\"message\":\"" + errorMsg + "\"}";
                } else {
                    result = "{\"success\":false,\"message\":\"Respuesta inválida\"}";
                }
            } else {
                Serial.printf("[GraphQL] Error JSON: %s\n", error.c_str());
                result = "{\"success\":false,\"message\":\"Error al procesar respuesta\"}";
            }
        } else {
            result = "{\"success\":false,\"message\":\"Error HTTP " + String(httpCode) + "\"}";
        }
    } else {
        Serial.printf("[GraphQL] Error de conexión: %d\n", httpCode);
        String errorMsg = "Timeout de conexión";
        if (httpCode == -1) errorMsg = "Conexión rechazada";
        else if (httpCode == -11) errorMsg = "Timeout SSL (servidor tardó mucho)";
        result = "{\"success\":false,\"message\":\"" + errorMsg + "\"}";
    }
    
    http.end();
    delete client;
    
    Serial.printf("[GraphQL] Memoria libre después: %d bytes\n", ESP.getFreeHeap());
    
    return result;
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
    
    // Resolver DNS con retry
    IPAddress serverIP;
    if (!resolveBackendDNS(serverIP)) {
        Serial.println("[GraphQL] Error: No se pudo resolver DNS para obtener zonas");
        return "[]";
    }
    
    // Cliente WiFi seguro
    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) {
        Serial.println("[GraphQL] Error: No se pudo crear cliente SSL");
        return "[]";
    }
    
    client->setInsecure();
    client->setTimeout(90);  // Aumentado a 90 segundos
    
    HTTPClient http;
    http.setTimeout(60000);  // 60 segundos
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
                        Serial.printf("[GraphQL] Zonas obtenidas: %d\n", zoneCount);
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

// ==================== GraphQL - Actualizar Estado del Dispositivo ====================
bool WiFiManager::updateDispositivoStatus(int dispositivoId, const String& status, int batteryLevel) {
    Serial.printf("[GraphQL] Actualizando dispositivo %d - status: %s, battery: %d\n", dispositivoId, status.c_str(), batteryLevel);
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GraphQL] Error: WiFi no conectado - saltando actualizacion");
        return false;
    }
    
    // Resolver DNS con retry (pero con timeout corto)
    IPAddress serverIP;
    if (!backendIPCached) {
        Serial.println("[GraphQL] IP no en cache, intentando resolver DNS rapido...");
        // Solo 1 intento para no bloquear
        if (WiFi.hostByName("bovino-io-backend.onrender.com", serverIP)) {
            Serial.printf("[GraphQL] DNS resuelto: %s\n", serverIP.toString().c_str());
            cachedBackendIP = serverIP;
            backendIPCached = true;
        } else {
            Serial.println("[GraphQL] DNS no pudo resolverse - saltando actualizacion");
            return false;
        }
    } else {
        serverIP = cachedBackendIP;
        Serial.printf("[GraphQL] Usando IP en cache: %s\n", serverIP.toString().c_str());
    }
    
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10);  // Timeout corto de 10s
    
    HTTPClient http;
    http.setTimeout(12000);  // 12 segundos
    http.setReuse(false);
    
    const char* graphqlUrl = "https://bovino-io-backend.onrender.com/graphql";
    
    if (!http.begin(client, graphqlUrl)) {
        Serial.println("[GraphQL] Error: No se pudo iniciar conexión HTTPS");
        return false;
    }
    
    // Construir mutation
    String mutation = "{\"query\":\"mutation UpdateDispositivo($id: Int!, $input: UpdateDispositivoInput!){ updateDispositivo(id: $id, input: $input){ id status battery_level } }\",\"variables\":{\"id\":" + String(dispositivoId) + ",\"input\":{\"status\":\"" + status + "\",\"battery_level\":" + String(batteryLevel) + "}}}";
    
    Serial.printf("[GraphQL] Mutation: %s\n", mutation.c_str());
    
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(mutation);
    bool success = false;
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[GraphQL] Respuesta HTTP: %d\n", httpCode);
        Serial.printf("[GraphQL] Respuesta: %s\n", response.c_str());
        
        if (httpCode == 200) {
            const size_t capacity = JSON_OBJECT_SIZE(10) + 200;
            DynamicJsonDocument doc(capacity);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                if (doc.containsKey("data")) {
                    JsonObject dataObj = doc["data"];
                    if (dataObj.containsKey("updateDispositivo")) {
                        Serial.println("[GraphQL] Dispositivo actualizado correctamente");
                        success = true;
                    } else {
                        Serial.println("[GraphQL] Error: Campo 'updateDispositivo' no encontrado");
                    }
                } else if (doc.containsKey("errors")) {
                    Serial.println("[GraphQL] Error en mutation: respuesta con errores");
                } else {
                    Serial.println("[GraphQL] Error: Respuesta inválida");
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
    return success;
}

// ==================== GraphQL - Obtener Sublocalidades por Zona ====================
String WiFiManager::fetchSublocationsFromGraphQL(int zoneId) {
    Serial.printf("[GraphQL] Obteniendo sublocalidades de la zona %d...\n", zoneId);
    
    // Verificar conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[GraphQL] Error: WiFi no conectado");
        return "[]";
    }
    
    // Resolver DNS con retry
    IPAddress serverIP;
    if (!resolveBackendDNS(serverIP)) {
        Serial.println("[GraphQL] Error: No se pudo resolver DNS para obtener sublocalidades");
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
    String query = "{\"query\":\"query DevicesByZone($id_zone: Int!){ dispositivosByZone(id_zone: $id_zone){ id type location mac_address battery_level status zone { id name } } }\",\"variables\":{\"id_zone\":" + String(zoneId) + "}}";
    
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
                        
                        // Limpiar MAC detectada al inicio
                        detectedMasterMac = "";
                        
                        int deviceCount = 0;
                        for (JsonVariant item : devices) {
                            if (item.is<JsonObject>()) {
                                // FILTRO: Solo incluir dispositivos con status="pending"
                                String deviceStatus = "";
                                if (item.containsKey("status")) {
                                    deviceStatus = item["status"].as<String>();
                                }
                                
                                // Extraer tipo y MAC del dispositivo
                                String deviceType = "";
                                if (item.containsKey("type")) {
                                    deviceType = item["type"].as<String>();
                                }
                                
                                String deviceMac = "";
                                if (item.containsKey("mac_address")) {
                                    deviceMac = item["mac_address"].as<String>();
                                }
                                
                                // Guardar MAC del maestro si lo encontramos
                                if (deviceType == "master" && deviceMac.length() > 0) {
                                    detectedMasterMac = deviceMac;
                                    Serial.printf("[GraphQL] MAC del maestro detectado: %s\n", detectedMasterMac.c_str());
                                }
                                
                                // Solo agregar si el estado es "pending" (disponible)
                                if (deviceStatus == "pending") {
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
                        }
                        
                        result = "";
                        serializeJson(resultArray, result);
                        Serial.printf("[GraphQL] Sublocalidades obtenidas: %d\n", deviceCount);
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

String WiFiManager::saveDeviceLocation(const String& zoneName, const String& subLocation, int zoneId) {
    Preferences prefs;
    if (prefs.begin("location_cfg", false)) {
        prefs.putString("zone_name", zoneName);
        prefs.putString("sub_location", subLocation);
        prefs.putInt("zone_id", zoneId);
        prefs.end();
        Serial.printf("[Config] Ubicación guardada - Zona: %s (ID: %d), Sublocalización: %s\n", 
                     zoneName.c_str(), zoneId, subLocation.c_str());
        
        // Actualizar variable global
        LOADED_ZONE_ID = zoneId;
        
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
    
    // Obtener dirección MAC del ESP32
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Crear SSID con formato: AP_SSID_PREFIX_MAC
    String apSsid = String(AP_SSID_PREFIX) + "_" + String(macStr);

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
        dnsServerActive = true;  // Marcar DNS como activo
        Serial.printf("[Portal] DNS Server iniciado en puerto %d (Captive Portal activo)\n", DNS_PORT);
        Serial.printf("[Portal] Redirigiendo todas las DNS queries a %s\n", local_IP.toString().c_str());
    } else {
        dnsServerActive = false;
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

// Helper para agregar headers CORS a todas las respuestas
void WiFiManager::sendCORSHeaders() {
    configServer.sendHeader("Access-Control-Allow-Origin", "*");
    configServer.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    configServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void WiFiManager::setupPortalRoutes() {
    // Handler para peticiones OPTIONS (preflight CORS)
    configServer.onNotFound([this]() {
        if (configServer.method() == HTTP_OPTIONS) {
            sendCORSHeaders();
            configServer.send(200);
            return;
        }
        IPAddress clientIP = configServer.client().remoteIP();
        String path = configServer.uri();
        Serial.printf("[Portal] Ruta no encontrada desde %s: %s - Redirigiendo a /\n", 
                      clientIP.toString().c_str(), path.c_str());
        configServer.sendHeader("Location", "/");
        configServer.send(302);
    });
    
    // Ruta principal
    configServer.on("/", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET / desde %s\n", clientIP.toString().c_str());
        sendCORSHeaders();
        configServer.send(200, "text/html", renderPortalPage(""));
    });

    // Ruta para login de usuario
    configServer.on("/login", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /login desde %s\n", clientIP.toString().c_str());
        
        sendCORSHeaders();
        
        String email = configServer.arg("email");
        String password = configServer.arg("password");
        
        email.trim();
        password.trim();
        
        if (email.length() == 0 || password.length() == 0) {
            configServer.send(200, "application/json", "{\"success\":false,\"message\":\"Email y contraseña requeridos\"}");
            return;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            configServer.send(200, "application/json", "{\"success\":false,\"message\":\"WiFi no conectado\"}");
            return;
        }
        
        Serial.printf("[Portal] Intentando login: %s\n", email.c_str());
        
        // Liberar memoria antes de operación HTTPS
        Serial.printf("[Portal] Memoria libre antes de login: %d bytes\n", ESP.getFreeHeap());
        delay(500);  // Breve pausa para estabilizar conexión
        
        String loginResult = loginUser(email, password);
        
        Serial.printf("[Portal] Resultado login: %s\n", loginResult.c_str());
        
        configServer.send(200, "application/json", loginResult);
    });

    // Ruta para conectar WiFi y verificar conexión
    configServer.on("/connectWifi", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /connectWifi desde %s\n", clientIP.toString().c_str());
        
        sendCORSHeaders();
        
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
            Serial.println("\n[Portal] WiFi conectado exitosamente");
            Serial.printf("[Portal] IP obtenida: %s\n", WiFi.localIP().toString().c_str());
            
            // Detener DNS server para liberar memoria (ya no se necesita)
            dnsServer.stop();
            dnsServerActive = false;
            Serial.println("[Portal] DNS Server detenido (ya no se necesita)");
            
            // Mantener portal accesible durante configuración
            Serial.println("[Portal] IMPORTANTE: Portal sigue activo en ambas redes:");
            Serial.printf("[Portal]   - Access Point: http://%s\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("[Portal]   - Red WiFi: http://%s\n", WiFi.localIP().toString().c_str());
            Serial.println("[Portal] Si pierdes conexión, reconéctate al AP bovino_io_xxx");
            
            // Guardar credenciales WiFi
            saveCredentials(ssid, password);
            
            // Liberar memoria manualmente
            Serial.printf("[Portal] Memoria libre post-WiFi: %d bytes\n", ESP.getFreeHeap());
            
            configServer.send(200, "application/json", "{\"success\":true,\"message\":\"WiFi conectado\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"portalIP\":\"" + WiFi.softAPIP().toString() + "\"}");
        } else {
            Serial.println("\n[Portal] Error al conectar WiFi");
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);  // Volver a modo AP puro
            configServer.send(200, "application/json", "{\"success\":false,\"message\":\"No se pudo conectar al WiFi\"}");
        }
    });

    // Ruta para obtener zonas desde GraphQL
    configServer.on("/getZones", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET /getZones desde %s\n", clientIP.toString().c_str());
        
        sendCORSHeaders();
        
        // Verificar si hay usuario logueado
        if (loggedUserId == 0) {
            Serial.println("[Portal] No hay usuario logueado");
            configServer.send(200, "application/json", "{\"zones\":[],\"error\":\"Usuario no autenticado\"}");
            return;
        }
        
        // Verificar si WiFi está conectado
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Portal] WiFi NO conectado. No se pueden obtener zonas.");
            configServer.send(200, "application/json", "{\"zones\":[],\"error\":\"WiFi no conectado\"}");
            return;
        }
        
        Serial.printf("[Portal] WiFi conectado - IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[Portal] Usuario logueado - ID: %d\n", loggedUserId);
        
        // ⚡ CRÍTICO: Pequeño delay adicional para asegurar estabilidad de memoria
        // antes de la primera llamada HTTPS que requiere ~8-12KB de heap contiguo
        delay(500);
        
        Serial.println("[Portal] Llamando a fetchZonesFromGraphQL...");
        
        String zonesJson = fetchZonesFromGraphQL(loggedUserId);
        
        Serial.printf("[Portal] Zonas JSON recibido (longitud: %d): %s\n", zonesJson.length(), zonesJson.c_str());
        
        String response = "{\"zones\":" + zonesJson + "}";
        Serial.printf("[Portal] Enviando respuesta: %s\n", response.c_str());
        
        configServer.send(200, "application/json", response);
    });

    // Ruta para obtener sublocalidades por zona
    configServer.on("/getSublocations", HTTP_GET, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] GET /getSublocations desde %s\n", clientIP.toString().c_str());
        
        sendCORSHeaders();
        
        String zoneIdStr = configServer.arg("zone_id");
        int zoneId = zoneIdStr.toInt();
        
        if (zoneId == 0) {
            Serial.println("[Portal] zone_id invalido o no proporcionado");
            configServer.send(200, "application/json", "{\"sublocations\":[],\"error\":\"zone_id requerido\"}");
            return;
        }
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Portal] WiFi NO conectado");
            configServer.send(200, "application/json", "{\"sublocations\":[],\"error\":\"WiFi no conectado\"}");
            return;
        }
        
        Serial.printf("[Portal] Obteniendo sublocalidades para zona ID: %d\n", zoneId);
        
        String sublocationsJson = fetchSublocationsFromGraphQL(zoneId);
        
        Serial.printf("[Portal] Sublocalidades JSON (longitud: %d): %s\n", sublocationsJson.length(), sublocationsJson.c_str());
        
        String response = "{\"sublocations\":" + sublocationsJson + "}";
        Serial.printf("[Portal] Enviando respuesta: %s\n", response.c_str());
        
        configServer.send(200, "application/json", response);
    });

    // Manejador OPTIONS para CORS preflight en /save
    configServer.on("/save", HTTP_OPTIONS, [this]() {
        sendCORSHeaders();
        configServer.send(204);  // No Content
    });

    // Ruta para guardar configuración
    configServer.on("/save", HTTP_POST, [this]() {
        sendCORSHeaders();  // Agregar headers CORS
        
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /save desde %s\n", clientIP.toString().c_str());
        
        String deviceMode = configServer.arg("device_mode");
        String tempSSID = configServer.arg("temp_ssid");
        String tempPassword = configServer.arg("temp_password");
        String masterMac = configServer.arg("master_mac");
        String zoneName = configServer.arg("zone_name");
        String subLocation = configServer.arg("sub_location");
        String dispositivoIdStr = configServer.arg("dispositivo_id");
        String zoneIdStr = configServer.arg("zone_id");
        int zoneId = zoneIdStr.toInt();

        deviceMode.trim();
        tempSSID.trim();
        tempPassword.trim();
        masterMac.trim();
        zoneName.trim();
        subLocation.trim();

        // Validar zona y sublocalización (requeridas para TODOS)
        if (zoneName.length() == 0) {
            Serial.println("[Portal] Error: Zona requerida");
            configServer.send(200, "text/html", renderPortalPage("   Debes seleccionar una Zona."));
            return;
        }
        
        if (subLocation.length() == 0) {
            Serial.println("[Portal] Error: Sublocalización requerida");
            configServer.send(200, "text/html", renderPortalPage("   Debes seleccionar una Sublocalización."));
            return;
        }

        // Determinar el modo del dispositivo
        DeviceMode mode = (deviceMode == "master") ? DEVICE_MASTER : DEVICE_SLAVE;
        
        // Validación según el modo
        if (mode == DEVICE_MASTER) {
            // MAESTRO: Guardar WiFi persistentemente
            if (tempSSID.length() == 0) {
                Serial.println("[Portal] Error: MAESTRO requiere SSID");
                configServer.send(200, "text/html", renderPortalPage("   El modo MAESTRO requiere configurar WiFi."));
                return;
            }
            
            Serial.printf("[Portal] Configurando MAESTRO\n");
            Serial.printf("[Portal] WiFi SSID: %s (se guardará persistentemente)\n", tempSSID.c_str());
            Serial.printf("[Portal] Zona: %s (ID: %d), Sublocalización: %s\n", zoneName.c_str(), zoneId, subLocation.c_str());
            
            saveCredentials(tempSSID, tempPassword);  // GUARDAR WiFi para MAESTRO
            saveDeviceConfig(mode, "");  // MAESTRO no necesita MAC
            saveDeviceLocation(zoneName, subLocation, zoneId);  // Guardar ubicación con zone_id
            
            // Marcar que es primera ejecución después de configurar (para activar modo registro)
            Preferences prefsFirstRun;
            if (prefsFirstRun.begin("first_run", false)) {
                prefsFirstRun.putBool("pending", true);
                prefsFirstRun.end();
                Serial.println("[Portal] Flag de primera ejecución marcado");
            }
            
            // Respuesta HTML simple y rapida
            String successHTML = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
            successHTML += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f0f0;}";
            successHTML += ".success{background:#4CAF50;color:white;padding:20px;border-radius:8px;margin:20px auto;max-width:400px;}";
            successHTML += "h1{color:#333;}</style></head><body>";
            successHTML += "<h1>BovinoIOT</h1>";
            successHTML += "<div class='success'>";
            successHTML += "<h2>Configuracion Exitosa</h2>";
            successHTML += "<p><b>Modo:</b> MAESTRO</p>";
            successHTML += "<p><b>WiFi:</b> " + tempSSID + "</p>";
            successHTML += "<p><b>Zona:</b> " + zoneName + "</p>";
            successHTML += "<p><b>Sublocalidad:</b> " + subLocation + "</p>";
            successHTML += "<p style='margin-top:20px;'>El dispositivo se reiniciara en 3 segundos...</p>";
            successHTML += "</div></body></html>";
            
            // Enviar respuesta PRIMERO (HTML simple)
            sendCORSHeaders();
            configServer.send(200, "text/html", successHTML);
            configServer.client().flush();  // Asegurar que se envió
            delay(200);
            
            // Actualizar estado del dispositivo en la base de datos (DESPUES de enviar respuesta)
            int dispositivoId = dispositivoIdStr.toInt();
            if (dispositivoId > 0 && WiFi.status() == WL_CONNECTED) {
                Serial.printf("[Portal] Actualizando dispositivo %d a estado 'active'\n", dispositivoId);
                bool updated = updateDispositivoStatus(dispositivoId, "active", 100);
                if (updated) {
                    Serial.println("[Portal] Estado del dispositivo actualizado");
                } else {
                    Serial.println("[Portal] No se pudo actualizar el estado (se procedera de todos modos)");
                }
            } else if (dispositivoId > 0) {
                Serial.println("[Portal] WiFi no conectado - saltando actualizacion de estado");
            }
            
            delay(3000);
            ESP.restart();  // Reiniciar para aplicar configuración
            
        } else {
            // ESCLAVO: NO guardar WiFi (solo se usó temporalmente)
            // Usar la MAC del maestro detectada automáticamente
            if (detectedMasterMac.length() == 0 || detectedMasterMac.length() < 17) {
                Serial.println("[Portal] Error: No se encontró un maestro en la zona seleccionada");
                configServer.send(200, "text/html", renderPortalPage("Error: No se puede configurar un ESCLAVO porque no hay un MAESTRO en esta zona.<br><br>Por favor, configura primero un dispositivo como MAESTRO antes de agregar esclavos."));
                return;
            }
            
            masterMac = detectedMasterMac;
            Serial.printf("[Portal] Usando MAC del maestro detectada automaticamente: %s\n", masterMac.c_str());
            
            Serial.printf("[Portal] Configurando ESCLAVO\n");
            Serial.printf("[Portal] MAC Maestro (auto-detectada): %s\n", masterMac.c_str());
            Serial.printf("[Portal] Zona: %s (ID: %d), Sublocalizacion: %s\n", zoneName.c_str(), zoneId, subLocation.c_str());
            Serial.println("[Portal] WiFi NO se guardara (solo para configuracion inicial)");
            
            // NO llamar saveCredentials() - El esclavo NO guarda WiFi
            saveDeviceConfig(mode, masterMac);  // Guardar modo ESCLAVO + MAC del maestro
            saveDeviceLocation(zoneName, subLocation, zoneId);  // Guardar ubicación con zone_id
            
            // Respuesta HTML simple y rapida
            String successHTML = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
            successHTML += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f0f0;}";
            successHTML += ".success{background:#4CAF50;color:white;padding:20px;border-radius:8px;margin:20px auto;max-width:400px;}";
            successHTML += "h1{color:#333;}</style></head><body>";
            successHTML += "<h1>BovinoIOT</h1>";
            successHTML += "<div class='success'>";
            successHTML += "<h2>Configuracion Exitosa</h2>";
            successHTML += "<p><b>Modo:</b> ESCLAVO</p>";
            successHTML += "<p><b>MAC Maestro:</b> " + masterMac + "</p>";
            successHTML += "<p><b>Zona:</b> " + zoneName + "</p>";
            successHTML += "<p><b>Sublocalidad:</b> " + subLocation + "</p>";
            successHTML += "<p style='margin-top:20px;'>WiFi NO guardado (solo para configuracion)</p>";
            successHTML += "<p>El dispositivo se reiniciara en 3 segundos...</p>";
            successHTML += "</div></body></html>";
            
            // Enviar respuesta PRIMERO (HTML simple)
            sendCORSHeaders();
            configServer.send(200, "text/html", successHTML);
            configServer.client().flush();  // Asegurar que se envió
            delay(200);
            
            // Actualizar estado del dispositivo en la base de datos (DESPUES de enviar respuesta)
            int dispositivoId = dispositivoIdStr.toInt();
            if (dispositivoId > 0 && WiFi.status() == WL_CONNECTED) {
                Serial.printf("[Portal] Actualizando dispositivo %d a estado 'active'\n", dispositivoId);
                bool updated = updateDispositivoStatus(dispositivoId, "active", 100);
                if (updated) {
                    Serial.println("[Portal] Estado del dispositivo actualizado");
                } else {
                    Serial.println("[Portal] No se pudo actualizar el estado (se procedera de todos modos)");
                }
            } else if (dispositivoId > 0) {
                Serial.println("[Portal] WiFi no conectado - saltando actualizacion de estado");
            }
            
            delay(3000);
            ESP.restart();  // Reiniciar para aplicar configuración
        }   
    });
    
    // Captive Portal - Redirigir cualquier petición a la página principal
    configServer.on("/generate_204", HTTP_GET, [this]() {  // Android
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Android) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/gen_204", HTTP_GET, [this]() {  // Android alternativo
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Android gen_204) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/hotspot-detect.html", HTTP_GET, [this]() {  // iOS/macOS
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (iOS/macOS) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/library/test/success.html", HTTP_GET, [this]() {  // iOS alternativo
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (iOS library) desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/connecttest.txt", HTTP_GET, [this]() {  // Windows
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Windows) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
        // Devolver contenido INCORRECTO para forzar detección de captive portal
        configServer.send(200, "text/plain", "CAPTIVE PORTAL");
    });
    
    configServer.on("/ncsi.txt", HTTP_GET, [this]() {  // Windows 10
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Windows 10 ncsi) desde %s\n", clientIP.toString().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
        configServer.send(200, "text/plain", "CAPTIVE PORTAL");
    });
    
    configServer.on("/redirect", HTTP_GET, [this]() {  // Windows redirect
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Windows redirect) desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/canonical.html", HTTP_GET, [this]() {  // Ubuntu/Linux
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Ubuntu/Linux) desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/html", renderPortalPage(""));
    });
    
    configServer.on("/success.txt", HTTP_GET, [this]() {  // Firefox
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Captive Portal detectado (Firefox) desde %s\n", clientIP.toString().c_str());
        configServer.send(200, "text/plain", "success");
    });

    // Manejar cualquier ruta no encontrada
    configServer.onNotFound([this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] Ruta no encontrada desde %s: %s - Redirigiendo a /\n", 
                     clientIP.toString().c_str(), 
                     configServer.uri().c_str());
        configServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        configServer.sendHeader("Pragma", "no-cache");
        configServer.sendHeader("Expires", "-1");
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
    page += "h1{color:#2e7d32;font-size:24px;margin:0 0 10px;text-align:center;}";
    page += "p{color:#666;font-size:14px;margin:0 0 20px;text-align:center;}";
    page += "label{display:block;margin:15px 0 5px;font-weight:bold;color:#333;font-size:14px;}";
    page += "input{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;";
    page += "box-sizing:border-box;font-size:16px;}";
    page += "input:focus{outline:none;border-color:#4CAF50;}";
    page += "select{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;";
    page += "box-sizing:border-box;font-size:16px;background:#fff;cursor:pointer;}";
    page += "select:focus{outline:none;border-color:#4CAF50;}";
    page += ".radio-group{display:flex;gap:20px;margin:15px 0;justify-content:center;}";
    page += ".radio-option{display:flex;align-items:center;padding:10px 20px;";
    page += "border:2px solid #ddd;border-radius:6px;cursor:pointer;transition:all 0.3s;}";
    page += ".radio-option:hover{border-color:#4CAF50;background:#e8f5e9;}";
    page += ".radio-option input{margin-right:8px;cursor:pointer;}";
    page += ".radio-option label{margin:0;cursor:pointer;font-weight:bold;font-size:15px;}";
    page += ".radio-option input:checked + label{color:#4CAF50;}";
    page += "button{width:100%;margin-top:20px;padding:14px;background:#4CAF50;";
    page += "color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;font-weight:bold;}";
    page += "button:hover{background:#388E3C;}";
    page += ".status{margin-top:15px;padding:12px;border-radius:4px;background:#e8f5e9;";
    page += "color:#2e7d32;font-size:14px;text-align:center;}";
    page += ".info{background:#f5f5f5;color:#666;margin-top:15px;padding:10px;";
    page += "border-radius:4px;font-size:12px;text-align:center;}";
    page += ".mac-display{background:#e8f5e9;color:#2e7d32;padding:15px;border-radius:4px;";
    page += "text-align:center;font-family:monospace;font-size:16px;margin:10px 0;font-weight:bold;}";
    page += ".section{margin:20px 0;padding:20px;background:#f9f9f9;border-radius:6px;}";
    page += ".section-title{font-size:16px;font-weight:bold;color:#2e7d32;margin-bottom:10px;}";
    page += ".hidden{display:none !important;}";
    page += ".help-text{font-size:12px;color:#888;margin-top:5px;font-style:italic;}";
    page += "</style>";
    page += "<script>";
    page += "var currentStep=1;";
    page += "var wifiSSID='';";
    page += "var wifiPassword='';";
    page += "var loggedUserId=0;";
    page += "var loggedUserName='';";
    page += "var selectedZoneId=null;";
    page += "function showStep(step){";
    page += "console.log('showStep llamado con step',step);";
    page += "var steps=['step1','step2','step3'];";
    page += "steps.forEach(function(s){";
    page += "var el=document.getElementById(s);";
    page += "if(el){el.classList.add('hidden');console.log('Ocultando',s);}";
    page += "});";
    page += "var targetStep=document.getElementById('step'+step);";
    page += "if(targetStep){targetStep.classList.remove('hidden');console.log('Mostrando step',step);}";
    page += "else{console.error('No se encontro elemento step',step);}";
    page += "currentStep=step;";
    page += "}";
    page += "var portalBase='http://192.168.4.1';";
    page += "async function connectWifi(){";
    page += "var ssid=document.getElementById('ssid').value;";
    page += "var password=document.getElementById('password').value;";
    page += "if(!ssid){alert('Ingresa el SSID del WiFi');return;}";
    page += "wifiSSID=ssid;";
    page += "wifiPassword=password;";
    page += "document.getElementById('connect_btn').disabled=true;";
    page += "document.getElementById('connect_btn').innerText='Conectando...';";
    page += "try{";
    page += "var response=await fetch('/connectWifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(password)});";
    page += "var data=await response.json();";
    page += "if(data.success){";
    page += "portalBase='http://'+(data.portalIP||'192.168.4.1');";
    page += "console.log('Portal base URL:',portalBase);";
    page += "var statusMsg='<div style=\"color:green;font-weight:bold;margin:10px 0;padding:15px;background:#d4edda;border:1px solid #c3e6cb;border-radius:5px;\">';";
    page += "statusMsg+='WiFi Conectado<br>';";
    page += "statusMsg+='<small>Red WiFi IP: '+data.ip+'</small><br>';";
    page += "statusMsg+='<small>Portal IP: '+(data.portalIP||'192.168.4.1')+'</small><br>';";
    page += "statusMsg+='<small style=\"color:#856404;background:#fff3cd;padding:5px;display:inline-block;margin-top:5px;border-radius:3px;\">Si pierdes conexion, reconectate al AP bovino_io</small>';";
    page += "statusMsg+='</div>';";
    page += "document.getElementById('wifi_status').innerHTML=statusMsg;";
    page += "setTimeout(function(){showStep(2);},500);";
    page += "}else{";
    page += "alert('Error al conectar WiFi: '+data.message);";
    page += "document.getElementById('connect_btn').disabled=false;";
    page += "document.getElementById('connect_btn').innerText='Conectar WiFi';";
    page += "}";
    page += "}catch(error){";
    page += "alert('Error de conexión: '+error.message);";
    page += "document.getElementById('connect_btn').disabled=false;";
    page += "document.getElementById('connect_btn').innerText='Conectar WiFi';";
    page += "}";
    page += "}";
    page += "async function loginUser(){";
    page += "var email=document.getElementById('login_email').value;";
    page += "var password=document.getElementById('login_password').value;";
    page += "if(!email || !password){alert('Ingresa email y password');return;}";
    page += "document.getElementById('login_btn').disabled=true;";
    page += "document.getElementById('login_btn').innerText='Autenticando (puede tardar 90s)...';";
    page += "document.getElementById('login_status').innerHTML='<div style=\"color:#0056b3;font-size:12px;margin:5px 0;\">Conectando al servidor (puede tardar hasta 90 segundos si esta inactivo)...</div>';";
    page += "try{";
    page += "var loginUrl=(typeof portalBase!=='undefined'?portalBase:'http://192.168.4.1')+'/login';";
    page += "console.log('Login POST URL:',loginUrl);";
    page += "var response=await fetch(loginUrl,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'email='+encodeURIComponent(email)+'&password='+encodeURIComponent(password)});";
    page += "console.log('Login response status:',response.status);";
    page += "var data=await response.json();";
    page += "console.log('Login response data:',data);";
    page += "if(data.success){";
    page += "console.log('Login exitoso, user_id:',data.user_id);";
    page += "loggedUserId=data.user_id;";
    page += "loggedUserName=data.name;";
    page += "document.getElementById('login_status').innerHTML='<div style=\"color:green;font-weight:bold;margin:10px 0;\">Bienvenido '+data.name+'</div>';";
    page += "console.log('Cargando zonas...');";
    page += "await loadZones();";
    page += "console.log('Zonas cargadas, mostrando step 3');";
    page += "showStep(3);";
    page += "}else{";
    page += "alert('Error: '+data.message);";
    page += "document.getElementById('login_btn').disabled=false;";
    page += "document.getElementById('login_btn').innerText='Iniciar Sesion';";
    page += "}";
    page += "}catch(error){";
    page += "console.error('loginUser error:',error);";
    page += "var el=document.getElementById('login_status');if(el)el.innerHTML='<div style=\"color:#b94a48;font-weight:bold;margin:10px 0;\">Error de conexion: '+error.message+'<br>Intenta reconectar al AP: http://192.168.4.1</div>';";
    page += "document.getElementById('login_btn').disabled=false;";
    page += "document.getElementById('login_btn').innerText='Iniciar Sesion';";
    page += "}";
    page += "}";
    page += "async function loadZones(){";
    page += "console.log('loadZones iniciado');";
    page += "var zoneSelect=document.getElementById('zone_name');";
    page += "if(zoneSelect)zoneSelect.innerHTML='<option value=\"\">Cargando zonas...</option>';";
    page += "try{";
    page += "var zonesUrl=(typeof portalBase!=='undefined'?portalBase:'http://192.168.4.1')+'/getZones';";
    page += "console.log('Fetching zones from:',zonesUrl);";
    page += "var response=await fetch(zonesUrl);";
    page += "console.log('getZones response status:',response.status);";
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
    page += "console.log('Zonas cargadas:',data.zones.length);";
    page += "}else{";
    page += "console.warn('No hay zonas');";
    page += "optionsHTML='<option value=\"\">-- No hay zonas disponibles --</option>';";
    page += "}";
    page += "if(zoneSelect){zoneSelect.innerHTML=optionsHTML;}";
    page += "}catch(error){";
    page += "console.error('Error loadZones:',error);";
    page += "var errorHTML='<option value=\"\">-- Error: '+error.message+'</option>';";
    page += "if(zoneSelect)zoneSelect.innerHTML=errorHTML;";
    page += "}";
    page += "}";
    page += "function onZoneChange(){";
    page += "var zoneSelect=document.getElementById('zone_name');";
    page += "var selectedOption=zoneSelect.options[zoneSelect.selectedIndex];";
    page += "var zoneId=selectedOption.getAttribute('data-zone-id');";
    page += "if(zoneId){";
    page += "console.log('Zona seleccionada:',zoneId);";
    page += "document.getElementById('zone_id').value=zoneId;";
    page += "loadSublocations(zoneId);";
    page += "}";
    page += "}";
    page += "async function loadSublocations(zoneId){";
    page += "console.log('loadSublocations iniciado con zoneId:',zoneId);";
    page += "var subSelect=document.getElementById('sub_location');";
    page += "if(subSelect)subSelect.innerHTML='<option value=\"\">Cargando sondeadores...</option>';";
    page += "var deviceInfo=document.getElementById('device_type_info');";
    page += "if(deviceInfo)deviceInfo.style.display='none';";
    page += "var slaveMacSection=document.getElementById('slave_mac_section');";
    page += "if(slaveMacSection)slaveMacSection.style.display='none';";
    page += "try{";
    page += "var subUrl=(typeof portalBase!=='undefined'?portalBase:'http://192.168.4.1')+'/getSublocations?zone_id='+zoneId;";
    page += "console.log('Fetching sublocations from:',subUrl);";
    page += "var response=await fetch(subUrl);";
    page += "console.log('getSublocations response status:',response.status);";
    page += "if(!response.ok){throw new Error('Error HTTP '+response.status);}";
    page += "var data=await response.json();";
    page += "console.log('Respuesta /getSublocations:',data);";
    page += "var optionsHTML='<option value=\"\">-- Selecciona un sondeador --</option>';";
    page += "if(data.sublocations && Array.isArray(data.sublocations) && data.sublocations.length>0){";
    page += "data.sublocations.forEach(function(subloc){";
    page += "if(subloc && subloc.name){";
    page += "optionsHTML+='<option value=\"'+subloc.name+'\" data-device-id=\"'+(subloc.id||'')+'\">'+subloc.name+'</option>';";
    page += "}";
    page += "});";
    page += "console.log('Sondeadores cargados:',data.sublocations.length);";
    page += "}else{";
    page += "console.warn('No hay sondeadores');";
    page += "optionsHTML='<option value=\"\">-- No hay sondeadores en esta zona --</option>';";
    page += "}";
    page += "if(subSelect)subSelect.innerHTML=optionsHTML;";
    page += "}catch(error){";
    page += "console.error('Error loadSublocations:',error);";
    page += "var errorHTML='<option value=\"\">-- Error: '+error.message+'</option>';";
    page += "if(subSelect)subSelect.innerHTML=errorHTML;";
    page += "}";
    page += "}";
    page += "function onSubLocationChange(){";
    page += "var subSelect=document.getElementById('sub_location');";
    page += "var subLocation=subSelect.value;";
    page += "if(!subLocation){return;}";
    page += "var selectedOption=subSelect.options[subSelect.selectedIndex];";
    page += "var deviceId=selectedOption.getAttribute('data-device-id');";
    page += "if(deviceId){";
    page += "document.getElementById('dispositivo_id').value=deviceId;";
    page += "console.log('Device ID guardado:',deviceId);";
    page += "}";
    page += "var parts=subLocation.split('/');";
    page += "if(parts.length<2){return;}";
    page += "var deviceType=parts[0].toLowerCase();";
    page += "var deviceName=parts[1];";
    page += "document.getElementById('device_mode').value=deviceType;";
    page += "document.getElementById('temp_ssid').value=wifiSSID;";
    page += "document.getElementById('temp_password').value=wifiPassword;";
    page += "var infoDiv=document.getElementById('device_type_info');";
    page += "var infoText=document.getElementById('device_type_text');";
    page += "if(deviceType==='master'){";
    page += "infoText.innerText='Tipo: MAESTRO - Usará WiFi y enviará datos al servidor';";
    page += "infoDiv.style.display='block';";
    page += "}else if(deviceType==='slave'){";
    page += "infoText.innerText='Tipo: ESCLAVO - Se comunicará con el maestro vía ESP-NOW';";
    page += "infoDiv.style.display='block';";
    page += "}else{";
    page += "infoDiv.style.display='none';";
    page += "}";
    page += "}";
    page += "function saveConfig(event){";
    page += "event.preventDefault();";
    page += "console.log('Guardando configuracion...');";
    page += "var form=document.getElementById('config_form');";
    page += "var formData=new FormData(form);";
    page += "var saveBtn=document.getElementById('save_btn');";
    page += "saveBtn.disabled=true;";
    page += "saveBtn.innerText='Guardando...';";
    page += "var baseUrl=portalBase||'http://192.168.4.1';";
    page += "console.log('Enviando a:',baseUrl+'/save');";
    page += "fetch(baseUrl+'/save',{";
    page += "method:'POST',";
    page += "headers:{'Content-Type':'application/x-www-form-urlencoded'},";
    page += "body:new URLSearchParams(formData)";
    page += "}).then(function(response){";
    page += "console.log('Respuesta recibida:',response.status);";
    page += "if(response.ok){";
    page += "return response.text();";
    page += "}";
    page += "throw new Error('Error al guardar (status: '+response.status+')');";
    page += "}).then(function(html){";
    page += "console.log('HTML recibido, actualizando pagina');";
    page += "document.body.innerHTML=html;";
    page += "}).catch(function(error){";
    page += "console.error('Error al guardar:',error);";
    page += "alert('Configuracion guardada en el dispositivo. Se reiniciara en breve.');";
    page += "});";
    page += "}";
    page += "window.addEventListener('DOMContentLoaded',function(){";
    page += "var form=document.getElementById('config_form');";
    page += "if(form){";
    page += "form.addEventListener('submit',saveConfig);";
    page += "}";
    page += "});";
    page += "</script>";
    page += "</head><body>";
    page += "<div class='container'>";
    page += "<h1>BovinoIOT</h1>";
    page += "<p>Configuración Inicial del Dispositivo</p>";
    
    // PASO 1: Conexión WiFi (TODOS)
    page += "<div id='step1'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>Paso 1: Conectar a WiFi</div>";
    page += "<p class='help-text'>Necesitas WiFi temporalmente para obtener las zonas del servidor</p>";
    page += "<label for='ssid'>Nombre de Red WiFi (SSID)</label>";
    page += "<input type='text' id='ssid' maxlength='32' placeholder='Mi_Red_WiFi'>";
    page += "<label for='password'>Contraseña WiFi</label>";
    page += "<input type='password' id='password' maxlength='64' placeholder='Dejar vacío si es red abierta'>";
    page += "<div id='wifi_status'></div>";
    page += "<button type='button' id='connect_btn' onclick='connectWifi()'>Conectar WiFi</button>";
    page += "</div>";
    page += "</div>";
    
    // PASO 2: Login de usuario (NUEVO)
    page += "<div id='step2' class='hidden'>";
    page += "<div class='section'>";
    page += "<div class='section-title'>Paso 2: Iniciar Sesión</div>";
    page += "<p class='help-text'>Ingresa tus credenciales para acceder a tus zonas</p>";
    page += "<label for='login_email'>Email</label>";
    page += "<input type='email' id='login_email' placeholder='usuario@ejemplo.com'>";
    page += "<label for='login_password'>Contraseña</label>";
    page += "<input type='password' id='login_password' placeholder='Tu contraseña'>";
    page += "<div id='login_status'></div>";
    page += "<button type='button' id='login_btn' onclick='loginUser()'>Iniciar Sesión</button>";
    page += "</div>";
    page += "</div>";
    
    // PASO 3: Configuración del dispositivo (UNIFICADO - determina tipo automáticamente)
    page += "<div id='step3' class='hidden'>";
    page += "<form id='config_form' method='POST' action='/save'>";
    page += "<input type='hidden' id='device_mode' name='device_mode' value=''>";
    page += "<input type='hidden' id='temp_ssid' name='temp_ssid' value=''>";
    page += "<input type='hidden' id='temp_password' name='temp_password' value=''>";
    page += "<input type='hidden' id='dispositivo_id' name='dispositivo_id' value=''>";
    page += "<input type='hidden' id='zone_id' name='zone_id' value='0'>";  // Nuevo campo
    page += "<div class='section'>";
    page += "<div class='section-title'>Paso 3: Configuración del Dispositivo</div>";
    page += "<div class='mac-display'>" + localMacStr + "</div>";
    page += "<p class='help-text'>Tu dirección MAC</p>";
    page += "</div>";
    page += "<div class='section'>";
    page += "<div class='section-title'>Ubicación del Dispositivo</div>";
    page += "<label for='zone_name'>Zona</label>";
    page += "<select id='zone_name' name='zone_name' required onchange='onZoneChange()'>";
    page += "<option value=''>-- Selecciona una Zona --</option>";
    page += "</select>";
    page += "<label for='sub_location'>Seleccionar sondeador</label>";
    page += "<select id='sub_location' name='sub_location' required onchange='onSubLocationChange()'>";
    page += "<option value=''>-- Selecciona un sondeador --</option>";
    page += "</select>";
    page += "<div id='device_type_info' style='margin-top:15px;padding:10px;background:#e3f2fd;border-radius:4px;display:none;'>";
    page += "<p style='margin:0;font-weight:bold;color:#1976d2;' id='device_type_text'></p>";
    page += "</div>";
    page += "</div>";
    page += "<button type='submit' id='save_btn'>Guardar / Activar dispositivo</button>";
    page += "</form>";
    page += "</div>";

    if (message.length() > 0) {
        page += "<div class='status'>" + message + "</div>";
    }

    page += "<div class='info'>IP: " + WiFi.softAPIP().toString() + "<br>";
    String macAddr = WiFi.macAddress();
    macAddr.replace(":", "");
    page += "MAC address: " + macAddr + "</div>";
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
