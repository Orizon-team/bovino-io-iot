#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

class DisplayManager {
public:
    DisplayManager();
    void initialize();
    void showWelcome();
    void showWiFiStatus(bool connecting, const String& ssid = "");
    void showIP(const String& ip);
    void showWiFiError();
    void showScanning(int stage);
    void showDevicesDetected(int stage, int count);
    void showNoDevices(int stage);
    void showPostStatus(int stage, int attempt);
    void showPostSuccess(int stage, int httpCode);
    void showHTTPError(int stage, int errorCode);
    void showServerError(int httpCode);
    void showComplete();
    void clear();
    void showMessage(const String& line1, const String& line2 = "");

private:
    LiquidCrystal_I2C* lcd;
    String truncate(const String& text, int maxLength = 16);
};

extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
