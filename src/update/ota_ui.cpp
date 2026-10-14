#include <Arduino.h>
#include <U8g2lib.h>
#include "update/ota_ui.h"
#include "version.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void otaDrawScreen(OtaState state,
                   int progress,
                   size_t bytesDone,
                   size_t bytesTotal,
                   const char *version,
                   const char *msg)
{
    static uint8_t animFrame = 0;
    static unsigned long lastAnim = 0;

    if (millis() - lastAnim >= 80)
    {
        animFrame = (animFrame + 1) % 16;
        lastAnim = millis();
    }

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    // -------------------------------------------------------------
    // TOP HEADER BANNER
    // -------------------------------------------------------------
    // Dark inverse header bar
    u8g2.drawBox(0, 0, 128, 12);
    u8g2.setDrawColor(0); // White on Black
    u8g2.setFont(u8g2_font_6x10_tf);

    const char *headerTitle = "4NGRY OTA UPDATE";
    int headerWidth = u8g2.getStrWidth(headerTitle);
    u8g2.drawStr((128 - headerWidth) / 2, 9, headerTitle);

    u8g2.setDrawColor(1); // Normal drawing

    // -------------------------------------------------------------
    // SUB-HEADER: VERSION
    // -------------------------------------------------------------
    u8g2.setFont(u8g2_font_5x8_tf);
    char verStr[32];
    if (version && strlen(version) > 0)
    {
        snprintf(verStr, sizeof(verStr), "v%s -> v%s", FIRMWARE_VERSION, version);
    }
    else
    {
        snprintf(verStr, sizeof(verStr), "Current: v%s", FIRMWARE_VERSION);
    }
    int verWidth = u8g2.getStrWidth(verStr);
    u8g2.drawStr((128 - verWidth) / 2, 22, verStr);

    // -------------------------------------------------------------
    // MAIN CONTENT ACCORDING TO STATE
    // -------------------------------------------------------------
    switch (state)
    {
    case OTA_STATE_CHECKING:
    {
        // Animated radar / spinner in center
        int cx = 64;
        int cy = 37;
        u8g2.drawCircle(cx, cy, 8);
        int angleIdx = animFrame % 8;
        float a = angleIdx * (3.14159f / 4.0f);
        u8g2.drawLine(cx, cy, cx + (int)(7 * cosf(a)), cy + (int)(7 * sinf(a)));

        u8g2.setFont(u8g2_font_5x8_tf);
        const char *t = msg ? msg : "Checking GitHub releases...";
        int tw = u8g2.getStrWidth(t);
        u8g2.drawStr((128 - tw) / 2, 53, t);

        u8g2.drawHLine(14, 60, 100);
        break;
    }

    case OTA_STATE_DOWNLOADING:
    {
        progress = constrain(progress, 0, 100);

        // Progress bar container
        const int barX = 10;
        const int barY = 26;
        const int barW = 108;
        const int barH = 11;

        u8g2.drawRFrame(barX, barY, barW, barH, 2);

        // Progress fill
        int innerW = barW - 4;
        int fillW = (innerW * progress) / 100;
        if (fillW > 0)
        {
            u8g2.drawBox(barX + 2, barY + 2, fillW, barH - 4);
        }

        // Animated barber-pole or marker on edge of bar
        if (fillW > 2 && fillW < innerW)
        {
            u8g2.setDrawColor(0);
            u8g2.drawVLine(barX + 2 + fillW - 1, barY + 2, barH - 4);
            u8g2.setDrawColor(1);
        }

        // Stats row: KB and %
        u8g2.setFont(u8g2_font_5x8_tf);

        char sizeStr[24];
        if (bytesTotal > 0)
        {
            snprintf(sizeStr, sizeof(sizeStr), "%u/%u KB",
                     (unsigned int)(bytesDone / 1024),
                     (unsigned int)(bytesTotal / 1024));
        }
        else
        {
            snprintf(sizeStr, sizeof(sizeStr), "%u KB",
                     (unsigned int)(bytesDone / 1024));
        }
        u8g2.drawStr(12, 47, sizeStr);

        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", progress);
        int pw = u8g2.getStrWidth(pctStr);
        u8g2.drawStr(116 - pw, 47, pctStr);

        // Bottom activity / warning
        u8g2.setFont(u8g2_font_4x6_tf);
        const char *warn = "DO NOT DISCONNECT POWER";
        int ww = u8g2.getStrWidth(warn);
        u8g2.drawStr((128 - ww) / 2, 58, warn);

        // Animated dots on bottom edge
        int dotOffset = (animFrame % 6) * 4;
        u8g2.drawPixel(6 + dotOffset, 62);
        u8g2.drawPixel(121 - dotOffset, 62);
        break;
    }

    case OTA_STATE_FLASHING:
    {
        // 100% full bar
        u8g2.drawRFrame(10, 26, 108, 11, 2);
        u8g2.drawBox(12, 28, 104, 7);

        u8g2.setFont(u8g2_font_6x10_tf);
        const char *flashingText = "FLASHING ROM...";
        int fw = u8g2.getStrWidth(flashingText);
        u8g2.drawStr((128 - fw) / 2, 48, flashingText);

        u8g2.setFont(u8g2_font_5x8_tf);
        const char *sub = "Verifying firmware...";
        int sw = u8g2.getStrWidth(sub);
        u8g2.drawStr((128 - sw) / 2, 59, sub);
        break;
    }

    case OTA_STATE_SUCCESS:
    {
        u8g2.drawRFrame(8, 25, 112, 35, 3);

        u8g2.setFont(u8g2_font_6x10_tf);
        const char *succ = "UPDATE COMPLETE!";
        int sw = u8g2.getStrWidth(succ);
        u8g2.drawStr((128 - sw) / 2, 38, succ);

        u8g2.setFont(u8g2_font_5x8_tf);
        const char *reboot = "Rebooting in 2 sec...";
        int rw = u8g2.getStrWidth(reboot);
        u8g2.drawStr((128 - rw) / 2, 51, reboot);
        break;
    }

    case OTA_STATE_ERROR:
    {
        u8g2.drawRFrame(8, 25, 112, 35, 3);

        u8g2.setFont(u8g2_font_6x10_tf);
        const char *errTitle = "UPDATE FAILED";
        int ew = u8g2.getStrWidth(errTitle);
        u8g2.drawStr((128 - ew) / 2, 38, errTitle);

        u8g2.setFont(u8g2_font_5x8_tf);
        const char *reason = msg ? msg : "Network / Hash error";
        int rw = u8g2.getStrWidth(reason);
        u8g2.drawStr((128 - rw) / 2, 51, reason);
        break;
    }

    default:
        break;
    }

    u8g2.sendBuffer();
}

void otaUpdateScreen(int progress)
{
    otaDrawScreen(OTA_STATE_DOWNLOADING, progress);
}