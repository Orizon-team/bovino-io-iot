#include "alerts.h"

// Definición de la instancia global
AlertManager alertManager;

// ==================== Constructor ====================
AlertManager::AlertManager() : loaderState(false) {
}

// ==================== Inicialización ====================
void AlertManager::initialize() {

    pinMode(LED_RGB_RED, OUTPUT);
    pinMode(LED_RGB_GREEN, OUTPUT);
    pinMode(LED_RGB_BLUE, OUTPUT);
    pinMode(ZUMBADOR, OUTPUT);

    allOff();

    Serial.println("[Alerts] Sistema de alertas inicializado");
    Serial.println("[Alerts] LED RGB configurado:");
    Serial.printf("[Alerts]   - Rojo: GPIO%d\n", LED_RGB_RED);
    Serial.printf("[Alerts]   - Verde: GPIO%d\n", LED_RGB_GREEN);
    Serial.printf("[Alerts]   - Azul: GPIO%d\n", LED_RGB_BLUE);
}

// ==================== Control del LED de Carga (Loader) ====================
void AlertManager::loaderOn() {
    setColor(0, 0, 255);  // Azul para indicar cargando
    loaderState = true;
}

void AlertManager::loaderOff() {
    allOff();
    loaderState = false;
}

void AlertManager::loaderToggle() {
    loaderState = !loaderState;
    if (loaderState) {
        setColor(0, 0, 255);  // Azul
    } else {
        allOff();
    }
}

// ==================== Control de Color RGB ====================
void AlertManager::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    analogWrite(LED_RGB_RED, red);
    analogWrite(LED_RGB_GREEN, green);
    analogWrite(LED_RGB_BLUE, blue);
}

void AlertManager::flashColor(uint8_t red, uint8_t green, uint8_t blue, int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        setColor(red, green, blue);
        delay(delayMs);
        allOff();
        delay(delayMs);
    }
}

// ==================== Alertas Predefinidas ====================
void AlertManager::showSuccess(int times) {
    // Verde puro
    flashColor(0, 255, 0, times, 100);
}

void AlertManager::showError(int times) {
    // Rojo puro con beep
    for (int i = 0; i < times; i++) {
        setColor(255, 0, 0);
        digitalWrite(ZUMBADOR, HIGH);
        delay(200);
        allOff();
        digitalWrite(ZUMBADOR, LOW);
        delay(200);
    }
}

void AlertManager::showWarning(int times) {
    // Amarillo (rojo + verde)
    flashColor(255, 255, 0, times, 150);
}

void AlertManager::showInfo(int times) {
    // Azul puro
    flashColor(0, 0, 255, times, 100);
}

// ==================== Control General ====================
void AlertManager::beep(int duration) {
    digitalWrite(ZUMBADOR, HIGH);
    delay(duration);
    digitalWrite(ZUMBADOR, LOW);
}

void AlertManager::allOff() {
    analogWrite(LED_RGB_RED, 0);
    analogWrite(LED_RGB_GREEN, 0);
    analogWrite(LED_RGB_BLUE, 0);
    digitalWrite(ZUMBADOR, LOW);
}
