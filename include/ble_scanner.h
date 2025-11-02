#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>
#include <vector>
#include "config.h"

enum ScanMode {
    MODE_ACTIVE,
    MODE_NORMAL,
    MODE_ECO
};

class BLEScanner {
public:
    BLEScanner();
    bool initialize();
    void performScan();
    float calculateDistance(int8_t rssi);
    void updateBehavior(uint32_t animalId, const BeaconData& beacon);
    void checkMissingAnimals();
    void adjustScanMode();
    std::vector<uint32_t> getCurrentAnimals();
    std::vector<uint32_t> getMissingAnimals();
    std::map<uint32_t, BeaconData> getBeaconData();
    std::map<uint32_t, AnimalBehavior> getBehaviorData();
    ScanMode getCurrentMode();
    int getAnimalCount();
    void getStats(int& totalScans, int& changesDetected);

private:
    ScanMode currentMode;
    unsigned long lastScanTime;
    int scansWithoutChange;
    int recentChanges;
    int totalScanCount;
    std::map<uint32_t, BeaconData> beacons;
    std::map<uint32_t, AnimalBehavior> behaviors;

    void processDevice(BLEAdvertisedDevice advertisedDevice);
    uint32_t extractAnimalId(std::string manufacturerData);
    int getScanDuration();
    unsigned long getScanInterval();

    friend class AnimalBeaconCallbacks;
};

class AnimalBeaconCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    AnimalBeaconCallbacks(BLEScanner* scanner);
    void onResult(BLEAdvertisedDevice advertisedDevice) override;

private:
    BLEScanner* bleScanner;
};

extern BLEScanner bleScanner;

#endif // BLE_SCANNER_H
