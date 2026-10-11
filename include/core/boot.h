
#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "core/Global.h"
#include "features/watch.h"
#include "features/weather.h"
#include "update/ota_update.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

namespace BootUI
{

    void playBootAnimation()
    {
        const int cx = 64;
        const int cy = 32;

        // Draw the complete MTR logo.

        auto drawMTRLogo = []()
        {
            u8g2.setDrawColor(1);

            // Main MTR logo
            u8g2.setFont(u8g2_font_helvB24_tf);

            const char *logo = "MTR";
            int logoWidth = u8g2.getStrWidth(logo);
            int logoX = (128 - logoWidth) / 2;

            u8g2.drawStr(logoX, 34, logo);

            // Short underline
            int lineWidth = logoWidth + 6;
            u8g2.drawHLine((128 - lineWidth) / 2, 39, lineWidth);

            // Small subtitle
            u8g2.setFont(u8g2_font_5x8_tf);
            const char *subtitle = "MR THORIN RAPTOR";
            int subWidth = u8g2.getStrWidth(subtitle);
            u8g2.drawStr((128 - subWidth) / 2, 48, subtitle);

            // Small signature: 4ngry
            u8g2.setFont(u8g2_font_6x10_tf);
            const char *signature = "4ngry";
            int sigWidth = u8g2.getStrWidth(signature);
            u8g2.drawStr((128 - sigWidth) / 2, 61, signature);
        };

        // ---------------------------------
        // 1. Expanding circle wipe
        // ---------------------------------
        for (int r = 0; r <= 80; r += 4)
        {
            u8g2.clearBuffer();
            u8g2.setDrawColor(1);
            u8g2.drawDisc(cx, cy, r);
            u8g2.sendBuffer();
            delay(10);
        }

        // ---------------------------------
        // 2. Inverse circle wipe
        // ---------------------------------
        for (int r = 0; r <= 80; r += 4)
        {
            u8g2.clearBuffer();

            u8g2.setDrawColor(1);
            u8g2.drawDisc(cx, cy, 80);

            u8g2.setDrawColor(0);
            u8g2.drawDisc(cx, cy, r);

            u8g2.sendBuffer();
            delay(10);
        }

        // ---------------------------------
        // 3. Animated horizontal logo reveal
        // ---------------------------------
        for (int width = 0; width <= 64; width += 8)
        {
            u8g2.clearBuffer();

            // Moving scan lines
            u8g2.setDrawColor(1);
            u8g2.drawHLine(0, 15, width);
            u8g2.drawHLine(128 - width, 48, width);

            // Reveal logo after the first frames
            if (width >= 24)
            {
                drawMTRLogo();
            }

            u8g2.sendBuffer();
            delay(35);
        }

        // ---------------------------------
        // 4. Brief inverted flash
        // ---------------------------------
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, 0, 128, 64);

        u8g2.setDrawColor(0);
        u8g2.setFont(u8g2_font_helvB24_tf);

        const char *logo = "MTR";
        int logoWidth = u8g2.getStrWidth(logo);
        u8g2.drawStr((128 - logoWidth) / 2, 36, logo);

        u8g2.sendBuffer();
        delay(100);

        // ---------------------------------
        // 5. Final clean logo
        // ---------------------------------
        u8g2.clearBuffer();
        drawMTRLogo();
        u8g2.sendBuffer();

