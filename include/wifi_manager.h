#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
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
    void setupPortalRoutes();
    String renderPortalPage(const String& statusMessage);
    bool attemptConnection(unsigned long timeout);
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
