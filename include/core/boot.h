
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

    // ==================================================
    // Init Function Type
    // ==================================================

    typedef void (*InitFunction)();

    struct InitTask
    {
        const char *name;
        InitFunction function;
    };

    // ==================================================
    // Add your initialization functions here
    // ==================================================

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
        WiFiOn();
    }
    
    void initUpdate()
    {
        checkfirmwareUpdate();
    }

    void initTime()
    {
        getLocalTimeInfo();
    }

    void initWeather()
    {
        getWeatherInfo();
    }

    // ==================================================
    // INIT LIST
    // Just add your functions here
    // ==================================================

    InitTask tasks[] =
        {
            {"Driver", initDriver},
            {"Display", initDisplay},

            {"WIFI", initWiFi},
            {"Update", initUpdate},

            {"TIME", initTime},
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
