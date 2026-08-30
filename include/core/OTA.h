#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void otaUpdateScreen(int progress)
{
    static uint8_t frame = 0;
    static unsigned long lastFrame = 0;

    // Animate every 120 ms
    if (millis() - lastFrame >= 120)
    {
        frame = (frame + 1) % 8;
        lastFrame = millis();
    }

    u8g2.clearBuffer();

    // -------------------------
    // Header
    // -------------------------
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(4, 10, "4NGRY");
    u8g2.drawStr(70, 10, "OTA UPDATE");

    // Divider
    u8g2.drawHLine(4, 14, 120);

    // -------------------------
    // Animated spinner
    // -------------------------
    const char spinner[] = {'|', '/', '-', '\\'};

    u8g2.setFont(u8g2_font_10x20_tf);

    char spin[2];
    spin[0] = spinner[frame % 4];
    spin[1] = '\0';

    u8g2.drawStr(8, 38, spin);

    // -------------------------
    // Status
    // -------------------------
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(27, 29, "UPDATING FIRMWARE");

    // -------------------------
    // Progress bar
    // -------------------------
    int barX = 27;
    int barY = 34;
    int barW = 94;
    int barH = 10;

    u8g2.drawFrame(barX, barY, barW, barH);

    progress = constrain(progress, 0, 100);

    int fillW = ((barW - 2) * progress) / 100;

    if (fillW > 0)
        u8g2.drawBox(barX + 1, barY + 1, fillW, barH - 2);

    // -------------------------
    // Percentage
    // -------------------------
    char percent[8];
    sprintf(percent, "%d%%", progress);

    u8g2.setFont(u8g2_font_6x10_tf);

    int textWidth = u8g2.getStrWidth(percent);

    u8g2.drawStr(
        64 - textWidth / 2,
        56,
        percent
    );

    u8g2.sendBuffer();
}