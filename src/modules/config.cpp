#include "config.h"
#include <Arduino.h>

// ==================== VARIABLES DE ESTADO DEL BOTÓN ====================
static unsigned long buttonPressStart = 0;
static bool buttonWasPressed = false;
static unsigned long lastDebounceTime = 0;
static int lastButtonState = HIGH;
static int buttonState = HIGH;

// ==================== CONFIGURACIÓN BLE ====================
const char* BLE_DEVICE_NAME = "ESP32-BovinoIOT";

// ==================== CONFIGURACIÓN WiFi ====================
const char* WIFI_SSID = "UZIEL 1257";
const char* WIFI_PASSWORD = "123456789";
const char* CONFIG_PORTAL_PASSWORD = "bovinoiot";
const char* AP_SSID_PREFIX = "bovino_io";

// ==================== CONFIGURACIÓN API ====================
const char* API_URL = "https://bovino-io-backend.onrender.com/detections/ingest";
const char* API_KEY = "tu-api-key";

const char* MQTT_BROKER = "621de91008c745099bb8eb28731701af.s1.eu.hivemq.cloud";
const char* MQTT_USER = "orizoncompany";
const char* MQTT_PASSWORD = "UzObFn33";
const char* MQTT_TOPIC = "bovino_io/detections";

const char* NTP_SERVER1 = "pool.ntp.org";
const char* NTP_SERVER2 = "time.nist.gov";

// ==================== IDENTIFICACIÓN DE ESTE DISPOSITIVO IOT (ZONA) ====================
const char* DEVICE_ID = "IOT_ZONA_001";
const char* DEVICE_LOCATION = "Comedero Norte";
ZoneType CURRENT_ZONE_TYPE = ZONE_FEEDER;

// Variables globales para ubicación cargada desde Preferences
String LOADED_ZONE_NAME = "";
String LOADED_SUB_LOCATION = "";
String LOADED_DEVICE_ID = "";

// ==================== MODO DISPOSITIVO ====================
DeviceMode CURRENT_DEVICE_MODE = DEVICE_SLAVE;  // Por defecto es ESCLAVO

// ==================== ESP-NOW ====================
uint8_t MASTER_MAC_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ==================== RESET BUTTON FUNCTIONS ====================
/**
 * Inicializa el botón de reset
 * Configura el pin con pull-up interno
 */
void initResetButton() {
    pinMode(RESET_BUTTON, INPUT_PULLUP);
    Serial.printf("[Reset] Botón de reset configurado en GPIO%d\n", RESET_BUTTON);
    Serial.println("[Reset] Mantén presionado 3 segundos para borrar configuración");
}

/**
 * Verifica si el botón de reset ha sido presionado durante 3 segundos
 * Implementa debouncing y validación de tiempo sostenido
 * @return true si se debe resetear la configuración
 */
bool checkResetButton() {
    int reading = digitalRead(RESET_BUTTON);
    
    // ========== DEBOUNCING ==========
    // Si el estado cambió, resetear el temporizador de debounce
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    
    // Si ha pasado el tiempo de debounce, actualizar el estado
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        // Si el estado es diferente al anterior
        if (reading != buttonState) {
            buttonState = reading;
            
            // ========== BOTÓN PRESIONADO (LOW porque usa PULLUP) ==========
            if (buttonState == LOW && !buttonWasPressed) {
                buttonWasPressed = true;
                buttonPressStart = millis();
                Serial.println("[Reset] ⏳ Botón presionado - mantén 3 seg...");
            }
            
            // ========== BOTÓN LIBERADO ==========
            if (buttonState == HIGH && buttonWasPressed) {
                unsigned long pressDuration = millis() - buttonPressStart;
                buttonWasPressed = false;
                
                Serial.printf("[Reset] ❌ Botón liberado después de %lu ms\n", pressDuration);
            }
        }
    }
    
    // ========== VERIFICAR SI SE MANTIENE PRESIONADO ==========
    if (buttonWasPressed && buttonState == LOW) {
        unsigned long pressDuration = millis() - buttonPressStart;
        
        // Feedback cada segundo
        static unsigned long lastFeedback = 0;
        if (pressDuration >= 1000 && pressDuration - lastFeedback >= 1000) {
            lastFeedback = pressDuration;
            Serial.printf("[Reset] ⏱️  Presionado: %lu ms / %lu ms\n", pressDuration, RESET_BUTTON_HOLD_TIME);
        }
        
        // ✅ VERIFICAR SI YA SE CUMPLIERON LOS 3 SEGUNDOS
        if (pressDuration >= RESET_BUTTON_HOLD_TIME) {
            Serial.println("\n[Reset] ✅ RESET CONFIRMADO - Borrando configuración...");
            buttonWasPressed = false;  // Evitar múltiples triggers
            return true;  // ← RESET ACTIVADO
        }
    }
    
    lastButtonState = reading;
    return false;
}