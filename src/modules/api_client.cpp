#include "api_client.h"
#include "wifi_manager.h"
#include "alerts.h"
#include "display_manager.h"

APIClient apiClient;

APIClient::APIClient() {
}
bool APIClient::initializeTimeSync() {
    Serial.println("[NTP] Sincronizando tiempo con servidores NTP...");
    
    configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
    
    time_t now = time(nullptr);
    int attempts = 0;
    
    while (now < 8 * 3600 * 2 && attempts < NTP_TIMEOUT_SECONDS) {
        delay(500);
        Serial.print(".");
        now = time(nullptr);
        attempts++;
    }
    
    if (now >= 8 * 3600 * 2) {
        Serial.println("\n[NTP]     Tiempo sincronizado correctamente");
        return true;
    } else {
        Serial.println("\n[NTP]     Advertencia: Tiempo no sincronizado");
        return false;
    }
}

time_t APIClient::getCurrentEpoch() {
    return time(NULL);
}

String APIClient::getCurrentTimestamp() {
    time_t now = time(NULL);
    now -= 6 * 3600;  // Ajuste de zona horaria (GMT-6)
    
    struct tm* timeinfo = gmtime(&now);
    char isoTime[25];
    strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    
    return String(isoTime);
}

// ==================== Manejo de Respuestas ====================
bool APIClient::handleResponse(int httpCode, const String& response) {
    // Códigos de éxito (2xx)
    if (httpCode >= 200 && httpCode < 300) {
        return true;
    }
    
    return false;
}

bool APIClient::shouldRetry(int httpCode) {
    // Errores del servidor (5xx) son recuperables
    if (httpCode >= 500 && httpCode < 600) {
        return true;
    }
    
    // 404, 408, 429 también pueden ser recuperables
    if (httpCode == 404 || httpCode == 408 || httpCode == 429) {
        return true;
    }
    
    return false;
}

unsigned long APIClient::getRetryDelay(int httpCode) {
    switch (httpCode) {
        case 502:  // Bad Gateway
        case 503:  // Service Unavailable
            return 5000;  // 5 segundos
        
        case 504:  // Gateway Timeout
            return 3000;  // 3 segundos
        
        case 500:  // Internal Server Error
            return 4000;  // 4 segundos
        
        case 404:  // Not Found
            return 2000;  // 2 segundos
        
        default:
            return 2000;  // 2 segundos por defecto
    }
}

