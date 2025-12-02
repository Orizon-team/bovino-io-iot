#include "config.h"
#include <Arduino.h>

static unsigned long buttonPressStart = 0;
static unsigned long lastDebounceTime = 0;
static bool buttonWasPressed = false;
static int lastButtonState = HIGH;
static int buttonState = HIGH;

const char* API_URL = "https://bovino-io-backend.onrender.com/detections/ingest";
const char* MQTT_BROKER = "621de91008c745099bb8eb28731701af.s1.eu.hivemq.cloud";
const char* CONFIG_PORTAL_PASSWORD = "bovinoiot";
const char* BLE_DEVICE_NAME = "ESP32-BovinoIOT";
const char* MQTT_TOPIC = "bovino_io/detections";
const char* NTP_SERVER2 = "time.nist.gov";
const char* AP_SSID_PREFIX = "bovino_io";
const char* NTP_SERVER1 = "pool.ntp.org";
const char* WIFI_PASSWORD = "123456789";
const char* MQTT_USER = "orizoncompany";
const char* MQTT_PASSWORD = "UzObFn33";
const char* WIFI_SSID = "UZIEL 1257";
const char* API_KEY = "tu-api-key";

String LOADED_SUB_LOCATION = "";
String LOADED_ZONE_NAME = "";
String LOADED_DEVICE_ID = "";
int LOADED_ZONE_ID = 0;

bool beaconRegistrationModeActive = false;

const char* getDeviceId() {
    static char deviceIdBuf[64];
    if (LOADED_DEVICE_ID.length() > 0) {
        strncpy(deviceIdBuf, LOADED_DEVICE_ID.c_str(), sizeof(deviceIdBuf) - 1);
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(deviceIdBuf, sizeof(deviceIdBuf), "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]);
    }
    deviceIdBuf[sizeof(deviceIdBuf) - 1] = '\0';
    return deviceIdBuf;
}

const char* getDeviceLocation() {
    static char locationBuf[64];
    if (LOADED_ZONE_NAME.length() > 0) {
        strncpy(locationBuf, LOADED_ZONE_NAME.c_str(), sizeof(locationBuf) - 1);
    } else {
        strncpy(locationBuf, "Zona sin configurar", sizeof(locationBuf) - 1);
    }
    locationBuf[sizeof(locationBuf) - 1] = '\0';
    return locationBuf;
}

DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;  
uint8_t MASTER_MAC_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void initResetButton() {
    pinMode(RESET_BUTTON, INPUT_PULLUP);
    Serial.printf("[Reset] Botón de reset configurado en GPIO%d\n", RESET_BUTTON);
    Serial.println("[Reset] Mantén presionado 3 segundos para borrar configuración");
}

void initModeButton() {
    pinMode(MODE_BUTTON, INPUT_PULLUP);
    Serial.printf("[Mode] Botón de salida configurado en GPIO%d\n", MODE_BUTTON);
    Serial.println("[Mode] Presiona para salir del modo REGISTRO");
}

static bool registrationModeState = false;  

void enterRegistrationMode() {
    registrationModeState = true;
    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("[Mode] ✓ MODO REGISTRO ACTIVADO");
    Serial.println("[Mode] Presiona botón GPIO33 para pasar a MODO NORMAL");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

void exitRegistrationMode() {
    if (registrationModeState) {
        registrationModeState = false;
        Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("[Mode] ✓ MODO NORMAL ACTIVADO");
        Serial.println("[Mode] Dispositivo en modo de detección continua");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }
}

void checkModeButtonPress() {
    static unsigned long lastDebounceTime = 0;
    static int lastButtonState = HIGH;
    static int buttonState = HIGH;
    
    int reading = digitalRead(MODE_BUTTON);
    
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != buttonState) {
            buttonState = reading;
            
            if (buttonState == LOW && registrationModeState) {
                exitRegistrationMode();
            }
        }
    }
    
    lastButtonState = reading;
}


bool isRegistrationModeActive() {
    return registrationModeState;
}

bool checkResetButton() {
    int reading = digitalRead(RESET_BUTTON);
    

    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != buttonState) {
            buttonState = reading;
            
            if (buttonState == LOW && !buttonWasPressed) {
                buttonWasPressed = true;
                buttonPressStart = millis();
                Serial.println("[Reset]   Botón presionado - mantén 3 seg...");
            }
            
            if (buttonState == HIGH && buttonWasPressed) {
                unsigned long pressDuration = millis() - buttonPressStart;
                buttonWasPressed = false;
                
                Serial.printf("[Reset]   Botón liberado después de %lu ms\n", pressDuration);
            }
        }
    }
    
    if (buttonWasPressed && buttonState == LOW) {
        unsigned long pressDuration = millis() - buttonPressStart;
                static unsigned long lastFeedback = 0;
        if (pressDuration >= 1000 && pressDuration - lastFeedback >= 1000) {
            lastFeedback = pressDuration;
            Serial.printf("[Reset]   Presionado: %lu ms / %lu ms\n", pressDuration, RESET_BUTTON_HOLD_TIME);
        }
        
        if (pressDuration >= RESET_BUTTON_HOLD_TIME) {
            Serial.println("\n[Reset]   RESET CONFIRMADO - Borrando configuración...");
            buttonWasPressed = false;  
            return true;  
        }
    }
    
    lastButtonState = reading;
    return false;
}