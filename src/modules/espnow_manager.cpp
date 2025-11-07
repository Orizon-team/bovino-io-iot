#include "espnow_manager.h"
#include <ArduinoJson.h>

// Instancia global
ESPNowManager espNowManager;

// Variable estática para almacenar mensajes recibidos
static std::vector<ESPNowMessage> staticReceivedMessages;

// Constructor
ESPNowManager::ESPNowManager() : isMaster(false) {
}

// Callback para recibir datos
void ESPNowManager::onDataReceive(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != sizeof(ESPNowMessage)) {
        Serial.println("[ESP-NOW] Tamaño de mensaje incorrecto");
        return;
    }

    ESPNowMessage msg;
    memcpy(&msg, data, sizeof(ESPNowMessage));
    staticReceivedMessages.push_back(msg);

    Serial.printf("[ESP-NOW] Mensaje recibido de %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Inicializar como maestro
bool ESPNowManager::initializeMaster() {
    Serial.println("[ESP-NOW] Inicializando como MAESTRO...");
    
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Error al inicializar ESP-NOW");
        return false;
    }

    esp_now_register_recv_cb(onDataReceive);
    
    isMaster = true;
    Serial.println("[ESP-NOW] Maestro inicializado correctamente");
    Serial.printf("[ESP-NOW] MAC Address: %s\n", WiFi.macAddress().c_str());
    
    return true;
}

// Inicializar como esclavo
bool ESPNowManager::initializeSlave() {
    Serial.println("[ESP-NOW] Inicializando como ESCLAVO...");
    
    WiFi.mode(WIFI_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] Error al inicializar ESP-NOW");
        return false;
    }

    // Agregar peer del maestro
    if (!addPeer(MASTER_MAC_ADDRESS)) {
        Serial.println("[ESP-NOW] Error al agregar maestro como peer");
        return false;
    }
    
    isMaster = false;
    Serial.println("[ESP-NOW] Esclavo inicializado correctamente");
    Serial.printf("[ESP-NOW] MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("[ESP-NOW] Maestro configurado: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  MASTER_MAC_ADDRESS[0], MASTER_MAC_ADDRESS[1], MASTER_MAC_ADDRESS[2],
                  MASTER_MAC_ADDRESS[3], MASTER_MAC_ADDRESS[4], MASTER_MAC_ADDRESS[5]);
    
    return true;
}

// Agregar peer
bool ESPNowManager::addPeer(const uint8_t* macAddress) {
    memcpy(masterPeerInfo.peer_addr, macAddress, 6);
    masterPeerInfo.channel = ESPNOW_CHANNEL;
    masterPeerInfo.encrypt = false;

    if (esp_now_add_peer(&masterPeerInfo) != ESP_OK) {
        return false;
    }
    
    return true;
}

// Enviar mensaje al maestro (usado por esclavos)
bool ESPNowManager::sendToMaster(const String& jsonMessage) {
    if (isMaster) {
        Serial.println("[ESP-NOW] Error: Un maestro no puede enviar al maestro");
        return false;
    }

    // Parsear JSON y crear mensaje ESP-NOW
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonMessage);
    
    if (error) {
        Serial.println("[ESP-NOW] Error al parsear JSON");
        return false;
    }

    ESPNowMessage msg;
    strncpy(msg.deviceId, doc["device_id"] | "", sizeof(msg.deviceId) - 1);
    strncpy(msg.location, doc["device_location"] | "", sizeof(msg.location) - 1);
    msg.zoneType = doc["zone_type"] | 0;
    msg.animalId = doc["animal_id"] | 0;
    msg.rssi = doc["rssi"] | 0;
    msg.distance = doc["distance"] | 0.0f;
    msg.isPresent = doc["is_present"] | false;
    msg.timestamp = millis();

    esp_err_t result = esp_now_send(MASTER_MAC_ADDRESS, (uint8_t*)&msg, sizeof(msg));
    
    if (result == ESP_OK) {
        Serial.println("[ESP-NOW] Mensaje enviado al maestro");
        return true;
    } else {
        Serial.printf("[ESP-NOW] Error al enviar: %d\n", result);
        return false;
    }
}

// Obtener mensajes recibidos
std::vector<ESPNowMessage> ESPNowManager::getReceivedMessages() {
    std::vector<ESPNowMessage> messages = staticReceivedMessages;
    return messages;
}

// Limpiar mensajes recibidos
void ESPNowManager::clearReceivedMessages() {
    staticReceivedMessages.clear();
}
