#include "api_client.h"
#include "wifi_manager.h"
#include "alerts.h"
#include "display_manager.h"

// Definición de la instancia global
APIClient apiClient;

// ==================== Constructor ====================
APIClient::APIClient() {
}

// ==================== Sincronización de Tiempo ====================
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
        Serial.println("\n[NTP] ✓ Tiempo sincronizado correctamente");
        return true;
    } else {
        Serial.println("\n[NTP] ⚠ Advertencia: Tiempo no sincronizado");
        return false;
    }
}

// ==================== Envío de Asistencias ====================
// TODO: Refactorizar para sistema de ganado - Temporalmente deshabilitado
/*
bool APIClient::sendAttendance(const std::vector<BleData>& devices, int stageNumber) {
    Serial.println("\n========================================");
    Serial.printf("  Enviando Asistencias - Etapa %d\n", stageNumber);
    Serial.println("========================================");

    // Crear el payload JSON
    String payload = createPayload(devices);
    
    Serial.println("[API] Payload:");
    Serial.println(payload);

    int attempt = 1;
    bool success = false;

    while (!success) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);

        displayManager.showPostStatus(stageNumber, attempt);
        Serial.printf("\n[API] Intento #%d - POST a: %s\n", attempt, API_URL);

        alertManager.loaderOn();

        // Iniciar conexión HTTP
        if (!http.begin(API_URL)) {
            Serial.println("[API] ❌ Error: No se pudo iniciar la conexión HTTP");
            alertManager.loaderOff();
            alertManager.showError();
            delay(2000);
            attempt++;
            continue;
        }

        // Configurar headers
        http.addHeader("Content-Type", "application/json");

        delay(1000);

        // Enviar POST
        int httpCode = http.POST(payload);
        
        alertManager.loaderOff();

        if (httpCode > 0) {
            String response = http.getString();
            Serial.printf("[API] Código HTTP: %d\n", httpCode);
            Serial.println("[API] Respuesta:");
            Serial.println(response);

            if (handleResponse(httpCode, response)) {
                // Éxito
                displayManager.showPostSuccess(stageNumber, httpCode);
                alertManager.showSuccess();
                delay(1500);
                success = true;
            } 
            else if (shouldRetry(httpCode)) {
                // Error recuperable, reintentar
                displayManager.showServerError(httpCode);
                alertManager.showError();
                unsigned long retryDelay = getRetryDelay(httpCode);
                delay(retryDelay);
                attempt++;
            } 
            else {
                // Error no recuperable
                Serial.printf("[API] ❌ Error no recuperable: %d\n", httpCode);
                displayManager.showHTTPError(stageNumber, httpCode);
                alertManager.showError();
                delay(3000);
                attempt++;
            }
        } 
        else {
            // Error de conexión
            Serial.printf("[API] ❌ Error de conexión: %d\n", httpCode);
            displayManager.showHTTPError(stageNumber, httpCode);
            alertManager.showError();

            if (httpCode == -11) {
                // Conexión perdida, reconectar WiFi
                Serial.println("[API] Reconectando WiFi...");
                wifiManager.reconnect();
            }

            delay(2000);
            attempt++;
        }

        http.end();

        // Límite de intentos (opcional)
        if (attempt > 20) {
            Serial.println("[API] ⚠ Máximo de intentos alcanzado");
            break;
        }
    }

    return success;
}

// ==================== Creación de Payload ====================
String APIClient::createPayload(const std::vector<BleData>& devices) {
    DynamicJsonDocument doc(1024);
    
    doc["id_device"] = ID_DEVICE;
    
    // Obtener timestamp actual
    String timestamp = getCurrentTimestamp();
    doc["data_time"] = timestamp;

    // Array de asistencias
    JsonArray attendances = doc.createNestedArray("attendances");

    if (!devices.empty()) {
        time_t now = time(NULL);
        now -= 6 * 3600;  // Ajuste de zona horaria (GMT-6)

        for (const auto& device : devices) {
            JsonObject att = attendances.createNestedObject();
            att["id_student"] = device.studentId;

            // Calcular el tiempo de detección
            unsigned long ms = device.detectionTime;
            time_t detectedEpoch = now - ((millis() - ms) / 1000);
            
            struct tm* detectedTimeinfo = gmtime(&detectedEpoch);
            char detectedIsoTime[25];
            strftime(detectedIsoTime, sizeof(detectedIsoTime), "%Y-%m-%dT%H:%M:%SZ", detectedTimeinfo);
            
            att["attendance_time"] = detectedIsoTime;
        }
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}

// ==================== Timestamp Actual ====================
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
*/
