#include <Arduino.h>
#include "features/watch.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>
#include "core/global.h"
#include <Arduino_JSON.h>

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// https://api.openweathermap.org/data/2.5/weather?id=1185099&appid=554eb7387cc38ab998746cb3522e1cd3&units=metric


String openwetherapikey = "554eb7387cc38ab998746cb3522e1cd3";
String countryCode = "BD";
String latitude = "24.857";
String longitude = "92.014";
String units = "metric";
String city = "Sylhet";
String language = "en";
String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openwetherapikey + "&units=metric";
// String url = "https://api.openweathermap.org/data/2.5/weather?id=1185099&appid=554eb7387cc38ab998746cb3522e1cd3&units=metric";
String jsonBuffer;

// ================= WEATHER DATA =================
String weatherCity = "--";
String weatherCondition = "--";

float weatherTemp = 0.0;
float weatherTempMax = 0.0;
float weatherTempMin = 0.0;

bool weatherAvailable = false;

String httpGETRequest(const char *serverName)
{
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);

    // Send HTTP POST request
    int httpResponseCode = http.GET();

    String payload = "{}";

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();

    return payload;
}
void getWeatherInfo()
{
  
        if (WiFi.status() == WL_CONNECTED)
        {
            jsonBuffer = httpGETRequest(serverPath.c_str());

            JSONVar myObject = JSON.parse(jsonBuffer);

            if (JSON.typeof(myObject) == "undefined")
            {
                Serial.println("Parsing input failed!");
                return;
            }

            // Store weather data
            weatherTemp =
                (double)myObject["main"]["temp"];

            weatherTempMax =
                (double)myObject["main"]["temp_max"];

            weatherTempMin =
                (double)myObject["main"]["temp_min"];

            weatherCity =
                (const char *)myObject["name"];

            weatherCondition =
                (const char *)myObject["weather"][0]["main"];

            weatherAvailable = true;

            Serial.println("Weather updated");

            Serial.print("Temperature: ");
            Serial.println(myObject["main"]["temp"]);

            Serial.print("High Temperature: ");
            Serial.println(myObject["main"]["temp_max"]);

            Serial.print("Low Temperature: ");
            Serial.println(myObject["main"]["temp_min"]);

            Serial.print("City: ");
            Serial.println(myObject["name"]);

            Serial.print("Condition: ");
            Serial.println(myObject["weather"][0]["main"]);
        }
        else
        {
            Serial.println("WiFi Disconnected");
        }

    
}

static uint8_t weatherFrame = 0;

// ============================================================
// WEATHER ICON
// ============================================================

static void drawWeatherIcon(int x, int y)
{
    // Sun body
    u8g2.drawCircle(x, y, 7);

    // Sun rays - 8 evenly spaced rays
    u8g2.drawLine(x, y - 11, x, y - 9); // top
    u8g2.drawLine(x, y + 9, x, y + 11); // bottom

    u8g2.drawLine(x - 11, y, x - 9, y); // left
    u8g2.drawLine(x + 9, y, x + 11, y); // right

    u8g2.drawLine(x - 8, y - 8, x - 6, y - 6); // top-left
    u8g2.drawLine(x + 6, y - 6, x + 8, y - 8); // top-right

    u8g2.drawLine(x - 8, y + 8, x - 6, y + 6); // bottom-left
    u8g2.drawLine(x + 6, y + 6, x + 8, y + 8); // bottom-right
}
// ============================================================
// WEATHER BACKGROUND
// ============================================================

static void drawWeatherBackground()
{
    // Slow horizontal atmospheric lines
    int offset = (weatherFrame / 2) % 32;

    u8g2.drawHLine(4 + offset, 17, 12);
    u8g2.drawHLine(92 - offset, 47, 14);

    // Tiny drifting particles
    for (int i = 0; i < 6; i++)
    {
        int x = (i * 27 + weatherFrame / 4) % 128;
        int y = 14 + ((i * 9) % 38);

        // Don't disturb main temperature
        if (x > 43 && x < 91 && y > 23 && y < 43)
            continue;

        u8g2.drawPixel(x, y);
    }
}

// ============================================================
// TEMPERATURE
// ============================================================

static void drawTemperature()
{
    // Main temperature
    u8g2.setFont(u8g2_font_logisoso28_tf);
    char tempText[16];

    snprintf(
        tempText,
        sizeof(tempText),
        "%.0f",
        weatherTemp);
    u8g2.drawStr(43, 42, tempText);

    // Degree symbol
    u8g2.drawCircle(81, 20, 2);

    // Celsius
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(86, 40, "C");
}

// ============================================================
// WEATHER INFORMATION
// ============================================================

static void drawWeatherInfo()
{
    u8g2.setFont(u8g2_font_5x8_tf);

    // Location
    u8g2.drawStr(5, 9, city.c_str());

    // Condition
    u8g2.drawStr(91, 9, weatherCondition.c_str());
    char maxText[15];

    snprintf(
        maxText,
        sizeof(maxText),
        "L:%.0f",
        weatherTempMax);
    // Bottom information
    u8g2.drawStr(5, 61, maxText);

    char minText[15];

    snprintf(
        minText,
        sizeof(minText),
        "L:%.0f",
        weatherTempMin);

    u8g2.drawStr(102, 61, minText);
}

// ============================================================
// WEATHER ACCENTS
// ============================================================

static void drawWeatherAccents()
{
    // Small left vertical marker
    u8g2.drawVLine(3, 23, 19);

    // Small right vertical marker
    u8g2.drawVLine(124, 23, 19);

    // Bottom center indicator
    u8g2.drawDisc(64, 52, 1);
}

// ============================================================
// MAIN WEATHER UI
// ============================================================

void weatherUI()
{
    u8g2.clearBuffer();

    drawWeatherBackground();

    drawWeatherInfo();

    // Weather icon slightly behind / beside temperature
    drawWeatherIcon(22, 31);

    drawTemperature();

    drawWeatherAccents();

    u8g2.sendBuffer();

    weatherFrame++;
}
