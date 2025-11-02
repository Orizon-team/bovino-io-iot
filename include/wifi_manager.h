#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiManager {
public:
    WiFiManager();
    bool connect();
    bool isConnected();
    bool reconnect();
    String getLocalIP();
    int getRSSI();
    void disconnect();

private:
    unsigned long lastConnectionAttempt;
    bool wasConnected;
    bool attemptConnection(unsigned long timeout);
};

extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
