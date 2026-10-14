#include <Arduino.h>
#include "features/watch.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>
#include <math.h>
#include "core/Global.h"
#include <ArduinoJson.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// ================= WEATHER CONFIGURATION =================

static const String openwetherapikey = "554eb7387cc38ab998746cb3522e1cd3";
static const String countryCode = "BD";
static const String city = "Sylhet";
static const String units = "metric";
static const String language = "en";

static const String serverPath =
    "http://api.openweathermap.org/data/2.5/weather?q=" +
    city + "," + countryCode +
    "&APPID=" + openwetherapikey +
    "&units=" + units +
    "&lang=" + language;

// ================= WEATHER DATA =================

static String weatherCity = city;
static String weatherCondition = "--";

static float weatherTemp = 0.0;
static float weatherTempMax = 0.0;
static float weatherTempMin = 0.0;

static bool weatherAvailable = false;
static uint8_t weatherFrame = 0;

static unsigned long lastWeatherFetch = 0;
static const unsigned long WEATHER_REFRESH_INTERVAL = 20UL * 60UL * 1000UL; // 20 minutes
static const unsigned long WEATHER_RETRY_INTERVAL = 30UL * 1000UL;          // 30 seconds if not yet fetched

bool isWeatherAvailable()
{
    return weatherAvailable;
}

// ================= HTTP REQUEST =================

String httpGETRequest(const char *serverName)
{
    if (!Global::isWiFiConnected())
    {
        return "";
    }

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(5000);

    if (!http.begin(client, serverName))
    {
        Serial.println("Weather: HTTP begin failed");
        return "";
    }

    int httpResponseCode = http.GET();
    String payload = "";

    if (httpResponseCode == HTTP_CODE_OK)
    {
        payload = http.getString();
    }
    else
    {
        Serial.printf("Weather HTTP code: %d\n", httpResponseCode);
    }

    http.end();
    return payload;
}

// ================= FETCH WEATHER =================

void getWeatherInfo()
{
    if (!Global::isWiFiConnected())
    {
        Serial.println("Weather fetch skipped: WiFi offline.");
        return;
    }

    String response = httpGETRequest(serverPath.c_str());

    if (response.length() == 0)
    {
        Serial.println("Weather: Empty or failed response");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error)
    {
        Serial.printf("Weather JSON parse failed: %s\n", error.c_str());
        return;
    }

    if (!doc["main"]["temp"].is<float>() || !doc["weather"][0]["main"].is<const char *>())
    {
        Serial.println("Weather: Missing expected fields in response.");
        return;
    }

    float temp = doc["main"]["temp"].as<float>();
    float tempMax = doc["main"]["temp_max"].as<float>();
    float tempMin = doc["main"]["temp_min"].as<float>();

    if (!isfinite(temp) || temp < -60.0f || temp > 65.0f)
    {
        Serial.println("Weather: Unreasonable temperature reading rejected.");
        return;
    }

    weatherTemp = temp;
    weatherTempMax = tempMax;
    weatherTempMin = tempMin;

    if (doc["name"].is<const char *>())
    {
        weatherCity = doc["name"].as<String>();
    }
    weatherCondition = doc["weather"][0]["main"].as<String>();

    weatherAvailable = true;

    Serial.println("Weather updated successfully:");
    Serial.printf("City: %s | Temp: %.1f C (H: %.1f, L: %.1f) | Condition: %s\n",
                  weatherCity.c_str(), weatherTemp, weatherTempMax, weatherTempMin, weatherCondition.c_str());
}

// ================= PERIODIC WEATHER FETCHER =================

void updateWeatherPeriodically()
{
    if (!Global::isWiFiConnected())
        return;

    unsigned long now = millis();
    unsigned long interval = weatherAvailable ? WEATHER_REFRESH_INTERVAL : WEATHER_RETRY_INTERVAL;

    if (now - lastWeatherFetch >= interval || lastWeatherFetch == 0)
    {
        lastWeatherFetch = now;
        getWeatherInfo();
    }
}

