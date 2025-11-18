#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <map>
#include "config.h"

class MQTTClient {
public:
    MQTTClient();
    bool initialize();                                          // Inicializa y conecta al broker MQTT
    bool isConnected();                                         // Verifica si está conectado
    bool reconnect();                                           // Reconecta al broker
    void loop();                                                // Mantiene la conexión activa
    bool sendDetections(const std::map<String, BeaconData>& beacons);   // Envía detecciones al broker

private:
    WiFiClientSecure wifiClient;                                // Cliente WiFi con TLS
    PubSubClient mqttClient;                                    // Cliente MQTT
    unsigned long lastReconnectAttempt;                         // Timestamp del último intento de reconexión
    
    String createDetectionsPayload(const std::map<String, BeaconData>& beacons);   // Crea payload JSON
    static void messageCallback(char* topic, byte* payload, unsigned int length);  // Callback de mensajes
};

extern MQTTClient mqttClient;

#endif // MQTT_CLIENT_H
