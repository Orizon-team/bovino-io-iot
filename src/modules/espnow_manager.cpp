#include "espnow_manager.h"

ESPNowManager espNowManager;
ESPNowManager* ESPNowManager::instance = nullptr;

ESPNowManager::ESPNowManager() : isInitialized(false) {
    instance = this;
}

bool ESPNowManager::initializeMaster() {
    Serial.println("[ESP-NOW] Inicializando como MAESTRO...");
    
    WiFi.mode(WIFI_AP_STA);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ Error al inicializar ESP-NOW");
        return false;
    }
    
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);
    
    isInitialized = true;
    Serial.println("[ESP-NOW] ✅ Maestro inicializado correctamente");
    Serial.print("[ESP-NOW] MAC Address: ");
    Serial.println(WiFi.macAddress());
    
    return true;
}

bool ESPNowManager::initializeSlave() {
    Serial.println("[ESP-NOW] Inicializando como ESCLAVO...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ Error al inicializar ESP-NOW");
        return false;
    }
    
    esp_now_register_send_cb(onDataSent);
    
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, MASTER_MAC_ADDRESS, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ Error al agregar maestro como peer");
        return false;
    }
    
    isInitialized = true;
    Serial.println("[ESP-NOW] ✅ Esclavo inicializado correctamente");
    Serial.print("[ESP-NOW] MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("[ESP-NOW] Maestro configurado: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 MASTER_MAC_ADDRESS[0], MASTER_MAC_ADDRESS[1], MASTER_MAC_ADDRESS[2],
                 MASTER_MAC_ADDRESS[3], MASTER_MAC_ADDRESS[4], MASTER_MAC_ADDRESS[5]);
    
    return true;
}

bool ESPNowManager::sendToMaster(const ESPNowMessage& message) {
    if (!isInitialized) {
        Serial.println("[ESP-NOW] ❌ ESP-NOW no inicializado");
        return false;
    }
    
    esp_err_t result = esp_now_send(MASTER_MAC_ADDRESS, (uint8_t*)&message, sizeof(ESPNowMessage));
    
    if (result == ESP_OK) {
        Serial.printf("[ESP-NOW] ✅ Mensaje enviado a maestro (Animal ID: %u)\n", message.animalId);
        return true;
    } else {
        Serial.printf("[ESP-NOW] ❌ Error enviando mensaje: %d\n", result);
        return false;
    }
}

bool ESPNowManager::broadcastToSlaves(const ESPNowMessage& message) {
    if (!isInitialized) {
        Serial.println("[ESP-NOW] ❌ ESP-NOW no inicializado");
        return false;
    }
    
    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t*)&message, sizeof(ESPNowMessage));
    
    if (result == ESP_OK) {
        Serial.println("[ESP-NOW] ✅ Mensaje broadcast enviado");
        return true;
    } else {
        Serial.printf("[ESP-NOW] ❌ Error en broadcast: %d\n", result);
        return false;
    }
}

void ESPNowManager::processReceivedData() {
    // Los datos se procesan en el callback onDataRecv
}

std::vector<ESPNowMessage> ESPNowManager::getReceivedMessages() {
    return receivedMessages;
}

void ESPNowManager::clearReceivedMessages() {
    receivedMessages.clear();
}

int ESPNowManager::getReceivedCount() {
    return receivedMessages.size();
}

void ESPNowManager::onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.print("[ESP-NOW] Estado de envío: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Éxito" : "❌ Fallo");
}

void ESPNowManager::onDataRecv(const uint8_t *macAddr, const uint8_t *data, int dataLen) {
    if (instance == nullptr) return;
    
    Serial.printf("[ESP-NOW] 📨 Mensaje recibido de %02X:%02X:%02X:%02X:%02X:%02X\n",
                 macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    
    if (dataLen == sizeof(ESPNowMessage)) {
        ESPNowMessage message;
        memcpy(&message, data, sizeof(ESPNowMessage));
        
        instance->receivedMessages.push_back(message);
        
        Serial.printf("[ESP-NOW] 🐄 Datos: Device=%s, Zona=%s, Animal ID=%u, RSSI=%d, Dist=%.2fm\n",
                     message.deviceId, message.location, message.animalId, 
                     message.rssi, message.distance);
    } else {
        Serial.printf("[ESP-NOW] ⚠️ Tamaño incorrecto: %d bytes (esperado %d)\n", 
                     dataLen, sizeof(ESPNowMessage));
    }
}
