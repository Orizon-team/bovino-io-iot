#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class WiFiManager {
public:
    WiFiManager();
    void begin();
    bool connect();
    bool isConnected();
    bool reconnect();
    String getLocalIP();
    int getRSSI();
    void disconnect();
    void loop();
    const String& getConfiguredSSID() const { return currentSSID; }
    void stopConfigPortal();
    void startConfigPortal();  // Hacerlo público
    bool isPortalActive() const { return portalActive; }
    int getConnectionAttempts() const { return connectionAttempts; }  // Nuevo método
    bool isConfigured();  // Verificar si está configurado (WiFi para MASTER, MAC para SLAVE)
    void clearAllConfig();  // Limpiar toda la configuración
    String fetchZonesFromGraphQL(int userId);  // Obtener zonas desde GraphQL como JSON
    String fetchSublocationsFromGraphQL(int zoneId);  // Obtener sublocalidades por zona
    String saveDeviceLocation(const String& zoneName, const String& subLocation);  // Guardar ubicación

private:
    unsigned long lastConnectionAttempt;
    bool wasConnected;
    WebServer configServer;
    DNSServer dnsServer;
    bool portalActive;
    bool pendingReconnect;
    unsigned long lastPortalAnnounce;
    unsigned long reconnectRequestTime;
    String currentSSID;
    String currentPassword;
    int connectionAttempts;
    
    void loadStoredCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void loadDeviceConfig();
    void loadDeviceLocation();
    void saveDeviceConfig(DeviceMode mode, const String& masterMac);
    void setupPortalRoutes();
    String renderPortalPage(const String& statusMessage);
    bool attemptConnection(unsigned long timeout);
    String macToString(const uint8_t* mac);
    void stringToMac(const String& macStr, uint8_t* mac);
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