bool fetchWeatherWithRetry()
{
    for (uint8_t attempt = 1; attempt <= 2; attempt++)
    {
        getWeatherInfo();
        if (weatherAvailable)
            return true;
        delay(1000);
    }
    return false;
}

// ============================================================
// WEATHER ICONS
// ============================================================

static void drawSunIcon(int x, int y)
{
    u8g2.drawCircle(x, y, 6);
    u8g2.drawLine(x, y - 10, x, y - 8);
    u8g2.drawLine(x, y + 8, x, y + 10);
    u8g2.drawLine(x - 10, y, x - 8, y);
    u8g2.drawLine(x + 8, y, x + 10, y);

    u8g2.drawLine(x - 7, y - 7, x - 5, y - 5);
    u8g2.drawLine(x + 5, y - 5, x + 7, y - 7);
    u8g2.drawLine(x - 7, y + 7, x - 5, y + 5);
    u8g2.drawLine(x + 5, y + 5, x + 7, y + 7);
}

static void drawCloudIcon(int x, int y)
{
    u8g2.drawDisc(x - 4, y + 1, 5);
    u8g2.drawDisc(x + 3, y - 1, 6);
    u8g2.drawDisc(x + 9, y + 2, 4);
    u8g2.drawBox(x - 8, y + 2, 20, 5);
}

static void drawRainIcon(int x, int y)
{
    drawCloudIcon(x, y - 3);

    // Falling raindrops
    int animOffset = (weatherFrame / 2) % 3;
    u8g2.drawLine(x - 5, y + 6 + animOffset, x - 7, y + 9 + animOffset);
    u8g2.drawLine(x + 1, y + 6 + animOffset, x - 1, y + 9 + animOffset);
    u8g2.drawLine(x + 7, y + 6 + animOffset, x + 5, y + 9 + animOffset);
}

static void drawThunderIcon(int x, int y)
{
    drawCloudIcon(x, y - 3);

    // Lightning bolt
    u8g2.drawLine(x + 2, y + 5, x - 1, y + 9);
    u8g2.drawLine(x - 1, y + 9, x + 3, y + 9);
    u8g2.drawLine(x + 3, y + 9, x, y + 14);
}

static void drawSnowIcon(int x, int y)
{
    drawCloudIcon(x, y - 3);

    // Snowflakes
    u8g2.drawPixel(x - 5, y + 7);
    u8g2.drawPixel(x + 1, y + 8);
    u8g2.drawPixel(x + 7, y + 7);
    u8g2.drawPixel(x - 2, y + 11);
    u8g2.drawPixel(x + 4, y + 11);
}

static void drawMistIcon(int x, int y)
{
    int anim = (weatherFrame / 3) % 4;
    u8g2.drawHLine(x - 10 + anim, y - 4, 20);
    u8g2.drawHLine(x - 8 - anim, y, 18);
    u8g2.drawHLine(x - 11 + anim, y + 4, 22);
}

static void drawDynamicWeatherIcon(int x, int y)
{
    if (weatherCondition.equalsIgnoreCase("Clear"))
    {
        drawSunIcon(x, y);
    }
    else if (weatherCondition.equalsIgnoreCase("Clouds"))
    {
        drawCloudIcon(x, y);
    }
    else if (weatherCondition.equalsIgnoreCase("Rain") || weatherCondition.equalsIgnoreCase("Drizzle"))
    {
        drawRainIcon(x, y);
    }
    else if (weatherCondition.equalsIgnoreCase("Thunderstorm"))
    {
        drawThunderIcon(x, y);
    }
    else if (weatherCondition.equalsIgnoreCase("Snow"))
    {
        drawSnowIcon(x, y);
    }
    else if (weatherCondition.equalsIgnoreCase("Mist") || weatherCondition.equalsIgnoreCase("Fog") ||
             weatherCondition.equalsIgnoreCase("Haze") || weatherCondition.equalsIgnoreCase("Smoke"))
    {
        drawMistIcon(x, y);
    }
    else
    {
        drawSunIcon(x, y);
    }
}

// ============================================================
// WEATHER UNAVAILABLE UI
// ============================================================