        delay(2200);
    }

    // ==================================================
    // Init Function Type
    // ==================================================

    typedef void (*InitFunction)();

    struct InitTask
    {
        const char *name;
        InitFunction function;
    };

    void initDisplay()
    {
        u8g2.begin();
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_5x7_tf);
    }
    void initDriver()
    {
        pinMode(TOUCH_PIN, INPUT);

        Wire.begin(OLED_SDA, OLED_SCL); // i2c initialize
    }

    void initWiFi()
    {
        Global::WiFiOn();
    }

    // void initUpdate()
    // {
    //     checkfirmwareUpdate();
    // }

    void initWeather()
    {
        Serial.println("Waiting for WiFi before fetching weather...");

        const unsigned long startTime = millis();
        const unsigned long wifiTimeout = 15000;

        while (WiFi.status() != WL_CONNECTED &&
               millis() - startTime < wifiTimeout)
        {
            Global::updateWiFi();
            delay(100);
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Weather skipped: WiFi connection timeout");
            return;
        }

        fetchWeatherWithRetry();
    }

    void initTime()
    {
        initLocalTime();
    }

    InitTask tasks[] =
        {
            {"Driver", initDriver},
            {"Display", initDisplay},

            {"WIFI", initWiFi},
            {"TIME", initTime},
            // {"Update", initUpdate},
            {"WEATHER", initWeather},
    };

    const uint8_t TASK_COUNT =
        sizeof(tasks) / sizeof(tasks[0]);

    // ==================================================
    // Progress Bar
    // ==================================================

    void drawProgress(uint8_t progress)
    {
        const uint8_t x = 14;
        const uint8_t y = 42;
        const uint8_t w = 100;
        const uint8_t h = 7;

        u8g2.drawRFrame(x, y, w, h, 2);

        uint8_t fill = map(progress, 0, 100, 0, w - 4);

        if (fill > 0)
        {
            u8g2.drawBox(
                x + 2,
                y + 2,
                fill,
                h - 4);
        }
    }

    // ==================================================
    // Boot Screen
    // ==================================================

    void draw(
        uint8_t frame,
        uint8_t progress,
        const char *currentTask)
    {
        u8g2.clearBuffer();

        // ----------------------------------------------
        // Brand
        // ----------------------------------------------

        u8g2.setFont(u8g2_font_10x20_tf);

        const char *name = "4NGRY";

        uint8_t textWidth =
            u8g2.getStrWidth(name);

        uint8_t textX =
            (128 - textWidth) / 2;

        u8g2.drawStr(
            textX,
            15,
            name);

        // ----------------------------------------------
        // Initializing text
        // ----------------------------------------------

        u8g2.setFont(u8g2_font_5x7_tf);

        const char *base = "INITIALIZING";

        uint8_t baseWidth =
            u8g2.getStrWidth(base);

        uint8_t baseX =
            (128 - baseWidth) / 2;

        u8g2.drawStr(
            baseX,
            27,
            base);

        // Animated dots
        uint8_t dots =
            (frame / 6) % 4;

        uint8_t dotX =
            baseX + baseWidth + 3;

        for (uint8_t i = 0; i < dots; i++)
        {
            u8g2.drawDisc(
                dotX + (i * 4),
                32,
                1);
        }

        // ----------------------------------------------
        // Current task
        // ----------------------------------------------

        if (currentTask != nullptr)
        {
            u8g2.setFont(u8g2_font_4x6_tf);

            uint8_t taskWidth =
                u8g2.getStrWidth(currentTask);

            uint8_t taskX =
                (128 - taskWidth) / 2;

            u8g2.drawStr(
                taskX,
                36,
                currentTask);
        }

        // ----------------------------------------------
        // Progress
        // ----------------------------------------------

        drawProgress(progress);

        // ----------------------------------------------
        // Percentage
        // ----------------------------------------------

        u8g2.setFont(u8g2_font_4x6_tf);

        char percent[5];

        snprintf(
            percent,
            sizeof(percent),
            "%d%%",
            progress);

        uint8_t percentWidth =
            u8g2.getStrWidth(percent);

        u8g2.drawStr(
            128 - percentWidth - 4,
            58,
            percent);

        // ----------------------------------------------
        // System indicator
        // ----------------------------------------------

        u8g2.drawDisc(7, 56, 1);

        u8g2.setCursor(11, 58);
        u8g2.print("MTR");

        u8g2.sendBuffer();
    }

    // ==================================================
    // Run Boot
    // ==================================================

    void run()
    {
        const uint16_t FRAME_DELAY = 35;

        for (uint8_t i = 0; i < TASK_COUNT; i++)
        {
            // ------------------------------------------
            // Calculate progress
            // ------------------------------------------

            uint8_t progress =
                (i * 100) / TASK_COUNT;

            // ------------------------------------------
            // Show current initialization
            // ------------------------------------------

            draw(
                i * 10,
                progress,
                tasks[i].name);

            delay(100);

            // ------------------------------------------
            // ACTUALLY INITIALIZE
            // ------------------------------------------

            tasks[i].function();

            // ------------------------------------------
            // Show completed progress
            // ------------------------------------------

            progress =
                ((i + 1) * 100) / TASK_COUNT;

            draw(
                i * 10,
                progress,
                tasks[i].name);

            delay(FRAME_DELAY);
        }

        // ----------------------------------------------
        // Finished
        // ----------------------------------------------

        draw(
            100,
            100,
            "READY");

        delay(300);
    }

}
