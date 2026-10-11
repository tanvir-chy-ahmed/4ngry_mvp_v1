#include <U8g2lib.h>
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "time.h"
#include "features/watch.h"
#include "features/weather.h"
#include "core/Global.h"

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

const long gmtOffset_sec = 6 * 3600;
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
    {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

const char *monthname[] =
    {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

static bool timeSynced = false;
static unsigned long lastClockUpdate = 0;
static const unsigned long CLOCK_UPDATE_INTERVAL = 200;
static unsigned long lastNtpSyncAttempt = 0;
static uint8_t watchFrame = 0;

bool isTimeSynced()
{
    return timeSynced;
}

// ============================================================
// INITIALIZE NTP
// ============================================================

void initLocalTime()
{
    configTime(
        gmtOffset_sec,
        daylightOffset_sec,
        "pool.ntp.org",
        "time.google.com",
        "time.nist.gov");

    Serial.println("NTP server configured.");

    // Quick non-blocking attempt (3 quick attempts)
    for (int i = 0; i < 3; i++)
    {
        if (getLocalTime(&timeinfo, 300))
        {
            if (timeinfo.tm_year >= (2024 - 1900))
            {
                timeSynced = true;
                Serial.println("Time synchronized at boot.");
                return;
            }
        }
        delay(100);
    }

    Serial.println("Initial NTP sync pending; will complete in background.");
}

// ============================================================
// UPDATE CLOCK VARIABLES
// ============================================================

void updateLocalTime()
{
    unsigned long now = millis();

    if (now - lastClockUpdate < CLOCK_UPDATE_INTERVAL)
        return;

    lastClockUpdate = now;

    // Background NTP resync trigger if WiFi is connected and not synced yet or every 3 hours
    if (Global::isWiFiConnected())
    {
        if (!timeSynced && (now - lastNtpSyncAttempt >= 10000))
        {
            lastNtpSyncAttempt = now;
            if (getLocalTime(&timeinfo, 100))
            {
                if (timeinfo.tm_year >= (2024 - 1900))
                {
                    timeSynced = true;
                    Serial.println("Time successfully synced via NTP in background.");
                }
            }
        }
        else if (timeSynced && (now - lastNtpSyncAttempt >= 3UL * 3600UL * 1000UL))
        {
            lastNtpSyncAttempt = now;
            getLocalTime(&timeinfo, 50);
        }
    }

    // Read current local time from internal RTC
    if (!getLocalTime(&timeinfo, 5))
    {
        return;
    }

    if (timeinfo.tm_year >= (2024 - 1900))
    {
        timeSynced = true;

        hour = timeinfo.tm_hour;
        minute = timeinfo.tm_min;
        second = timeinfo.tm_sec;

        day = timeinfo.tm_mday;
        month = timeinfo.tm_mon + 1;
        year = timeinfo.tm_year + 1900;
        weekday = timeinfo.tm_wday;
    }
}

// ============================================================
// DRAW MINI WIFI STATUS INDICATOR
// ============================================================
static void drawMiniWiFiIcon(int x, int y)
{
    if (Global::isWiFiConnected())
    {
        // 3 signal bars
        u8g2.drawVLine(x, y + 4, 2);
        u8g2.drawVLine(x + 3, y + 2, 4);
        u8g2.drawVLine(x + 6, y, 6);
    }
    else
    {
        // Disconnected WiFi symbol (dot with small cross/slash)
        u8g2.drawDisc(x + 2, y + 4, 1);
        u8g2.drawLine(x, y, x + 6, y + 6);
    }
}

// ============================================================
// TIME UNAVAILABLE UI
// ============================================================
static void drawTimeUnavailableUI()
{
    u8g2.setFont(u8g2_font_6x10_tf);

    // Top Header
    u8g2.drawStr(5, 10, "4NGRY CLOCK");
    drawMiniWiFiIcon(118, 4);
    u8g2.drawHLine(4, 14, 120);

    // Animated clock face with spinning hand
    int cx = 28;
    int cy = 38;
    u8g2.drawCircle(cx, cy, 14);
    u8g2.drawDisc(cx, cy, 2);

    float angle = (watchFrame % 12) * (3.14159f / 6.0f);
    int handX = cx + (int)(10 * sinf(angle));
    int handY = cy - (int)(10 * cosf(angle));
    u8g2.drawLine(cx, cy, handX, handY);

    // Status texts
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(50, 31, "TIME NOT SET");

    u8g2.setFont(u8g2_font_5x8_tf);
    if (Global::isWiFiConnected())
    {
        u8g2.drawStr(50, 43, "Syncing NTP...");
        u8g2.drawStr(50, 53, "Connecting...");
    }
    else
    {
        u8g2.drawStr(50, 43, "WiFi Offline");
        uint32_t retrySec = Global::getSecondsUntilRetry();
        if (retrySec > 0)
        {
            char rBuf[20];
            snprintf(rBuf, sizeof(rBuf), "Retry: %us", retrySec);
            u8g2.drawStr(50, 53, rBuf);
        }
        else
        {
            u8g2.drawStr(50, 53, "Awaiting WiFi");
        }
    }

    u8g2.drawHLine(4, 57, 120);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(26, 63, "Auto-syncs when online");
}

// ============================================================
// DIGITAL WATCH HEADER
// ============================================================
static void drawWatchHeader()
{
    char dateStr[14];

    snprintf(dateStr, sizeof(dateStr), "%02d %s",
             day, monthname[constrain(month - 1, 0, 11)]);

    u8g2.setFont(u8g2_font_5x8_tf);

    // Day label
    u8g2.drawStr(5, 10, weekdays[constrain(weekday, 0, 6)]);

    // Date aligned to center-right
    int dateWidth = u8g2.getStrWidth(dateStr);
    u8g2.drawStr(108 - dateWidth, 10, dateStr);

    // Mini WiFi icon on far right
    drawMiniWiFiIcon(118, 4);

    // Header separator
    u8g2.drawHLine(4, 14, 120);
}

// ============================================================
// LARGE DIGITAL TIME
// ============================================================
static void drawDigitalTime()
{
    char timeStr[6];
    char secondStr[3];

    snprintf(timeStr, sizeof(timeStr), "%02d:%02d",
             hour, minute);

    snprintf(secondStr, sizeof(secondStr), "%02d",
             second);

    // Large digital time
    u8g2.setFont(u8g2_font_logisoso32_tf);

    int timeWidth = u8g2.getStrWidth(timeStr);
    int timeX = (128 - timeWidth) / 2 - 5;

    u8g2.drawStr(timeX, 45, timeStr);

    // Seconds display
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(108, 30, secondStr);

    // Seconds progress indicator
    u8g2.drawFrame(106, 35, 18, 5);

    uint8_t progress = (second * 16) / 59;
    if (progress > 16)
        progress = 16;

    if (progress > 0)
        u8g2.drawBox(107, 36, progress, 3);
}

// ============================================================
// BOTTOM WATCH STATUS BAR
// ============================================================
static void drawWatchFooter()
{
    char yearStr[6];
    snprintf(yearStr, sizeof(yearStr), "%04d", year);

    // Separator
    u8g2.drawHLine(4, 49, 120);

    u8g2.setFont(u8g2_font_5x8_tf);

    // Year
    u8g2.drawStr(5, 60, yearStr);

    // Animated center indicator
    u8g2.drawHLine(43, 56, 12);
    u8g2.drawHLine(73, 56, 12);

    uint8_t pulse = (watchFrame / 4) % 3;
    u8g2.drawDisc(64, 56, pulse == 1 ? 2 : 1);

    // 24-hour mode
    u8g2.drawStr(99, 60, "24H");
}

// ============================================================
// MAIN WATCH UI
// ============================================================
void watchUI()
{
    u8g2.clearBuffer();

    if (!isTimeSynced())
    {
        drawTimeUnavailableUI();
    }
    else
    {
        drawWatchHeader();
        drawDigitalTime();
        drawWatchFooter();
    }

    u8g2.sendBuffer();
    watchFrame++;
}