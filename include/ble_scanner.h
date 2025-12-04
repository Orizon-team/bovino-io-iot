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

class BLEScanner {
public:
    BLEScanner();
    bool initialize();
    void performScan();
    void startBeaconRegistrationMode();
    std::map<String, BeaconData> getBeaconData();
    void clearBeacons();
    float calculateDistance(int8_t rssi);

private:
    std::map<String, BeaconData> beacons;   
    std::map<String, BeaconData> configurableBeacons;
    std::map<String, unsigned long> registeredBeaconsCache;
    
    void processDevice(BLEAdvertisedDevice advertisedDevice);
    bool shouldProcessBeacon(BLEAdvertisedDevice& device);
    uint32_t extractAnimalId(std::string manufacturerData);
    void publishBeaconsToMQTT(const std::vector<String>& macAddresses);

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

#endif
