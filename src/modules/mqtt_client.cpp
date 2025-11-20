#include "mqtt_client.h"
#include <ArduinoJson.h>
#include "api_client.h"

MQTTClient mqttClient;

MQTTClient::MQTTClient() : mqttClient(wifiClient), lastReconnectAttempt(0) {
}

bool MQTTClient::initialize() {
    if (!ENABLE_MQTT) {
        Serial.println("[MQTT] MQTT deshabilitado en configuración");
        return false;
    }
    
    Serial.println("[MQTT] Inicializando cliente MQTT...");
    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
    Serial.printf("[MQTT] Topic: %s\n", MQTT_TOPIC);
    
    wifiClient.setInsecure();
    
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(MQTTClient::messageCallback);
    mqttClient.setBufferSize(2048);
    
    return reconnect();
}

bool MQTTClient::isConnected() {
    return mqttClient.connected();
}

bool MQTTClient::reconnect() {
    if (mqttClient.connected()) {
        return true;
    }
    
    unsigned long now = millis();
    if (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL) {
        return false;
    }
    
    lastReconnectAttempt = now;
    
    Serial.println("[MQTT] Intentando conectar al broker...");
    
    String clientId = "ESP32-" + String(DEVICE_ID);
    bool connected = false;
    
    if (strlen(MQTT_USER) > 0 && strlen(MQTT_PASSWORD) > 0) {
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
        Serial.println("[MQTT] ✅ Conectado al broker MQTT");
        mqttClient.subscribe(MQTT_TOPIC);
        return true;
    } else {
        Serial.printf("[MQTT] ❌ Error al conectar. Código: %d\n", mqttClient.state());
        return false;
    }
}

void MQTTClient::loop() {
    if (!ENABLE_MQTT) {
        return;
    }
    
    if (!mqttClient.connected()) {
        reconnect();
    } else {
        mqttClient.loop();
    }
}

String MQTTClient::createDetectionsPayload(const std::map<String, BeaconData>& beacons) {
    StaticJsonDocument<2048> doc;
    
    String currentLocation = LOADED_ZONE_NAME.length() > 0 ? LOADED_ZONE_NAME : DEVICE_LOCATION;
    
    // Obtener la dirección MAC del ESP32
    String macAddress = WiFi.macAddress();
    
    // Extraer location del sondeador desde LOADED_SUB_LOCATION (formato: "tipo/location")
    String deviceLocation = currentLocation; // Fallback a zona si no hay sublocation
    if (LOADED_SUB_LOCATION.length() > 0) {
        int slashPos = LOADED_SUB_LOCATION.indexOf('/');
        if (slashPos > 0) {
            deviceLocation = LOADED_SUB_LOCATION.substring(slashPos + 1); // Extraer "Comedero A" de "master/Comedero A"
        }
    }
    
    doc["mac_address"] = macAddress;
    doc["zone_name"] = currentLocation;
    doc["timestamp"] = apiClient.getCurrentEpoch();
    
    JsonArray detectionsArray = doc.createNestedArray("detections");
    
    for (const auto& pair : beacons) {
        const BeaconData& beacon = pair.second;
        
        if (!beacon.isPresent) {
            continue;
        }
        
        JsonObject detection = detectionsArray.createNestedObject();
        detection["tag_id"] = beacon.animalId;
        detection["device_location"] = deviceLocation; // Usar location específica del sondeador
        detection["distance"] = round(beacon.distance * 100) / 100.0;
        detection["rssi"] = beacon.rssi;
        detection["is_present"] = beacon.isPresent;
        detection["first_seen"] = beacon.firstSeen / 1000;
        detection["last_seen"] = beacon.lastSeen / 1000;
    }
    
    String payload;
    serializeJson(doc, payload);
    return payload;
}

bool MQTTClient::sendDetections(const std::map<String, BeaconData>& beacons) {
    if (!ENABLE_MQTT) {
        return false;
    }
    
    if (!isConnected()) {
        Serial.println("[MQTT] No conectado al broker");
        if (!reconnect()) {
            return false;
        }
    }
    
    String payload = createDetectionsPayload(beacons);
    
    Serial.println("\n========================================");
    Serial.printf("  Publicando en MQTT: %s\n", MQTT_TOPIC);
    Serial.println("========================================");
    Serial.printf("[MQTT] Payload:\n%s\n\n", payload.c_str());
    
    bool success = mqttClient.publish(MQTT_TOPIC, payload.c_str());
    
    if (success) {
        Serial.println("[MQTT] ✅ Datos publicados correctamente");
        return true;
    } else {
        Serial.println("[MQTT] ❌ Error al publicar datos");
        return false;
    }
}

void MQTTClient::messageCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("[MQTT] Mensaje recibido en topic '%s': ", topic);
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}
