#include "alerts.h"

// Definición de la instancia global
AlertManager alertManager;

// ==================== Constructor ====================
AlertManager::AlertManager() : loaderState(false) {
}

// ==================== Inicialización ====================
void AlertManager::initialize() {
    // Configurar pines como salida
    pinMode(LED_LOADER, OUTPUT);
    pinMode(LED_SUCCESS, OUTPUT);
    pinMode(LED_ERROR, OUTPUT);
    pinMode(LED_DANGER, OUTPUT);
    pinMode(ZUMBADOR, OUTPUT);

    // Apagar todos los LEDs y buzzer
    allOff();

    Serial.println("[Alerts] Sistema de alertas inicializado");
}

// ==================== Control del LED de Carga ====================
void AlertManager::loaderOn() {
    digitalWrite(LED_LOADER, HIGH);
    loaderState = true;
}

void AlertManager::loaderOff() {
    digitalWrite(LED_LOADER, LOW);
    loaderState = false;
}

void AlertManager::loaderToggle() {
    loaderState = !loaderState;
    digitalWrite(LED_LOADER, loaderState ? HIGH : LOW);
}

// ==================== Alertas Predefinidas ====================
void AlertManager::showSuccess(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_SUCCESS, HIGH);
        delay(100);
        digitalWrite(LED_SUCCESS, LOW);
        delay(100);
    }
}

void AlertManager::showError(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_ERROR, HIGH);
        digitalWrite(ZUMBADOR, HIGH);
        delay(200);
        digitalWrite(LED_ERROR, LOW);
        digitalWrite(ZUMBADOR, LOW);
        delay(200);
    }
}

void AlertManager::showDanger(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_DANGER, HIGH);
        delay(150);
        digitalWrite(LED_DANGER, LOW);
        delay(150);
    }
}

// ==================== Control General ====================
void AlertManager::flashLED(int ledPin, int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(ledPin, HIGH);
        delay(delayMs);
        digitalWrite(ledPin, LOW);
        delay(delayMs);
    }
}

void AlertManager::beep(int duration) {
    digitalWrite(ZUMBADOR, HIGH);
    delay(duration);
    digitalWrite(ZUMBADOR, LOW);
}

void AlertManager::allOff() {
    digitalWrite(LED_LOADER, LOW);
    digitalWrite(LED_SUCCESS, LOW);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_DANGER, LOW);
    digitalWrite(ZUMBADOR, LOW);
    loaderState = false;
}
