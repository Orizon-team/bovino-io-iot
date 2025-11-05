#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include "config.h"

class APIClient {
public:
    APIClient();
    bool initializeTimeSync();
    String getCurrentTimestamp();
    bool checkDeviceRegistration(const char* deviceId);
    bool registerDevice(const char* deviceId, const char* location, ZoneType zoneType);

private:
    bool handleResponse(int httpCode, const String& response);
    bool shouldRetry(int httpCode);
    unsigned long getRetryDelay(int httpCode);
};

extern APIClient apiClient;

#endif // API_CLIENT_H
