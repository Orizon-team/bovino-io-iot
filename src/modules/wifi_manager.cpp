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
            reconnectRequestTime(0) {
}

// ==================== Inicialización ====================
void WiFiManager::begin() {
    loadStoredCredentials();

    if (ENABLE_WIFI_PORTAL) {
        startConfigPortal();
    }
}

bool WiFiManager::connect() {
    if (currentSSID.length() == 0) {
        Serial.println("[WiFi] SSID vacio. Configure credenciales desde el portal web.");
        return false;
    }

    Serial.println("\n[WiFi] Iniciando conexion WiFi...");
    Serial.printf("[WiFi] SSID: %s\n", currentSSID.c_str());

    displayManager.showWiFiStatus(true, currentSSID.c_str());

    WiFi.disconnect(false);
    delay(100);

    WiFi.mode(portalActive ? WIFI_AP_STA : WIFI_STA);
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
        return true;
    }

    Serial.println("\n[WiFi] [ERROR] No se pudo conectar");
    displayManager.showWiFiError();
    alertManager.showError();
    delay(2000);

    wasConnected = false;

    if (ENABLE_WIFI_PORTAL && !portalActive) {
        startConfigPortal();
    }

    return false;
}

bool WiFiManager::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool WiFiManager::reconnect() {
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

// ==================== Loop del Portal ====================
void WiFiManager::loop() {
    if (portalActive) {
        configServer.handleClient();

        if (lastPortalAnnounce == 0 || millis() - lastPortalAnnounce > 30000) {
            lastPortalAnnounce = millis();
            Serial.printf("[Portal] Disponible en http://%s\n", WiFi.softAPIP().toString().c_str());
        }
    }

    if (pendingReconnect && millis() - reconnectRequestTime > 500) {
        pendingReconnect = false;
        if (ENABLE_WIFI_SYNC) {
            Serial.println("[Portal] Intentando conectar con nuevas credenciales...");
            connect();
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
        return;
    }

    String apSsid = String("BovinoIOT-") + DEVICE_ID;

    WiFi.mode(WIFI_AP_STA);

    bool apStarted;
    if (CONFIG_PORTAL_PASSWORD != nullptr && strlen(CONFIG_PORTAL_PASSWORD) >= 8) {
        apStarted = WiFi.softAP(apSsid.c_str(), CONFIG_PORTAL_PASSWORD);
    } else {
        apStarted = WiFi.softAP(apSsid.c_str());
    }

    if (!apStarted) {
        Serial.println("[Portal] [ERROR] No se pudo iniciar el portal de configuracion");
        return;
    }

    setupPortalRoutes();
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
    Serial.println("[Portal] =================================");
}

void WiFiManager::setupPortalRoutes() {
    configServer.on("/", HTTP_GET, [this]() {
        configServer.send(200, "text/html", renderPortalPage(""));
    });

    configServer.on("/save", HTTP_POST, [this]() {
        String ssid = configServer.arg("ssid");
        String password = configServer.arg("password");

        ssid.trim();
        password.trim();

        if (ssid.length() == 0) {
            configServer.send(200, "text/html", renderPortalPage("SSID requerido."));
            return;
        }

        saveCredentials(ssid, password);
        configServer.send(200, "text/html", renderPortalPage("Credenciales guardadas. El dispositivo intentara conectarse."));

        pendingReconnect = true;
        reconnectRequestTime = millis();
    });

    configServer.onNotFound([this]() {
        configServer.send(404, "text/plain", "Ruta no encontrada");
    });
}

String WiFiManager::renderPortalPage(const String& statusMessage) {
    String message = statusMessage;
    String page;

    page  = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    page += "<title>BovinoIOT WiFi Setup</title>";
    page += "<style>body{font-family:Arial,sans-serif;background:#f2f2f2;margin:0;padding:0;}";
    page += ".container{max-width:420px;margin:40px auto;background:#fff;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.1);padding:24px;}";
    page += "h1{font-size:22px;margin-bottom:12px;text-align:center;}";
    page += "label{display:block;margin-top:16px;font-weight:bold;}";
    page += "input{width:100%;padding:10px;margin-top:6px;border:1px solid #ccc;border-radius:4px;}";
    page += "button{width:100%;margin-top:20px;padding:12px;background:#1e88e5;color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;}";
    page += "button:hover{background:#1565c0;}";
    page += ".status{margin-top:16px;padding:10px;border-radius:4px;background:#e3f2fd;color:#0d47a1;font-size:14px;}";
    page += "</style></head><body><div class='container'>";
    page += "<h1>BovinoIOT - Portal WiFi</h1>";
    page += "<p>Ingrese las credenciales de la red WiFi para el dispositivo maestro.</p>";
    page += "<form method='POST' action='/save'>";
    page += "<label for='ssid'>SSID</label>";
    page += "<input type='text' id='ssid' name='ssid' maxlength='32' value='" + currentSSID + "' required>";
    page += "<label for='password'>Password</label>";
    page += "<input type='password' id='password' name='password' maxlength='64' value='" + currentPassword + "'>";
    page += "<button type='submit'>Guardar y conectar</button>";
    page += "</form>";

    if (message.length() > 0) {
        page += "<div class='status'>" + message + "</div>";
    }

    page += "<div class='status'>IP portal: http://" + WiFi.softAPIP().toString() + "</div>";
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
