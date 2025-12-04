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
    bool sendToMaster(const String& jsonMessage);
    std::vector<ESPNowMessage> getReceivedMessages();
    void clearReceivedMessages();

private:
    bool isMaster;
    std::vector<ESPNowMessage> receivedMessages;
    esp_now_peer_info_t masterPeerInfo;
    
    static void onDataReceive(const uint8_t* mac, const uint8_t* data, int len);
    bool addPeer(const uint8_t* macAddress);
};

extern ESPNowManager espNowManager;

#endif
