#include "features/extra_screens.h"
#include "features/weather.h"
#include <Preferences.h>

#ifndef BUZZER_PIN
#define BUZZER_PIN 3
#endif

namespace ExtraScreens
{
    static bool timerScreen = false;
    static bool forecastScreen = false;
    static bool timerRunning = false;
    static uint8_t selectedMinutes = 1;
    static uint32_t timerEndsAt = 0;
    static uint8_t brightness = 255;  // default brightness
    static bool buzzerEnabled = true;
    static uint8_t buzzerVolume = 60;
    static bool pwmAttached = false;
    static uint8_t alarmPhase = 0;
    static uint32_t alarmNextAt = 0;
    static bool alarmActive = false;

    static void ensureBuzzerPWM()
    {
        if (!pwmAttached)
            pwmAttached = ledcAttach(BUZZER_PIN, 2000, 8);
    }
    static void buzzerTone(uint16_t frequency)
    {
        if (!buzzerEnabled || frequency == 0)
        {
            if (pwmAttached)
            {
                ledcWrite(BUZZER_PIN, 0);
                ledcWriteTone(BUZZER_PIN, 0);
            }
            return;
        }
        ensureBuzzerPWM();
        if (pwmAttached)
        {
            ledcWriteTone(BUZZER_PIN, frequency);
            ledcWrite(BUZZER_PIN, (uint32_t)buzzerVolume * 255U / 100U);
        }
    }
    void loadDeviceSettings()
    {
        Preferences p;
        if (p.begin("4ngryROBO", true))
        {
            brightness = p.getUChar("oled_bright", 180);
            buzzerEnabled = p.getBool("buzzer_on", true);
            buzzerVolume = p.getUChar("buzzer_vol", 60);
            p.end();
        }
        ensureBuzzerPWM();
        buzzerTone(0);
    }
    void setBrightness(uint8_t value, U8G2 &display)
    {
        brightness = value;
        display.setContrast(brightness);
        Preferences p;
        if (p.begin("4ngryROBO", false))
        {
            p.putUChar("oled_bright", brightness);
            p.end();
        }
    }
    uint8_t getBrightness() { return brightness; }
    void setBuzzerEnabled(bool enabled)
    {
        buzzerEnabled = enabled;
        Preferences p;
        if (p.begin("4ngryROBO", false))
        {
            p.putBool("buzzer_on", enabled);
            p.end();
        }
        if (!enabled)
        {
            buzzerTone(0);
            alarmActive = false;
        }
    }
    bool isBuzzerEnabled() { return buzzerEnabled; }
    void setBuzzerVolume(uint8_t percent)
    {
        buzzerVolume = percent > 100 ? 100 : percent;
        Preferences p;
        if (p.begin("4ngryROBO", false))
        {
            p.putUChar("buzzer_vol", buzzerVolume);
            p.end();
        }
    }
    uint8_t getBuzzerVolume() { return buzzerVolume; }
    void playBuzzerDemo(uint8_t sound)
    {
        if (!buzzerEnabled)
            return;
        static const uint16_t tones[][4] = {{880, 1175, 1568, 0}, {660, 880, 660, 0}, {523, 659, 784, 1047}};
        sound = sound > 2 ? 0 : sound;
        for (int i = 0; i < 4; ++i)
        {
            if (!tones[sound][i])
                break;
            buzzerTone(tones[sound][i]);
            delay(90);
            buzzerTone(0);
            delay(15);
        }
        buzzerTone(0);
    }
    void playBuzzerAlarm()
    {
        if (!buzzerEnabled)
            return;
        alarmActive = true;
        alarmPhase = 0;
        alarmNextAt = millis();
    }
    void enterTimer()
    {
        timerScreen = true;
        forecastScreen = false;
        timerRunning = false;
        selectedMinutes = 1;
        alarmActive = false;
        playBuzzerDemo(0);
    }
    void timerTap()
    {
        if (timerRunning)
        {
            timerRunning = false;
            timerScreen = false;
            return;
        }
        selectedMinutes = selectedMinutes >= 60 ? 1 : selectedMinutes + 1;
    }
    void timerHold()
    {
        if (!timerRunning)
        {
            timerRunning = true;
            timerEndsAt = millis() + (uint32_t)selectedMinutes * 60000UL;
        }
    }
    bool timerActive() { return timerScreen; }
    void updateTimer()
    {
        if (timerRunning && (int32_t)(millis() - timerEndsAt) >= 0)
        {
            timerRunning = false;
            timerScreen = false;
            playBuzzerAlarm();
        }
        if (alarmActive && buzzerEnabled && millis() >= alarmNextAt)
        {
            if (alarmPhase >= 6)
            {
                buzzerTone(0);
                alarmActive = false;
            }
            else
            {
                buzzerTone((alarmPhase % 2) ? 1400 : 900);
                ++alarmPhase;
                alarmNextAt = millis() + 120;
            }
        }
    }
    void renderTimer(U8G2 &d)
    {
        d.clearBuffer();
        d.setDrawColor(1);
        d.setFont(u8g2_font_6x10_tf);
        if (timerRunning)
        {
            int32_t rem = (int32_t)(timerEndsAt - millis());
            if (rem < 0)
                rem = 0;
            uint32_t tenths = (uint32_t)rem / 100UL;
            char t[20];
            snprintf(t, sizeof(t), "%02lu:%02lu.%lu", (unsigned long)(tenths / 600), (unsigned long)((tenths / 10) % 60), (unsigned long)(tenths % 10));
            d.setFont(u8g2_font_logisoso24_tf);
            d.drawStr(25, 40, t);
            d.setFont(u8g2_font_5x8_tf);
            d.drawStr(14, 54, "Countdown running");
            d.drawStr(25, 63, "Single Tap to cancel");
        }
        else
        {
            char m[20];
            snprintf(m, sizeof(m), "%02u m", selectedMinutes);
            d.setFont(u8g2_font_logisoso24_tf);
            d.drawStr(22, 39, m);
            d.setFont(u8g2_font_5x8_tf);
            d.drawStr(13, 63, "Hold 3s to Start");
        }
        d.sendBuffer();
    }
    void enterForecast()
    {
        timerScreen = false;
        forecastScreen = true;
        playBuzzerDemo(1);
        fetchThreeDayForecast();
    }
    bool forecastActive() { return forecastScreen; }
    void renderForecast(U8G2 &d) { drawThreeDayForecast(d); }
    void exitSpecialScreen()
    {
        timerScreen = false;
        forecastScreen = false;
        timerRunning = false;
    }
}
