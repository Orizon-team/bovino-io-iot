#ifndef ESPNOW_MANAGER_H
#define ESPNOW_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include "config.h"

class ESPNowManager {
public:
    ESPNowManager();
    bool initializeMaster();
    bool initializeSlave();
    bool sendToMaster(const ESPNowMessage& message);
    bool broadcastToSlaves(const ESPNowMessage& message);
    void processReceivedData();
    std::vector<ESPNowMessage> getReceivedMessages();
    void clearReceivedMessages();
    int getReceivedCount();

private:
    std::vector<ESPNowMessage> receivedMessages;
    bool isInitialized;
    
    static void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status);
    static void onDataRecv(const uint8_t *macAddr, const uint8_t *data, int dataLen);
    static ESPNowManager* instance;
};

extern ESPNowManager espNowManager;

#endif // ESPNOW_MANAGER_H
