#ifndef ALERTS_H
#define ALERTS_H
#include <Arduino.h>
#include "config.h"

class AlertManager {
public:
    AlertManager();
    void initialize();
    void loaderOn();
    void loaderOff();
    void loaderToggle();
    void showSuccess(int times = 3);
    void showError(int times = 3);
    void showWarning(int times = 3);
    void showInfo(int times = 3);
    void beep(int duration = 200);
    void allOff();
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
private:
    void flashColor(uint8_t red, uint8_t green, uint8_t blue, int times, int delayMs);
    bool loaderState;
};

extern AlertManager alertManager;

#endif