// ==================== Envío de Detecciones (API Real) ====================
bool APIClient::sendDetections(const std::map<String, BeaconData>& beacons) {
    if (beacons.empty()) {
        Serial.println("[API] No hay detecciones para enviar");
        return false;
    }

    Serial.println("\n========================================");
    Serial.printf("  Enviando %d Detecciones a la API\n", beacons.size());
    Serial.println("========================================");

    // Crear el payload JSON
    String payload = createDetectionsPayload(beacons);
    
    Serial.println("[API] Payload:");
    Serial.println(payload);

    int attempt = 1;
    bool success = false;

    while (!success && attempt <= MAX_RETRY_ATTEMPTS) {
        //  CRÍTICO: Liberar memoria antes de cada intento HTTPS
        Serial.printf("\n[API] Intento #%d/%d\n", attempt, MAX_RETRY_ATTEMPTS);
        Serial.printf("[API] Memoria libre: %d bytes\n", ESP.getFreeHeap());
        
        // Forzar garbage collection
        if (attempt > 1) {
            Serial.println("[API] Esperando estabilización de heap...");
            delay(2000);
        }
        
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);

        Serial.printf("[API] POST a: %s\n", API_URL);

        alertManager.loaderOn();

        // Iniciar conexión HTTP
        if (!http.begin(API_URL)) {
            Serial.println("[API]  Error: No se pudo iniciar la conexión HTTP");
            alertManager.loaderOff();
            alertManager.showError();
            delay(2000);
            attempt++;
            continue;
        }

        // ⚡ CRÍTICO: Dar tiempo para estabilización de memoria antes de HTTPS
        if (attempt > 1) {
            Serial.println("[API] Esperando estabilización de memoria...");
            delay(2000);  // Dar tiempo para que el heap se reorganice
        }
        
        Serial.printf("[API] Memoria libre antes de HTTP: %d bytes\n", ESP.getFreeHeap());
        
        // Configurar headers
        http.addHeader("Content-Type", "application/json");

        // Enviar POST
        int httpCode = http.POST(payload);
        
        Serial.printf("[API] Memoria libre después de HTTP: %d bytes\n", ESP.getFreeHeap());
        
        alertManager.loaderOff();

        if (httpCode > 0) {
            String response = http.getString();
            Serial.printf("[API] Código HTTP: %d\n", httpCode);
            Serial.println("[API] Respuesta:");
            Serial.println(response);

            if (handleResponse(httpCode, response)) {
                // Éxito
                Serial.println("[API] Detecciones enviadas correctamente");
                alertManager.showSuccess();
                delay(1500);
                success = true;
            } 
            else if (shouldRetry(httpCode)) {
                // Error recuperable, reintentar
                Serial.printf("[API] Error %d - Reintentando...\n", httpCode);
                alertManager.showError();
                unsigned long retryDelay = getRetryDelay(httpCode);
                delay(retryDelay);
                attempt++;
            } 
            else {
                // Error no recuperable
                Serial.printf("[API]  Error no recuperable: %d\n", httpCode);
                alertManager.showError();
                delay(3000);
                break;
            }
        } 
        else {
            // Error de conexión
            Serial.printf("[API]  Error de conexión: %d (Memoria: %d bytes)\n", 
                         httpCode, ESP.getFreeHeap());
            alertManager.showError();

            if (httpCode == -11) {
                // Error SSL - Memoria insuficiente o timeout
                Serial.println("[API]  Error SSL/TLS - Reconectando WiFi y liberando memoria...");
                wifiManager.reconnect();
                delay(3000);  // Tiempo extra para estabilización
            }

            delay(2000);
            attempt++;
        }

        http.end();
    }

    if (!success) {
        Serial.println("[API]  No se pudieron enviar las detecciones después de todos los intentos");
    }

    return success;
}

// ==================== Creación de Payload de Detecciones ====================
String APIClient::createDetectionsPayload(const std::map<String, BeaconData>& beacons) {
    // OPTIMIZACIÓN: Reducir tamaño del documento para evitar error -11
    DynamicJsonDocument doc(1536);  // Reducido de 2048 a 1536 bytes
    
    // Datos del dispositivo - usar valores cargados con fallback automático
    String currentDeviceId = String(getDeviceId());
    String currentZoneName = String(getDeviceLocation());
    doc["device_id"] = currentDeviceId;
    doc["zone_name"] = currentZoneName;
    
    // Timestamp actual (epoch Unix)
    time_t currentTime = getCurrentEpoch();
    doc["timestamp"] = currentTime;

    // Array de detecciones
    JsonArray detections = doc.createNestedArray("detections");

    for (const auto& entry : beacons) {
        const BeaconData& beacon = entry.second;
        
        JsonObject detection = detections.createNestedObject();
        
        // ID del tag (animal)
        detection["tag_id"] = beacon.animalId;
        
        // Ubicación del dispositivo
        detection["device_location"] = getDeviceLocation();
        
        // Distancia calculada
        detection["distance"] = round(beacon.distance * 100.0) / 100.0;  // 2 decimales
        
        // RSSI
        detection["rssi"] = beacon.rssi;
        
        // Timestamp actual (epoch)
        detection["detected_at"] = currentTime;
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}

// ==================== Consulta Status de Beacon ====================
String APIClient::checkBeaconStatus(const String& macAddress) {
    Serial.printf("[API] Consultando status del beacon: %s\n", macAddress.c_str());
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] Error: WiFi no conectado");
        return "unknown";
    }
    
    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) {
        Serial.println("[API] Error: No se pudo crear cliente SSL");
        return "unknown";
    }
    
    client->setInsecure();
    client->setTimeout(10);
    
    HTTPClient http;
    http.setTimeout(12000);
    http.setReuse(false);
    
    // Construir URL del endpoint
    String url = String(API_URL) + "/api/beacon/status/" + macAddress;
    
    Serial.printf("[API] URL: %s\n", url.c_str());
    
    if (!http.begin(*client, url)) {
        Serial.println("[API] Error: No se pudo iniciar conexion HTTPS");
        delete client;
        return "unknown";
    }
    
    http.addHeader("Content-Type", "application/json");
    if (strlen(API_KEY) > 0) {
        http.addHeader("Authorization", String("Bearer ") + API_KEY);
    }
    
    int httpCode = http.GET();
    String status = "unknown";
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[API] HTTP: %d\n", httpCode);
        Serial.printf("[API] Respuesta: %s\n", response.c_str());
        
        if (httpCode == 200) {
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                if (doc.containsKey("status")) {
                    status = doc["status"].as<String>();
                    Serial.printf("[API] Status del beacon: %s\n", status.c_str());
                } else {
                    Serial.println("[API] Error: Campo 'status' no encontrado");
                }
            } else {
                Serial.printf("[API] Error JSON: %s\n", error.c_str());
            }
        } else if (httpCode == 404) {
            Serial.println("[API] Beacon no encontrado en el sistema");
            status = "unknown";
        } else {
            Serial.printf("[API] Error HTTP: %d\n", httpCode);
        }
    } else {
        Serial.printf("[API] Error de conexion: %d\n", httpCode);
    }
    
    http.end();
    delete client;
    
    return status;
}

