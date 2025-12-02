#ifndef ESPNOW_MESSAGE_MODEL_H
#define ESPNOW_MESSAGE_MODEL_H
#include <Arduino.h>

struct ESPNowMessage
{
    char deviceId[32];
    char location[64];
    uint32_t animalId;
    int8_t rssi;
    float distance;
};

#endif