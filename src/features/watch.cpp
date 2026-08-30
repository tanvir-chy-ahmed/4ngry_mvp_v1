#include <U8g2lib.h>
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include "features/watch.h"
#include "features/weather.h"
const long gmtOffset_sec = 6 * 3600; // utc 6
const int daylightOffset_sec = 0; // BD dont save any offset time so 0

int hour = 0;
int minute = 0;
int second = 0;

int day = 0;
int month = 0;
int year = 0;
int weekday = 0;
struct tm timeinfo;
const char *weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char *monthname[] = {
    "JAN", "FEB", "MAR", "APR",
    "MAY", "JUN", "JUL", "AUG",
    "SEP", "OCT", "NOV", "DEC"};

void getLocalTimeInfo()
{
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to Obtain Time");
        return;
    }

    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
    weekday = timeinfo.tm_wday;
   Serial.println("Time synchronized successfully");

    // Bangladesh = UTC+6
}

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

static uint8_t watchFrame = 0;

static void drawWatchInfo()
{
    char timeStr[6];
    char dateStr[12];
    char yearStr[5];
    char secondStr[3];

    snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
             hour, minute);

    snprintf(dateStr, sizeof(dateStr), "%02d %s",
             day, monthname[month - 1]);

    snprintf(yearStr, sizeof(yearStr), "%04d",
             year);

    snprintf(secondStr, sizeof(secondStr), "%02d",
             second);

    // Main time
    u8g2.setFont(u8g2_font_logisoso32_tf);
    u8g2.drawStr(12, 43, timeStr);

    // Top information
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(5, 9, weekdays[weekday]);
    u8g2.drawStr(49, 9, dateStr);
    u8g2.drawStr(106, 9, secondStr);

    // Bottom information
    u8g2.drawStr(5, 61, yearStr);
    u8g2.drawStr(105, 61, "24H");

    // Minimal horizontal accents
    u8g2.drawHLine(4, 51, 18);
    u8g2.drawHLine(106, 51, 18);

    // Center indicator
    u8g2.drawDisc(64, 55, 1);
}
static void drawWatchBackground()
{
    // Subtle floating particles around the edges
    for (int i = 0; i < 8; i++)
    {
        int x = (i * 19 + watchFrame / 3) % 128;
        int y = 5 + ((i * 11) % 54);

        // Keep center clean for the time
        if (x > 25 && x < 103 && y > 18 && y < 47)
            continue;

        u8g2.drawPixel(x, y);
    }

    // Tiny corner accents
    uint8_t p = (watchFrame / 4) % 3;

    u8g2.drawPixel(2 + p, 17);
    u8g2.drawPixel(125 - p, 17);
    u8g2.drawPixel(2 + p, 47);
    u8g2.drawPixel(125 - p, 47);
}

// ============================================================
// MAIN UI FUNCTION
// ============================================================

void watchUI()
{
    u8g2.clearBuffer();
    drawWatchBackground();
    drawWatchInfo();
    u8g2.sendBuffer();

    watchFrame++;
}