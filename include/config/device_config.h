#pragma once
#include <Arduino.h>
#include "enums/device.h"

extern String LOADED_SUB_LOCATION;
extern String LOADED_ZONE_NAME;
extern String LOADED_DEVICE_ID;
extern int LOADED_ZONE_ID;
extern DeviceMode CURRENT_DEVICE_MODE;
extern uint8_t MASTER_MAC_ADDRESS[6];

const char* getDeviceId();
const char* getDeviceLocation();

constexpr int ESPNOW_CHANNEL = 0;
constexpr int MAX_SLAVES = 10;
constexpr unsigned long ESPNOW_SEND_INTERVAL = 3000;
