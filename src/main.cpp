#include <Arduino.h>
#include <U8g2lib.h>
#include "all_expressions.h"
#include <WiFi.h>
#include "time.h"
#include "features/watch.h"
#include "core/boot.h"
#include "version.h"
#include "features/watch.h"
#include "core/Global.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
//     U8G2_R0,
//     U8X8_PIN_NONE,
//     OLED_SCL,
//     OLED_SDA);
inline void drawWiFiStatusUI()
{
    Global::WiFiState state = Global::getWiFiState();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tf);

    switch (state)
    {
    case Global::WIFI_NO_CREDENTIALS:
    case Global::WIFI_SETUP_PORTAL:
        u8g2.drawStr(0, 12, "4ngryROBO Setup");
        u8g2.drawHLine(0, 17, 128);
        u8g2.drawStr(0, 31, "Connect to WiFi:");
        u8g2.drawStr(0, 43, "4ngryROBO");
        u8g2.drawStr(0, 55, "Pass: 12345678");
        u8g2.drawStr(0, 64, "Open 192.168.4.1");
        break;

    case Global::WIFI_CONNECTING:
        u8g2.drawStr(0, 12, "4ngryROBO");
        u8g2.drawHLine(0, 17, 128);
        u8g2.drawStr(0, 34, "Connecting to WiFi");
        u8g2.drawStr(0, 49, "Please wait...");
        break;

    case Global::WIFI_UNAVAILABLE:
        u8g2.drawStr(0, 12, "4ngryROBO");
        u8g2.drawHLine(0, 17, 128);
        u8g2.drawStr(0, 34, "WiFi unavailable");
        u8g2.drawStr(0, 48, "Retrying in background");
        break;

    case Global::WIFI_CONNECTED:
        u8g2.drawStr(0, 12, "WiFi Connected");
        u8g2.drawHLine(0, 17, 128);
        u8g2.drawStr(0, 34, "Network ready");
        break;
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

    if (state != Global::WIFI_CONNECTED)
    {
        drawWiFiStatusUI();
        return;
    }

    unsigned long now = millis();

    handleTouch(now);
    Global::updateWiFi();

    // Update local time once per second.
    static uint32_t lastTimeUpdate = 0;

    if (now - lastTimeUpdate >= 1000)
    {
        lastTimeUpdate = now;
        updateLocalTime();
    }

    // Frame-rate gate.
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