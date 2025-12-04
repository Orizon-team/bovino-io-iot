#pragma once

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* CONFIG_PORTAL_PASSWORD;
extern const char* AP_SSID_PREFIX;
extern const char* API_URL;
extern const char* API_KEY;
extern const char* MQTT_BROKER;
extern const char* MQTT_USER;
extern const char* MQTT_PASSWORD;
extern const char* MQTT_TOPIC;
extern const char* NTP_SERVER1;
extern const char* NTP_SERVER2;

constexpr unsigned long WIFI_TIMEOUT = 20000;
constexpr unsigned long WIFI_RETRY_INTERVAL = 300000;
constexpr bool ENABLE_WIFI_SYNC = true;
constexpr bool ENABLE_WIFI_PORTAL = true;
constexpr int HTTP_TIMEOUT = 15000;
constexpr int MAX_RETRY_ATTEMPTS = 3;
constexpr int MQTT_PORT = 8883;
constexpr bool ENABLE_MQTT = true;
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000;
constexpr long GMT_OFFSET_SEC = -21600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;
constexpr int NTP_TIMEOUT_SECONDS = 30;
