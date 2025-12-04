#include "managers/button_manager.h"
#include "config/hardware_config.h"
#include <Arduino.h>

static unsigned long buttonPressStart = 0;
static unsigned long lastDebounceTime = 0;
static bool buttonWasPressed = false;
static int lastButtonState = HIGH;
static int buttonState = HIGH;
static bool registrationModeState = false;

void initResetButton() {
    pinMode(RESET_BUTTON, INPUT_PULLUP);
    pinMode(LED_RGB_RED, OUTPUT);
    pinMode(LED_RGB_BLUE, OUTPUT);
    digitalWrite(LED_RGB_RED, LOW);
}

void initModeButton() {
    pinMode(MODE_BUTTON, INPUT_PULLUP);
}

void enterRegistrationMode() {
    registrationModeState = true;
    Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("[Mode] MODO REGISTRO ACTIVADO");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

void exitRegistrationMode() {
    if (registrationModeState) {
        registrationModeState = false;
        Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("[Mode] MODO NORMAL ACTIVADO");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }
}

void checkModeButtonPress() {
    static unsigned long localDebounceTime = 0;
    static int localLastState = HIGH;
    static int localState = HIGH;
    
    int reading = digitalRead(MODE_BUTTON);
    
    if (reading != localLastState) {
        localDebounceTime = millis();
    }
    
    if ((millis() - localDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != localState) {
            localState = reading;
            
            if (localState == LOW && registrationModeState) {
                digitalWrite(LED_RGB_BLUE, LOW);
                exitRegistrationMode();
            }
            else if (localState == LOW && !registrationModeState) {
                digitalWrite(LED_RGB_BLUE, HIGH);
                enterRegistrationMode();
            }
        }
    }

    localLastState = reading;
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
                digitalWrite(LED_RGB_RED, HIGH);
                Serial.println("[Reset]   Botón presionado - mantén 3 seg...");
            }
            
            if (buttonState == HIGH && buttonWasPressed) {
                unsigned long pressDuration = millis() - buttonPressStart;
                buttonWasPressed = false;
                digitalWrite(LED_RGB_RED, LOW);
                
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
