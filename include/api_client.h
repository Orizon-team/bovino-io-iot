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
    
    bool sendDetections(const std::map<String, BeaconData>& beacons);
    String checkBeaconStatus(const String& macAddress);
    std::map<String, String> checkMultipleBeaconStatus(const std::vector<String>& macAddresses);

private:
    String createDetectionsPayload(const std::map<String, BeaconData>& beacons);
    bool handleResponse(int httpCode, const String& response);
    bool shouldRetry(int httpCode);
    unsigned long getRetryDelay(int httpCode);
};

extern APIClient apiClient;

#endif
