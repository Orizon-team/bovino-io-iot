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

// Clase simplificada: solo escanear y reportar
class BLEScanner {
public:
    BLEScanner();
    bool initialize();                                  // Inicializa el sistema BLE
    void performScan();                                 // Ejecuta un escaneo BLE de 2s
    void startBeaconRegistrationMode();                 // Inicia modo registro de beacons
    std::map<String, BeaconData> getBeaconData();      // Obtiene snapshot actual de beacons
    void clearBeacons();                                // Limpia todos los beacons del escaneo anterior
    float calculateDistance(int8_t rssi);               // Calcula distancia desde RSSI

private:
    std::map<String, BeaconData> beacons;   
    std::map<String, BeaconData> configurableBeacons;           // Mapa de beacons detectados en el último escaneo
    std::map<String, unsigned long> registeredBeaconsCache;     // Cache de beacons ya procesados (MAC -> timestamp)
    
    void processDevice(BLEAdvertisedDevice advertisedDevice);   // Procesa dispositivo BLE detectado
    bool shouldProcessBeacon(BLEAdvertisedDevice& device);      // Filtra beacons localmente (RSSI, UUID)
    uint32_t extractAnimalId(std::string manufacturerData);     // Extrae ID del animal del beacon
    void publishBeaconsToMQTT(const std::vector<String>& macAddresses);         // Publica beacon unregistered a MQTT

    friend class AnimalBeaconCallbacks;
};

class AnimalBeaconCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    AnimalBeaconCallbacks(BLEScanner* scanner);
    void onResult(BLEAdvertisedDevice advertisedDevice) override;   // Callback al detectar dispositivo

private:
    BLEScanner* bleScanner;
};

extern BLEScanner bleScanner;

#endif // BLE_SCANNER_H
