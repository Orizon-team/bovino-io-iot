#ifndef BEACON_MODEL_H
#define BEACON_MODEL_H
#include <Arduino.h>

struct BeaconData
{
    uint32_t animalId;
    String macAddress;
    int8_t rssi;
    float distance;
    String detectedLocation;
};

#endif