static void drawWeatherUnavailableUI()
{
    u8g2.setFont(u8g2_font_5x8_tf);

    // Header
    u8g2.drawStr(5, 10, ("WEATHER - " + city).c_str());

    // Mini WiFi status
    if (Global::isWiFiConnected())
    {
        u8g2.drawVLine(118, 8, 2);
        u8g2.drawVLine(121, 6, 4);
        u8g2.drawVLine(124, 4, 6);
    }
    else
    {
        u8g2.drawDisc(120, 8, 1);
        u8g2.drawLine(118, 4, 124, 10);
    }

    u8g2.drawHLine(4, 14, 120);

    // Center cloud with question mark
    drawCloudIcon(24, 34);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(22, 34, "?");

    // Status box
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(48, 30, "NO WEATHER");

    u8g2.setFont(u8g2_font_5x8_tf);
    if (Global::isWiFiConnected())
    {
        u8g2.drawStr(48, 42, "Fetching data...");
        u8g2.drawStr(48, 52, "Contacting API...");
    }
    else
    {
        u8g2.drawStr(48, 42, "WiFi Offline");
        uint32_t retrySec = Global::getSecondsUntilRetry();
        if (retrySec > 0)
        {
            char rBuf[20];
            snprintf(rBuf, sizeof(rBuf), "Retry: %us", retrySec);
            u8g2.drawStr(48, 52, rBuf);
        }
        else
        {
            u8g2.drawStr(48, 52, "Waiting WiFi");
        }
    }

    u8g2.drawHLine(4, 57, 120);
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(26, 63, "Auto-updates when online");
}

// ============================================================
// WEATHER AVAILABLE UI
// ============================================================

static void drawWeatherAvailableUI()
{
    // Background drift particles
    int offset = (weatherFrame / 2) % 32;
    u8g2.drawHLine(4 + offset, 17, 12);
    u8g2.drawHLine(92 - offset, 47, 14);

    // Header: City and Condition
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(5, 10, weatherCity.c_str());

    int condW = u8g2.getStrWidth(weatherCondition.c_str());
    u8g2.drawStr(114 - condW, 10, weatherCondition.c_str());

    // Mini WiFi icon
    if (Global::isWiFiConnected())
    {
        u8g2.drawVLine(118, 8, 2);
        u8g2.drawVLine(121, 6, 4);
        u8g2.drawVLine(124, 4, 6);
    }

    u8g2.drawHLine(4, 14, 120);

    // Weather Icon on left
    drawDynamicWeatherIcon(22, 33);

    // Temperature in center
    u8g2.setFont(u8g2_font_logisoso28_tf);
    char tempText[12];
    snprintf(tempText, sizeof(tempText), "%.0f", weatherTemp);
    u8g2.drawStr(48, 43, tempText);

    // Degree symbol & Celsius unit
    int textW = u8g2.getStrWidth(tempText);
    int degX = 50 + textW;
    u8g2.drawCircle(degX + 3, 21, 2);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(degX + 8, 38, "C");

    // Separator line above footer
    u8g2.drawHLine(4, 50, 120);

    // Footer: High & Low
    u8g2.setFont(u8g2_font_5x8_tf);
    char maxText[16];
    snprintf(maxText, sizeof(maxText), "H:%.0f C", weatherTempMax);
    u8g2.drawStr(5, 60, maxText);

    char minText[16];
    snprintf(minText, sizeof(minText), "L:%.0f C", weatherTempMin);
    int minW = u8g2.getStrWidth(minText);
    u8g2.drawStr(123 - minW, 60, minText);

    // Center pulse dot
    uint8_t pulse = (weatherFrame / 4) % 3;
    u8g2.drawDisc(64, 57, pulse == 1 ? 2 : 1);
}

// ============================================================
// MAIN WEATHER UI
// ============================================================

void weatherUI()
{
    u8g2.clearBuffer();

    if (!isWeatherAvailable())
    {
        drawWeatherUnavailableUI();
    }
    else
    {
        drawWeatherAvailableUI();
    }

    u8g2.sendBuffer();
    weatherFrame++;
}
