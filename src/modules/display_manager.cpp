#include "display_manager.h"

// Definición de la instancia global
DisplayManager displayManager;

DisplayManager::DisplayManager() {
    lcd = new LiquidCrystal_I2C(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
}
// ==================== Inicialización ====================
void DisplayManager::initialize() {
    lcd->init();           // Inicializar LCD I2C
    lcd->backlight();      // Encender la retroiluminación
    Serial.println("[LCD] Pantalla LCD I2C inicializada");
    Serial.printf("[LCD] Dirección I2C: 0x%02X, Tamaño: %dx%d\n", 
                  LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
}

// ==================== Mensajes Generales ====================
void DisplayManager::showWelcome() {
    clear();
    lcd->print("Waiting for");
    lcd->setCursor(0, 1);
    lcd->print("Connect");
}

void DisplayManager::clear() {
    lcd->clear();
}

void DisplayManager::showMessage(const String& line1, const String& line2) {
    clear();
    lcd->print(truncate(line1));
    if (line2.length() > 0) {
        lcd->setCursor(0, 1);
        lcd->print(truncate(line2));
    }
}

// ==================== Mensajes de WiFi ====================
void DisplayManager::showWiFiStatus(bool connecting, const String& ssid) {
    clear();
    if (connecting) {
        lcd->print("Conectando WiFi");
        if (ssid.length() > 0) {
            lcd->setCursor(0, 1);
            lcd->print(truncate(ssid));
        }
    } else {
        lcd->print("WiFi Conectado");
    }
}

void DisplayManager::showIP(const String& ip) {
    clear();
    lcd->print("WiFi Conectado");
    lcd->setCursor(0, 1);
    lcd->print("IP: ");
    lcd->print(truncate(ip, 11));
}

void DisplayManager::showWiFiError() {
    clear();
    lcd->print("WiFi ERROR");
    lcd->setCursor(0, 1);
    lcd->print("Sin conexion");
}

// ==================== Mensajes de Escaneo BLE ====================
void DisplayManager::showScanning(int stage) {
    clear();
    lcd->print("Sondeo ");
    lcd->print(stage);
    lcd->setCursor(0, 1);
    lcd->print("Escaneando BLE...");
}

void DisplayManager::showDevicesDetected(int stage, int count) {
    clear();
    lcd->print("Sondeo ");
    lcd->print(stage);
    lcd->print(": ");
    lcd->print(count);
    lcd->setCursor(0, 1);
    lcd->print("Enviando POST...");
}

void DisplayManager::showNoDevices(int stage) {
    clear();
    lcd->print("Sondeo ");
    lcd->print(stage);
    lcd->setCursor(0, 1);
    lcd->print("Sin dispositivos");
}

// ==================== Mensajes de HTTP ====================
void DisplayManager::showPostStatus(int stage, int attempt) {
    clear();
    lcd->print("POST Intento #");
    lcd->print(attempt);
    lcd->setCursor(0, 1);
    lcd->print("Etapa ");
    lcd->print(stage);
    lcd->print(" - Ping API");
}

void DisplayManager::showPostSuccess(int stage, int httpCode) {
    clear();
    lcd->print("POST Exitoso!");
    lcd->setCursor(0, 1);
    lcd->print("Etapa ");
    lcd->print(stage);
    lcd->print(" - ");
    lcd->print(httpCode);
}

void DisplayManager::showHTTPError(int stage, int errorCode) {
    clear();
    lcd->print("HTTP ERROR");
    lcd->setCursor(0, 1);
    lcd->print("Etapa ");
    lcd->print(stage);
    lcd->print(" - ");
    lcd->print(errorCode);
}

void DisplayManager::showServerError(int httpCode) {
    clear();
    
    switch (httpCode) {
        case 404:
            lcd->print("HTTP 404 Error");
            lcd->setCursor(0, 1);
            lcd->print("Reintentando...");
            break;
        
        case 500:
            lcd->print("Error Servidor");
            lcd->setCursor(0, 1);
            lcd->print("Reintentando...");
            break;
        
        case 502:
            lcd->print("Servidor Caido");
            lcd->setCursor(0, 1);
            lcd->print("Reintentando...");
            break;
        
        case 503:
            lcd->print("Servicio");
            lcd->setCursor(0, 1);
            lcd->print("No disponible");
            break;
        
        case 504:
            lcd->print("Timeout Gateway");
            lcd->setCursor(0, 1);
            lcd->print("Reintentando...");
            break;
        
        default:
            if (httpCode >= 400 && httpCode < 500) {
                lcd->print("Error Cliente");
                lcd->setCursor(0, 1);
                lcd->print("Cod: ");
                lcd->print(httpCode);
            } else if (httpCode >= 500) {
                lcd->print("Error Servidor");
                lcd->setCursor(0, 1);
                lcd->print("Cod: ");
                lcd->print(httpCode);
            } else {
                lcd->print("Error ");
                lcd->print(httpCode);
            }
            break;
    }
}

// ==================== Mensaje de Finalización ====================
void DisplayManager::showComplete() {
    clear();
    lcd->print("SONDEO DE CLASE");
    lcd->setCursor(0, 1);
    lcd->print("FINALIZADO!");
}

// ==================== Utilidades ====================
String DisplayManager::truncate(const String& text, int maxLength) {
    if (text.length() <= maxLength) {
        return text;
    }
    return text.substring(0, maxLength);
}
