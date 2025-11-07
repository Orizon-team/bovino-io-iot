#include "wifi_manager.h"
#include "alerts.h"
#include "display_manager.h"
#include <Preferences.h>
#include <cstring>

// Definición de la instancia global
WiFiManager wifiManager;

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

    // Ruta para guardar configuración
    configServer.on("/save", HTTP_POST, [this]() {
        IPAddress clientIP = configServer.client().remoteIP();
        Serial.printf("[Portal] POST /save desde %s\n", clientIP.toString().c_str());
        String ssid = configServer.arg("ssid");
        String password = configServer.arg("password");

        ssid.trim();
        password.trim();

        if (ssid.length() == 0) {
            Serial.println("[Portal] Error: SSID vacio");
            configServer.send(200, "text/html", renderPortalPage("SSID requerido."));
            return;
        }

        Serial.printf("[Portal] Guardando credenciales - SSID: %s\n", ssid.c_str());
        saveCredentials(ssid, password);
        configServer.send(200, "text/html", renderPortalPage("Credenciales guardadas. El dispositivo intentara conectarse."));

        pendingReconnect = true;
        reconnectRequestTime = millis();
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

    page  = "<!DOCTYPE html><html lang='es'><head>";
    page += "<meta charset='UTF-8'>";
    page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    page += "<title>BovinoIOT WiFi</title>";
    page += "<style>";
    page += "body{font-family:Arial,sans-serif;background:#f5f5f5;margin:0;padding:20px;}";
    page += ".container{max-width:400px;margin:0 auto;background:#fff;border-radius:8px;";
    page += "box-shadow:0 2px 10px rgba(0,0,0,0.1);padding:30px;}";
    page += "h1{color:#1976d2;font-size:24px;margin:0 0 10px;text-align:center;}";
    page += "p{color:#666;font-size:14px;margin:0 0 20px;text-align:center;}";
    page += "label{display:block;margin:15px 0 5px;font-weight:bold;color:#333;}";
    page += "input{width:100%;padding:12px;border:1px solid #ddd;border-radius:4px;";
    page += "box-sizing:border-box;font-size:16px;}";
    page += "input:focus{outline:none;border-color:#1976d2;}";
    page += "button{width:100%;margin-top:20px;padding:14px;background:#1976d2;";
    page += "color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;font-weight:bold;}";
    page += "button:hover{background:#1565c0;}";
    page += ".status{margin-top:15px;padding:12px;border-radius:4px;background:#e3f2fd;";
    page += "color:#0d47a1;font-size:14px;text-align:center;}";
    page += ".info{background:#f5f5f5;color:#666;margin-top:15px;padding:10px;";
    page += "border-radius:4px;font-size:12px;text-align:center;}";
    page += "</style></head><body>";
    page += "<div class='container'>";
    page += "<h1>🐄 BovinoIOT</h1>";
    page += "<p>Configuración de Red WiFi</p>";
    page += "<form method='POST' action='/save'>";
    page += "<label for='ssid'>Nombre de Red (SSID)</label>";
    page += "<input type='text' id='ssid' name='ssid' maxlength='32' placeholder='Mi_Red_WiFi' required>";
    page += "<label for='password'>Contraseña</label>";
    page += "<input type='password' id='password' name='password' maxlength='64' placeholder='Dejar vacío si es red abierta'>";
    page += "<button type='submit'>💾 Guardar y Conectar</button>";
    page += "</form>";

    if (message.length() > 0) {
        page += "<div class='status'>" + message + "</div>";
    }

    page += "<div class='info'>IP: " + WiFi.softAPIP().toString() + "<br>";
    page += "Dispositivo: " + String(DEVICE_ID) + "</div>";
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
