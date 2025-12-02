#include "espnow_manager.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>

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
        Serial.printf("[ESP-NOW] ⚠️ Tamaño incorrecto: %d bytes (esperado: %d)\n", 
                     len, sizeof(ESPNowMessage));
        return;
    }

    ESPNowMessage msg;
    memcpy(&msg, data, sizeof(ESPNowMessage));
    staticReceivedMessages.push_back(msg);

    Serial.printf("[ESP-NOW] ✓ Recibido de %02X:%02X:%02X:%02X:%02X:%02X - ID=%u, Buffer: %d msgs\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                  msg.animalId, staticReceivedMessages.size());
}

// Inicializar como maestro
bool ESPNowManager::initializeMaster() {
    Serial.println("[ESP-NOW] Inicializando como MAESTRO...");
    
    // No cambiar modo WiFi si ya está conectado
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
    }
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ❌ Error al inicializar ESP-NOW");
        return false;
    }

    esp_err_t cb_result = esp_now_register_recv_cb(onDataReceive);
    if (cb_result == ESP_OK) {
        Serial.println("[ESP-NOW] ✓ Callback de recepción registrado exitosamente");
    } else {
        Serial.printf("[ESP-NOW] ❌ Error al registrar callback: %d\n", cb_result);
        return false;
    }
    
    // Agregar peer broadcast para recibir de cualquier esclavo
    esp_now_peer_info_t broadcastPeer;
    memset(&broadcastPeer, 0, sizeof(esp_now_peer_info_t));
    memset(broadcastPeer.peer_addr, 0xFF, 6);  // Broadcast address
    broadcastPeer.channel = 0;  // Canal actual
    broadcastPeer.encrypt = false;
    
    esp_err_t peer_result = esp_now_add_peer(&broadcastPeer);
    if (peer_result == ESP_OK) {
        Serial.println("[ESP-NOW] ✓ Peer broadcast agregado (recibe de todos los esclavos)");
    } else if (peer_result == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("[ESP-NOW] ⓘ Peer broadcast ya existe");
    } else {
        Serial.printf("[ESP-NOW] ⚠️ No se pudo agregar peer broadcast: %d\n", peer_result);
    }
    
    isMaster = true;
    
    // Obtener canal actual del WiFi
    uint8_t currentChannel;
    wifi_second_chan_t secondChannel;
    esp_wifi_get_channel(&currentChannel, &secondChannel);
    
    Serial.println("[ESP-NOW] ✓ Maestro inicializado correctamente");
    Serial.printf("[ESP-NOW] MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("[ESP-NOW] Canal WiFi: %d (secundario: %d)\n", currentChannel, secondChannel);
    Serial.println("[ESP-NOW] ⚠️ IMPORTANTE: Esclavo debe usar el mismo canal");
    Serial.println("[ESP-NOW] Esperando mensajes ESP-NOW...");
    
    return true;
}

// Inicializar como esclavo
bool ESPNowManager::initializeSlave() {
    Serial.println("[ESP-NOW] Inicializando como ESCLAVO...");
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Escanear redes WiFi para encontrar el canal del maestro
    Serial.println("[ESP-NOW] Escaneando para detectar canal del maestro...");
    int n = WiFi.scanNetworks();
    int detectedChannel = 1; // Fallback al canal 1
    
    if (n > 0) {
        Serial.printf("[ESP-NOW] Encontradas %d redes WiFi\n", n);
        // Intentar detectar el canal mirando redes cercanas
        // Usaremos el canal de la red más fuerte como referencia
        int maxRSSI = -100;
        for (int i = 0; i < n; i++) {
            int rssi = WiFi.RSSI(i);
            if (rssi > maxRSSI) {
                maxRSSI = rssi;
                detectedChannel = WiFi.channel(i);
            }
        }
        Serial.printf("[ESP-NOW] Canal detectado: %d (RSSI más fuerte: %d dBm)\n", detectedChannel, maxRSSI);
    } else {
        Serial.println("[ESP-NOW] No se encontraron redes WiFi, usando canal 1");
    }
    
    // Configurar el canal WiFi antes de inicializar ESP-NOW
    WiFi.disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(detectedChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    
    Serial.printf("[ESP-NOW] Canal WiFi configurado: %d\n", detectedChannel);
    
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
    
    // Usar canal 0 para autodetección o ESPNOW_CHANNEL si está configurado
    if (ESPNOW_CHANNEL == 0) {
        masterPeerInfo.channel = 0;  // Auto: usar el canal actual del WiFi
    } else {
        masterPeerInfo.channel = ESPNOW_CHANNEL;
    }
    
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
    msg.animalId = doc["animal_id"] | 0;
    msg.rssi = doc["rssi"] | 0;
    msg.distance = doc["distance"] | 0.0f;

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
