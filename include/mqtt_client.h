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
    bool initialize();
    bool isConnected();
    bool reconnect();
    void loop();
    bool sendDetections(const std::map<String, BeaconData>& beacons);
    bool publish(const char* topic, const char* payload);

private:
    WiFiClientSecure wifiClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
    
    String createDetectionsPayload(const std::map<String, BeaconData>& beacons);
    static void messageCallback(char* topic, byte* payload, unsigned int length);
};

extern MQTTClient mqttClient;

#endif
