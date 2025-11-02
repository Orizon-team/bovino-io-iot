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
    void showDanger(int times = 3);
    void flashLED(int ledPin, int times = 3, int delayMs = 100);
    void beep(int duration = 200);
    void allOff();

private:
    bool loaderState;
};

extern AlertManager alertManager;

#endif // ALERTS_H
