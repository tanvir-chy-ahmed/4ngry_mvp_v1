#include <U8g2lib.h>
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include "features/watch.h"
#include "features/weather.h"
#include <Preferences.h>

long gmtOffset_sec = 6 * 3600;
bool use24HourFormat = true;
const int daylightOffset_sec = 0;

int hour = 0;
int minute = 0;
int second = 0;

int day = 0;
int month = 0;
int year = 0;
int weekday = 0;

struct tm timeinfo;

const char *weekdays[] =
    {
        "SUN", "MON", "TUE", "WED",
        "THU", "FRI", "SAT"};

const char *monthname[] =
    {
        "JAN", "FEB", "MAR", "APR",
        "MAY", "JUN", "JUL", "AUG",
        "SEP", "OCT", "NOV", "DEC"};

bool timeSynced = false;
unsigned long lastClockUpdate = 0;
const unsigned long CLOCK_UPDATE_INTERVAL = 250;

// ============================================================
// INITIALIZE NTP
// ============================================================

void initLocalTime()
{
    Preferences prefs;
    if (prefs.begin("4ngryROBO", true)) {
        float offsetHours = prefs.getFloat("gmt_offset", 6.0f);
        use24HourFormat = prefs.getBool("clock_24h", true);
        prefs.end();
        if (offsetHours >= -12.0f && offsetHours <= 14.0f)
            gmtOffset_sec = (long)(offsetHours * 3600.0f);
    }
    configTime(
        gmtOffset_sec,
        daylightOffset_sec,
        "pool.ntp.org",
        "time.nist.gov");

    Serial.println("NTP configured");

    // Try several times because WiFi may be connected
    // before Internet/DNS is actually ready.
    for (int i = 0; i < 10; i++)
    {
        if (getLocalTime(&timeinfo, 1000))
        {
            timeSynced = true;

            Serial.println("Time synchronized successfully");
            return;
        }

        Serial.println("Waiting for NTP...");
        delay(500);
    }

    Serial.println("Failed to synchronize time");
}
void applyClockSettings(float offsetHours, bool use24Hour)
{
    if (offsetHours < -12.0f || offsetHours > 14.0f) return;
    gmtOffset_sec = (long)(offsetHours * 3600.0f);
    use24HourFormat = use24Hour;
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
}

// ============================================================
// UPDATE CLOCK VARIABLES
// ============================================================

void updateLocalTime()
{
    if (millis() - lastClockUpdate < CLOCK_UPDATE_INTERVAL)
        return;

    lastClockUpdate = millis();

    if (!getLocalTime(&timeinfo, 10))
    {
        timeSynced = false;
        return;
    }

    timeSynced = true;

    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    second = timeinfo.tm_sec;

    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
    weekday = timeinfo.tm_wday;
}

//! UI 1 -
// extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// static uint8_t watchFrame = 0;

// static void drawWatchInfo()
// {
//     char timeStr[6];
//     char dateStr[12];
//     char yearStr[5];
//     char secondStr[3];

//     snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
//              hour, minute);

//     snprintf(dateStr, sizeof(dateStr), "%02d %s",
//              day, monthname[month - 1]);

//     snprintf(yearStr, sizeof(yearStr), "%04d",
//              year);

//     snprintf(secondStr, sizeof(secondStr), "%02d",
//              second);

//     // Main time
//     u8g2.setFont(u8g2_font_logisoso32_tf);
//     u8g2.drawStr(12, 43, timeStr);

//     // Top information
//     u8g2.setFont(u8g2_font_5x8_tf);
//     u8g2.drawStr(5, 9, weekdays[weekday]);
//     u8g2.drawStr(49, 9, dateStr);
//     u8g2.drawStr(106, 9, secondStr);

//     // Bottom information
//     u8g2.drawStr(5, 61, yearStr);
//     u8g2.drawStr(105, 61, "24H");

//     // Minimal horizontal accents
//     u8g2.drawHLine(4, 51, 18);
//     u8g2.drawHLine(106, 51, 18);

//     // Center indicator
//     u8g2.drawDisc(64, 55, 1);
// }
// static void drawWatchBackground()
// {
//     // Subtle floating particles around the edges
//     for (int i = 0; i < 8; i++)
//     {
//         int x = (i * 19 + watchFrame / 3) % 128;
//         int y = 5 + ((i * 11) % 54);

//         // Keep center clean for the time
//         if (x > 25 && x < 103 && y > 18 && y < 47)
//             continue;

//         u8g2.drawPixel(x, y);
//     }

//     // Tiny corner accents
//     uint8_t p = (watchFrame / 4) % 3;

//     u8g2.drawPixel(2 + p, 17);
//     u8g2.drawPixel(125 - p, 17);
//     u8g2.drawPixel(2 + p, 47);
//     u8g2.drawPixel(125 - p, 47);
// }

// // ============================================================
// // MAIN UI FUNCTION
// // ============================================================

