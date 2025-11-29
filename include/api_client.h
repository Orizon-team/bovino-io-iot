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
    bool initializeTimeSync();                                  // Sincroniza hora con NTP
    String getCurrentTimestamp();                               // Obtiene timestamp actual como String
    time_t getCurrentEpoch();                                   // Obtiene epoch actual
    
    bool sendDetections(const std::map<String, BeaconData>& beacons);   // Envía detecciones a la API
    String checkBeaconStatus(const String& macAddress);         // Consulta status de beacon (unregistered/active/unknown)
    std::map<String, String> checkMultipleBeaconStatus(const std::vector<String>& macAddresses);  // Consulta status de múltiples beacons

private:
    String createDetectionsPayload(const std::map<String, BeaconData>& beacons);   // Crea payload JSON
    bool handleResponse(int httpCode, const String& response);  // Maneja respuesta HTTP
    bool shouldRetry(int httpCode);                             // Determina si reintentar
    unsigned long getRetryDelay(int httpCode);                  // Obtiene delay de reintento
};

extern APIClient apiClient;

#endif // API_CLIENT_H
