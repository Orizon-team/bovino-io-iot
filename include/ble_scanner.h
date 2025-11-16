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
    MODE_ACTIVE,     // Escaneo frecuente (alta actividad)
    MODE_NORMAL,     // Escaneo estándar (actividad moderada)
    MODE_ECO         // Escaneo espaciado (baja actividad)
};

class BLEScanner {
public:
    BLEScanner();
    bool initialize();                                          // Inicializa el sistema BLE
    void performScan();                                         // Ejecuta un escaneo BLE
    float calculateDistance(int8_t rssi);                       // Calcula distancia desde RSSI
    void updateBehavior(uint32_t animalId, const BeaconData& beacon);   // Actualiza comportamiento del animal
    void checkMissingAnimals();                                 // Verifica animales perdidos
    void adjustScanMode();                                      // Ajusta modo de escaneo según actividad
    std::vector<uint32_t> getCurrentAnimals();                  // Obtiene lista de animales presentes
    std::vector<uint32_t> getMissingAnimals();                  // Obtiene lista de animales perdidos
    std::map<String, BeaconData> getBeaconData();              // Obtiene mapa de beacons detectados
    std::map<uint32_t, AnimalBehavior> getBehaviorData();      // Obtiene mapa de comportamientos
    ScanMode getCurrentMode();                                  // Obtiene modo de escaneo actual
    int getAnimalCount();                                       // Cuenta animales presentes
    void getStats(int& totalScans, int& changesDetected);      // Obtiene estadísticas de escaneo

private:
    ScanMode currentMode;                                       // Modo de escaneo actual
    unsigned long lastScanTime;                                 // Timestamp del último escaneo
    int scansWithoutChange;                                     // Contador de escaneos sin cambios
    int recentChanges;                                          // Contador de cambios recientes
    int totalScanCount;                                         // Total de escaneos realizados
    std::map<String, BeaconData> beacons;                      // Mapa de beacons (clave: MAC_animalID)
    std::map<uint32_t, AnimalBehavior> behaviors;              // Mapa de comportamientos (clave: animalID)

    void processDevice(BLEAdvertisedDevice advertisedDevice);   // Procesa dispositivo BLE detectado
    uint32_t extractAnimalId(std::string manufacturerData);     // Extrae ID del animal del beacon
    int getScanDuration();                                      // Obtiene duración de escaneo según modo
    unsigned long getScanInterval();                            // Obtiene intervalo de escaneo según modo

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