// ==================== Consulta Status de Múltiples Beacons (OPTIMIZADO) ====================
std::map<String, String> APIClient::checkMultipleBeaconStatus(const std::vector<String>& macAddresses) {
    std::map<String, String> results;
    
    if (macAddresses.empty()) {
        Serial.println("[API] No hay MACs para consultar");
        return results;
    }
    
    Serial.printf("[API] Consultando status de %d beacons...\n", macAddresses.size());
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[API] Error: WiFi no conectado");
        return results;
    }
    
    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) {
        Serial.println("[API] Error: No se pudo crear cliente SSL");
        return results;
    }
    
    client->setInsecure();
    client->setTimeout(15);
    
    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    
    // Construir URL del endpoint
    String url = String(API_URL) + "/api/beacons/status/batch";
    
    Serial.printf("[API] URL: %s\n", url.c_str());
    
    if (!http.begin(*client, url)) {
        Serial.println("[API] Error: No se pudo iniciar conexion HTTPS");
        delete client;
        return results;
    }
    
    http.addHeader("Content-Type", "application/json");
    if (strlen(API_KEY) > 0) {
        http.addHeader("Authorization", String("Bearer ") + API_KEY);
    }
    
    // Crear payload JSON con array de MACs
    DynamicJsonDocument doc(2048);
    JsonArray macs = doc.createNestedArray("mac_addresses");
    for (const String& mac : macAddresses) {
        macs.add(mac);
    }
    
    String payload;
    serializeJson(doc, payload);
    
    Serial.printf("[API] Payload: %s\n", payload.c_str());
    
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("[API] HTTP: %d\n", httpCode);
        Serial.printf("[API] Respuesta: %s\n", response.c_str());
        
        if (httpCode == 200) {
            DynamicJsonDocument responseDoc(4096);
            DeserializationError error = deserializeJson(responseDoc, response);
            
            if (!error) {
                if (responseDoc.containsKey("beacons")) {
                    JsonArray beacons = responseDoc["beacons"];
                    
                    for (JsonObject beacon : beacons) {
                        String mac = beacon["mac"].as<String>();
                        String status = beacon["status"].as<String>();
                        
                        // Backend ya filtró: solo envía "unregistered"
                        // Pero verificamos por seguridad
                        if (status == "unregistered") {
                            results[mac] = status;
                            Serial.printf("[API] %s -> unregistered\n", mac.c_str());
                        }
                    }
                    
                    Serial.printf("[API] Beacons unregistered recibidos: %d\n", results.size());
                } else {
                    Serial.println("[API] Error: Campo 'beacons' no encontrado");
                }
            } else {
                Serial.printf("[API] Error JSON: %s\n", error.c_str());
            }
        } else {
            Serial.printf("[API] Error HTTP: %d\n", httpCode);
        }
    } else {
        Serial.printf("[API] Error de conexion: %d\n", httpCode);
    }
    
    http.end();
    delete client;
    
    return results;
}
