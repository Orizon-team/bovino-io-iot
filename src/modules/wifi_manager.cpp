#include "wifi_manager.h"
#include "alerts.h"
#include "display_manager.h"

// Definición de la instancia global
WiFiManager wifiManager;

// ==================== Constructor ====================
WiFiManager::WiFiManager() : lastConnectionAttempt(0), wasConnected(false) {
}

// ==================== Conexión WiFi ====================
bool WiFiManager::connect() {
    Serial.println("\n[WiFi] Iniciando conexión WiFi...");
    Serial.printf("[WiFi] SSID: %s\n", WIFI_SSID);

    // Mostrar en la pantalla LCD
    displayManager.showWiFiStatus(true, WIFI_SSID);

    // Desconectar cualquier conexión previa
    WiFi.disconnect(true);
    delay(100);

    // Iniciar conexión
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Intentar la conexión con timeout
    bool connected = attemptConnection(WIFI_TIMEOUT);

    if (connected) {
        Serial.println("\n[WiFi] ✓ Conexión exitosa");
        Serial.printf("[WiFi] IP Local: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());

        // Mostrar IP en la pantalla
        displayManager.showIP(getLocalIP());
        alertManager.showSuccess();
        delay(2000);

        wasConnected = true;
        return true;
    } 
    else {
        Serial.println("\n[WiFi] ❌ Error: No se pudo conectar");
        
        displayManager.showWiFiError();
        alertManager.showError();
        delay(2000);

        wasConnected = false;
        return false;
    }
}

// ==================== Verificación de Estado ====================
bool WiFiManager::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

// ==================== Reconexión ====================
bool WiFiManager::reconnect() {
    Serial.println("[WiFi] Intentando reconexión...");
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
    WiFi.disconnect(true);
    wasConnected = false;
}

// ==================== Métodos Privados ====================
bool WiFiManager::attemptConnection(unsigned long timeout) {
    unsigned long startTime = millis();
    unsigned long lastToggle = millis();
    
    // Esperar hasta conectar o timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
        // Alternar LED de carga cada 250ms
        if (millis() - lastToggle >= 250) {
            alertManager.loaderToggle();
            lastToggle = millis();
            Serial.print(".");
        }
        delay(50);
    }

    // Apagar LED de carga
    alertManager.loaderOff();

    // Retornar el estado de conexión
    return (WiFi.status() == WL_CONNECTED);
}
