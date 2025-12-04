#pragma once
#include <cstdint>
#include "enums/beacon.h"

extern const char* BLE_DEVICE_NAME;
extern bool beaconRegistrationModeActive;

constexpr int SCAN_DURATION = 5;
constexpr unsigned long SCAN_CYCLE_INTERVAL = 6000;
constexpr AnimalIdSource ANIMAL_ID_SOURCE = USE_MAJOR_MINOR;
constexpr int RSSI_REFERENCE = -59;
constexpr float PATH_LOSS_EXPONENT = 2.0;
constexpr int MIN_RSSI_THRESHOLD = -95;
constexpr BeaconFilterMode BEACON_FILTER_MODE = FILTER_BY_UUID;
constexpr const char* BEACON_UUID_1 = "FDA50693-A4E2-4FB1-AFCF-C6EB07647825";
constexpr uint16_t TARGET_COMPANY_ID = 0x004C;
