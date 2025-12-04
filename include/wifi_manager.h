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
    void startConfigPortal();
    bool isPortalActive() const { return portalActive; }
    int getConnectionAttempts() const { return connectionAttempts; }
    bool isConfigured();
    void clearAllConfig();
    String fetchZonesFromGraphQL(int userId);
    String fetchSublocationsFromGraphQL(int zoneId);
    String saveDeviceLocation(const String& zoneName, const String& subLocation, int zoneId);
    bool updateDispositivoStatus(int dispositivoId, const String& status, int batteryLevel);

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
    int loggedUserId;
    String detectedMasterMac;
    bool dnsServerActive;
    IPAddress cachedBackendIP;
    bool backendIPCached;
    
    void loadStoredCredentials();
    bool resolveBackendDNS(IPAddress& ip);
    String loginUser(const String& email, const String& password);
    void saveCredentials(const String& ssid, const String& password);
    void loadDeviceConfig();
    void loadDeviceLocation();
    void saveDeviceConfig(DeviceMode mode, const String& masterMac);
    void setupPortalRoutes();
    void sendCORSHeaders();
    String renderPortalPage(const String& statusMessage);
    bool attemptConnection(unsigned long timeout);
    String macToString(const uint8_t* mac);
    void stringToMac(const String& macStr, uint8_t* mac);
};

extern WiFiManager wifiManager;

#endif
