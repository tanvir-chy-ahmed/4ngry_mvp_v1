#include <Arduino.h>
#include <U8g2lib.h>
#include "all_expressions.h"
#include <WiFi.h>
#include "time.h"
#include "features/watch.h"
#include "features/weather.h"
#include "core/boot.h"
#include "version.h"
#include "core/Global.h"
#include "update/ota_update.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

inline void drawWiFiStatusUI()
{
    Global::WiFiState state = Global::getWiFiState();

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    switch (state)
    {
    case Global::WIFI_NO_CREDENTIALS:
    case Global::WIFI_SETUP_PORTAL:
    {
        // Header
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x10_tf);
        const char *title = "4NGRY WI-FI SETUP";
        int tw = u8g2.getStrWidth(title);
        u8g2.drawStr((128 - tw) / 2, 9, title);

        u8g2.setDrawColor(1);

        // Information frame
        u8g2.drawRFrame(6, 16, 116, 36, 2);

        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.drawStr(12, 26, "AP:   4ngryROBO");
        u8g2.drawStr(12, 36, "Pass: 12345678");
        u8g2.drawStr(12, 47, "URL:  192.168.4.1");

        u8g2.setFont(u8g2_font_4x6_tf);
        const char *hint = "Connect from phone/PC to configure";
        int hw = u8g2.getStrWidth(hint);
        u8g2.drawStr((128 - hw) / 2, 60, hint);
        break;
    }

    case Global::WIFI_CONNECTING:
    {
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x10_tf);
        const char *title = "CONNECTING WI-FI";
        int tw = u8g2.getStrWidth(title);
        u8g2.drawStr((128 - tw) / 2, 9, title);

        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(10, 32, "Connecting...");

        u8g2.setFont(u8g2_font_5x8_tf);
        String ssid = Global::getSavedSSID();
        if (ssid.length() > 0)
        {
            u8g2.drawStr(10, 46, ("SSID: " + ssid).c_str());
        }
        u8g2.drawStr(10, 58, "Please wait...");
        break;
    }

    case Global::WIFI_UNAVAILABLE:
    {
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x10_tf);
        const char *title = "WI-FI UNAVAILABLE";
        int tw = u8g2.getStrWidth(title);
        u8g2.drawStr((128 - tw) / 2, 9, title);

        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(10, 32, "Network Offline");

        u8g2.setFont(u8g2_font_5x8_tf);
        uint32_t retrySec = Global::getSecondsUntilRetry();
        char rBuf[32];
        snprintf(rBuf, sizeof(rBuf), "Retrying in %us...", retrySec);
        u8g2.drawStr(10, 46, rBuf);
        u8g2.drawStr(10, 58, "Retrying in background");
        break;
    }

    case Global::WIFI_CONNECTED:
    {
        u8g2.drawBox(0, 0, 128, 12);
        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_6x10_tf);
        const char *title = "WI-FI CONNECTED";
        int tw = u8g2.getStrWidth(title);
        u8g2.drawStr((128 - tw) / 2, 9, title);

        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x12_tf);
        u8g2.drawStr(10, 35, "Network Online");
        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.drawStr(10, 50, "Ready");
        break;
    }
    }

    u8g2.sendBuffer();
}

void setup()
{
    Serial.begin(115200);
    BootUI::initDriver();
    BootUI::initDisplay();

    randomSeed(analogRead(0) ^ (analogRead(1) << 8));
    lastScreenActivityMs = millis();

    // Start in IDLE
    initExpression(IDLE, anim);
    exprStartMs = millis();
    scheduleIdleBlink(millis());
    scheduleIdleLook(millis());
    Serial.println("Current Firmware Version: " + String(FIRMWARE_VERSION));

    BootUI::playBootAnimation();
    BootUI::initWiFi();
    BootUI::initTime();
    BootUI::initWeather();
}

void loop()
{
    Global::updateWiFi();

    Global::WiFiState state = Global::getWiFiState();

    // Only stay locked in WiFi screen if initial setup is required
    if (state == Global::WIFI_NO_CREDENTIALS || state == Global::WIFI_SETUP_PORTAL)
    {
        drawWiFiStatusUI();
        return;
    }

    unsigned long now = millis();

    // Handle touch interactions
    handleTouch(now);

    // Update local time and background NTP
    static uint32_t lastTimeUpdate = 0;
    if (now - lastTimeUpdate >= 250)
    {
        lastTimeUpdate = now;
        updateLocalTime();
    }

    // Background periodic weather updates
    updateWeatherPeriodically();

    // Background periodic OTA update checks
    checkFirmwareUpdatePeriodically();

    // Frame-rate gate for animations
    if (now - lastFrameMs < FRAME_MS)
        return;

    lastFrameMs = now;
    float ms = (float)now;

    // ========================================================
    // EXPRESSION TRANSITION
    // ========================================================
    if (transitioning)
    {
        transitionT += 0.08f;

        if (transitionT >= 1.0f)
        {
            transitioning = false;

            currentExpr = nextExpr;
            exprStartMs = now;

            initExpression(currentExpr, anim);

            if (currentExpr == IDLE)
            {
                scheduleIdleBlink(now);
                scheduleIdleLook(now);
            }
        }
        else
        {
            float squeeze;

            if (transitionT < 0.5f)
                squeeze = 1.0f - easeIn(transitionT * 2.0f);
            else
                squeeze = easeOut((transitionT - 0.5f) * 2.0f);

            anim.eyeScaleX = squeeze;
            anim.eyeScaleY = squeeze;
            anim.bounce = (1.0f - squeeze) * -3.0f;
        }
    }
    else
    {
        // ====================================================
        // IDLE BEHAVIOR
        // ====================================================
        if (currentExpr == IDLE)
        {
            // Scheduled blink.
            if (now >= idleNextBlink)
            {
                float blinkPhase =
                    fmodf((now - idleNextBlink) / 200.0f, 1.0f);

                float c;

                if (blinkPhase < 0.5f)
                    c = easeIn(blinkPhase * 2.0f);
                else
                    c = easeOut((1.0f - blinkPhase) * 2.0f);

                anim.eyeScaleY = 1.0f - 0.9f * c;

                if (blinkPhase > 0.95f)
                {
                    anim.eyeScaleY = 1.0f;
                    scheduleIdleBlink(now);
                }
            }

            // Scheduled gaze shift.
            if (now >= idleNextLook)
            {
                idleGazeTarget =
                    (float)random(-3, 4) / 3.0f;

                scheduleIdleLook(now);
            }

            // Smooth gaze movement.
            idleGazeX +=
                (idleGazeTarget - idleGazeX) * 0.06f;
        }

        // Update the existing expression animation.
        updateExpression(currentExpr, anim, ms);
    }

    // ========================================================
    // RENDER
    // ========================================================
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    renderScreen(currentScreen);

    u8g2.sendBuffer();
}