// void watchUI()
// {
//     u8g2.clearBuffer();
//     drawWatchBackground();
//     drawWatchInfo();
//     u8g2.sendBuffer();

//     watchFrame++;
// }

//! UI - 2

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

static uint8_t watchFrame = 0;

// ============================================================
// DIGITAL WATCH HEADER
// ============================================================
static void drawWatchHeader()
{
    char dateStr[12];

    snprintf(dateStr, sizeof(dateStr), "%02d %s",
             day, monthname[month - 1]);

    u8g2.setFont(u8g2_font_5x8_tf);

    // Day label
    u8g2.drawStr(5, 9, weekdays[weekday]);

    // Date aligned to the right
    int dateWidth = u8g2.getStrWidth(dateStr);
    u8g2.drawStr(123 - dateWidth, 9, dateStr);

    // Header separator
    u8g2.drawHLine(4, 13, 120);

    // Small status indicator
    u8g2.drawBox(4, 3, 2, 2);
}

// ============================================================
// LARGE DIGITAL TIME
// ============================================================
static void drawDigitalTime()
{
    char timeStr[6];
    char secondStr[3];

    int displayHour = hour;
    if (!use24HourFormat) { displayHour = hour % 12; if (displayHour == 0) displayHour = 12; }
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", displayHour, minute);

    snprintf(secondStr, sizeof(secondStr), "%02d",
             second);

    // Large digital time
    u8g2.setFont(u8g2_font_logisoso32_tf);

    int timeWidth = u8g2.getStrWidth(timeStr);
    int timeX = (128 - timeWidth) / 2 - 4;

    u8g2.drawStr(timeX, 44, timeStr);

    // Seconds display
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(108, 31, secondStr);

    // Seconds progress indicator
    u8g2.drawFrame(106, 35, 18, 4);

    uint8_t progress = (second * 16) / 60;

    if (progress > 16)
        progress = 16;

    if (progress > 0)
        u8g2.drawBox(107, 36, progress, 2);
}

// ============================================================
// BOTTOM WATCH STATUS BAR
// ============================================================
static void drawWatchFooter()
{
    char yearStr[5];

    snprintf(yearStr, sizeof(yearStr), "%04d", year);

    // Separator
    u8g2.drawHLine(4, 49, 120);

    u8g2.setFont(u8g2_font_5x8_tf);

    // Year
    u8g2.drawStr(5, 60, yearStr);

    // Animated center indicator
    u8g2.drawHLine(43, 56, 12);
    u8g2.drawHLine(73, 56, 12);

    uint8_t pulse = (watchFrame / 5) % 3;
    u8g2.drawDisc(64, 56, pulse == 1 ? 2 : 1);

    // 24-hour mode
    u8g2.drawStr(99, 60, use24HourFormat ? "24H" : (hour >= 12 ? "PM" : "AM"));

    // Tiny corner marks
    u8g2.drawPixel(122, 54);
    u8g2.drawPixel(122, 58);
}

// ============================================================
// MAIN WATCH UI
// ============================================================
void watchUI()
{
    u8g2.clearBuffer();

    drawWatchHeader();
    drawDigitalTime();
    drawWatchFooter();

    u8g2.sendBuffer();

    watchFrame++;
}

//! UI - 3

// extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// // ============================================================
// // MINIMAL TYPOGRAPHIC WATCH
// // ============================================================
// static void drawWatchInfo()
// {
//     char timeStr[6];
//     char secondStr[3];
//     char dateStr[16];
//     char yearStr[5];

//     snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
//              hour, minute);

//     snprintf(secondStr, sizeof(secondStr), "%02d",
//              second);

//     snprintf(dateStr, sizeof(dateStr), "%s  %02d %s",
//              weekdays[weekday], day, monthname[month - 1]);

//     snprintf(yearStr, sizeof(yearStr), "%04d", year);

//     // Main time — large and centered
//     u8g2.setFont(u8g2_font_logisoso32_tf);

//     int timeWidth = u8g2.getStrWidth(timeStr);
//     int timeX = (128 - timeWidth) / 2;

//     u8g2.drawStr(timeX, 39, timeStr);

//     // Seconds — tiny typography in the upper-right
//     u8g2.setFont(u8g2_font_5x8_tf);
//     u8g2.drawStr(109, 17, secondStr);

//     // Weekday and date — centered
//     u8g2.setFont(u8g2_font_6x10_tf);

//     int dateWidth = u8g2.getStrWidth(dateStr);
//     int dateX = (128 - dateWidth) / 2;

//     u8g2.drawStr(dateX, 53, dateStr);

//     // Year — subtle bottom text
//     u8g2.setFont(u8g2_font_4x6_tf);

//     int yearWidth = u8g2.getStrWidth(yearStr);
//     u8g2.drawStr((128 - yearWidth) / 2, 63, yearStr);
// }

// // ============================================================
// // MAIN UI FUNCTION
// // ============================================================
// void watchUI()
// {
//     u8g2.clearBuffer();

//     drawWatchInfo();

//     u8g2.sendBuffer();
// }