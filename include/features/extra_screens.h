#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace ExtraScreens {
    void enterTimer();
    void timerTap();
    void timerHold();
    bool timerActive();
    void updateTimer();
    void renderTimer(U8G2 &display);
    void enterForecast();
    bool forecastActive();
    void renderForecast(U8G2 &display);
    void exitSpecialScreen();
    void loadDeviceSettings();
    void setBrightness(uint8_t value, U8G2 &display);
    uint8_t getBrightness();
    void setBuzzerEnabled(bool enabled);
    bool isBuzzerEnabled();
    void setBuzzerVolume(uint8_t percent);
    uint8_t getBuzzerVolume();
    void playBuzzerDemo(uint8_t sound);
    void playBuzzerAlarm();
}
