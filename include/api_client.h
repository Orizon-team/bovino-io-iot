#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <map>
#include "config.h"

class APIClient {
public:
    APIClient();
    bool initializeTimeSync();
    String getCurrentTimestamp();
    time_t getCurrentEpoch();
    
    // Envío de detecciones a la API real
    bool sendDetections(const std::map<uint32_t, BeaconData>& beacons);

private:
    String createDetectionsPayload(const std::map<uint32_t, BeaconData>& beacons);
    bool handleResponse(int httpCode, const String& response);
    bool shouldRetry(int httpCode);
    unsigned long getRetryDelay(int httpCode);
};

extern APIClient apiClient;

#endif // API_CLIENT_H
