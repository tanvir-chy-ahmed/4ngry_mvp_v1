#include <Arduino.h>
#include "features/watch.h"
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>
#include <math.h>
#include "core/Global.h"
#include <Arduino_JSON.h>

extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// ================= WEATHER CONFIGURATION =================

String openwetherapikey = "554eb7387cc38ab998746cb3522e1cd3";
String countryCode = "BD";
String latitude = "24.857";
String longitude = "92.014";
String units = "metric";
String city = "Sylhet";
String language = "en";

String serverPath =
    "http://api.openweathermap.org/data/2.5/weather?q=" +
    city + "," + countryCode +
    "&APPID=" + openwetherapikey +
    "&units=" + units +
    "&lang=" + language;

String jsonBuffer;

// ================= WEATHER DATA =================

String weatherCity = "--";
String weatherCondition = "--";

float weatherTemp = 0.0;
float weatherTempMax = 0.0;
float weatherTempMin = 0.0;

bool weatherAvailable = false;

static uint8_t weatherFrame = 0;

// ================= HTTP REQUEST =================

String httpGETRequest(const char *serverName)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Weather: WiFi disconnected");
        return "";
    }

    WiFiClient client;
    HTTPClient http;

    http.setTimeout(8000);

    if (!http.begin(client, serverName))
    {
        Serial.println("Weather: HTTP begin failed");
        return "";
    }

    int httpResponseCode = http.GET();

    Serial.printf("Weather HTTP code: %d\n", httpResponseCode);

    String payload = "";

    if (httpResponseCode == HTTP_CODE_OK)
    {
        payload = http.getString();
        Serial.println("Weather API response:");
        Serial.println(payload);
    }
    else
    {
        if (httpResponseCode > 0)
        {
            Serial.println(http.getString());
        }
        else
        {
            Serial.println(http.errorToString(httpResponseCode));
        }
    }

    http.end();
    return payload;
}

// ================= FETCH WEATHER =================

void getWeatherInfo()
{
    // Do not erase valid previous weather on failure.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Weather: WiFi disconnected");
        return;
    }

    jsonBuffer = httpGETRequest(serverPath.c_str());

    if (jsonBuffer.length() == 0)
    {
        Serial.println("Weather: Empty response");
        return;
    }

    JSONVar data = JSON.parse(jsonBuffer);

    if (JSON.typeof(data) == "undefined")
    {
        Serial.println("Weather: JSON parsing failed");
        return;
    }

    if (JSON.typeof(data["main"]) == "undefined" ||
        JSON.typeof(data["main"]["temp"]) == "undefined" ||
        JSON.typeof(data["main"]["temp_max"]) == "undefined" ||
        JSON.typeof(data["main"]["temp_min"]) == "undefined" ||
        JSON.typeof(data["weather"]) == "undefined" ||
        JSON.typeof(data["weather"][0]["main"]) == "undefined")
    {
        Serial.println("Weather: Missing weather fields");
        Serial.println(jsonBuffer);
        return;
    }

    double temp = (double)data["main"]["temp"];
    double tempMax = (double)data["main"]["temp_max"];
    double tempMin = (double)data["main"]["temp_min"];

    if (!isfinite(temp) ||
        !isfinite(tempMax) ||
        !isfinite(tempMin) ||
        temp < -100 || temp > 70 ||
        tempMax < -100 || tempMax > 70 ||
        tempMin < -100 || tempMin > 70 ||
        tempMax < tempMin)
    {
        Serial.println("Weather: Invalid temperature values");
        return;
    }

    // Commit new values only after validation.
    weatherTemp = temp;
    weatherTempMax = tempMax;
    weatherTempMin = tempMin;

    weatherCity = (const char *)data["name"];
    weatherCondition =
        (const char *)data["weather"][0]["main"];

    weatherAvailable = true;

    Serial.println("Weather updated successfully");
    Serial.printf("Temperature: %.1f C\n", weatherTemp);
    Serial.printf("Maximum: %.1f C\n", weatherTempMax);
    Serial.printf("Minimum: %.1f C\n", weatherTempMin);
    Serial.printf("City: %s\n", weatherCity.c_str());
    Serial.printf("Condition: %s\n", weatherCondition.c_str());
}

// ================= RETRY FETCH =================

// Blocking version: use during initial testing.
// Each HTTP attempt can take up to 8 seconds.
bool fetchWeatherWithRetry()
{
    const uint8_t maxAttempts = 5;
    const unsigned long retryDelay = 2000;

    for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++)
    {
        Serial.printf(
            "Weather attempt %u/%u\n",
            attempt,
            maxAttempts);

        // A previous success is not proof of a new successful fetch.
        // Use a separate freshness flag if retrying after prior success.
        getWeatherInfo();

        // For startup, weatherAvailable indicates that valid data
        // has been fetched at least once.
        if (weatherAvailable)
        {
            Serial.println("Weather data available");
            return true;
        }

        if (attempt < maxAttempts)
        {
            Serial.println("Retrying weather in 2 seconds...");
            delay(retryDelay);
        }
    }

    Serial.println("Weather fetch failed after 5 attempts");
    return false;
}

// ============================================================
// WEATHER ICON
// ============================================================

static void drawWeatherIcon(int x, int y)
{
    u8g2.drawCircle(x, y, 7);

    u8g2.drawLine(x, y - 11, x, y - 9);
    u8g2.drawLine(x, y + 9, x, y + 11);

    u8g2.drawLine(x - 11, y, x - 9, y);
    u8g2.drawLine(x + 9, y, x + 11, y);

    u8g2.drawLine(x - 8, y - 8, x - 6, y - 6);
    u8g2.drawLine(x + 6, y - 6, x + 8, y - 8);

    u8g2.drawLine(x - 8, y + 8, x - 6, y + 6);
    u8g2.drawLine(x + 6, y + 6, x + 8, y + 8);
}

// ============================================================
// WEATHER BACKGROUND
// ============================================================

static void drawWeatherBackground()
{
    int offset = (weatherFrame / 2) % 32;

    u8g2.drawHLine(4 + offset, 17, 12);
    u8g2.drawHLine(92 - offset, 47, 14);

    for (int i = 0; i < 6; i++)
    {
        int x = (i * 27 + weatherFrame / 4) % 128;
        int y = 14 + ((i * 9) % 38);

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
    u8g2.setFont(u8g2_font_logisoso28_tf);

    char tempText[16];

    if (weatherAvailable)
    {
        snprintf(tempText, sizeof(tempText), "%.0f", weatherTemp);
    }
    else
    {
        snprintf(tempText, sizeof(tempText), "--");
    }

    u8g2.drawStr(43, 42, tempText);

    if (weatherAvailable)
    {
        u8g2.drawCircle(81, 20, 2);

        u8g2.setFont(u8g2_font_7x14B_tf);
        u8g2.drawStr(86, 40, "C");
    }
}

// ============================================================
// WEATHER INFORMATION
// ============================================================

static void drawWeatherInfo()
{
    u8g2.setFont(u8g2_font_5x8_tf);

    u8g2.drawStr(5, 9, city.c_str());

    if (weatherAvailable)
    {
        u8g2.drawStr(91, 9, weatherCondition.c_str());

        char maxText[15];
        snprintf(maxText, sizeof(maxText), "H:%.0f", weatherTempMax);
        u8g2.drawStr(5, 61, maxText);

        char minText[15];
        snprintf(minText, sizeof(minText), "L:%.0f", weatherTempMin);
        u8g2.drawStr(102, 61, minText);
    }
    else
    {
        u8g2.drawStr(91, 9, "Loading");
        u8g2.drawStr(5, 61, "H:--");
        u8g2.drawStr(102, 61, "L:--");
    }
}

// ============================================================
// WEATHER ACCENTS
// ============================================================

static void drawWeatherAccents()
{
    u8g2.drawVLine(3, 23, 19);
    u8g2.drawVLine(124, 23, 19);
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
    drawWeatherIcon(22, 31);
    drawTemperature();
    drawWeatherAccents();

    u8g2.sendBuffer();

    weatherFrame++;
}

// #include <Arduino.h>
// #include "features/watch.h"
// #include <U8g2lib.h>
// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <string.h>
// #include "core/Global.h"
// #include <Arduino_JSON.h>

// extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// // https://api.openweathermap.org/data/2.5/weather?id=1185099&appid=554eb7387cc38ab998746cb3522e1cd3&units=metric

// String openwetherapikey = "554eb7387cc38ab998746cb3522e1cd3";
// String countryCode = "BD";
// String latitude = "24.857";
// String longitude = "92.014";
// String units = "metric";
// String city = "Sylhet";
// String language = "en";
// String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openwetherapikey + "&units=metric";
// // String url = "https://api.openweathermap.org/data/2.5/weather?id=1185099&appid=554eb7387cc38ab998746cb3522e1cd3&units=metric";
// String jsonBuffer;

// // ================= WEATHER DATA =================
// String weatherCity = "--";
// String weatherCondition = "--";

// float weatherTemp = 0.0;
// float weatherTempMax = 0.0;
// float weatherTempMin = 0.0;

// bool weatherAvailable = false;

// // String httpGETRequest(const char *serverName)
// // {
// //     WiFiClient client;
// //     HTTPClient http;

// //     // Your Domain name with URL path or IP address with path
// //     http.begin(client, serverName);

// //     // Send HTTP POST request
// //     int httpResponseCode = http.GET();

// //     String payload = "{}";

// //     if (httpResponseCode > 0)
// //     {
// //         Serial.print("HTTP Response code: ");
// //         Serial.println(httpResponseCode);
// //         payload = http.getString();
// //     }
// //     else
// //     {
// //         Serial.print("Error code: ");
// //         Serial.println(httpResponseCode);
// //     }
// //     // Free resources
// //     http.end();

// //     return payload;
// // }
// // void getWeatherInfo()
// // {

// //         if (WiFi.status() == WL_CONNECTED)
// //         {
// //             jsonBuffer = httpGETRequest(serverPath.c_str());

// //             JSONVar myObject = JSON.parse(jsonBuffer);

// //             if (JSON.typeof(myObject) == "undefined")
// //             {
// //                 Serial.println("Parsing input failed!");
// //                 return;
// //             }

// //             // Store weather data
// //             weatherTemp =
// //                 (double)myObject["main"]["temp"];

// //             weatherTempMax =
// //                 (double)myObject["main"]["temp_max"];

// //             weatherTempMin =
// //                 (double)myObject["main"]["temp_min"];

// //             weatherCity =
// //                 (const char *)myObject["name"];

// //             weatherCondition =
// //                 (const char *)myObject["weather"][0]["main"];

// //             weatherAvailable = true;

// //             Serial.println("Weather updated");

// //             Serial.print("Temperature: ");
// //             Serial.println(myObject["main"]["temp"]);

// //             Serial.print("High Temperature: ");
// //             Serial.println(myObject["main"]["temp_max"]);

// //             Serial.print("Low Temperature: ");
// //             Serial.println(myObject["main"]["temp_min"]);

// //             Serial.print("City: ");
// //             Serial.println(myObject["name"]);

// //             Serial.print("Condition: ");
// //             Serial.println(myObject["weather"][0]["main"]);
// //         }
// //         else
// //         {
// //             Serial.println("WiFi Disconnected");
// //         }

// // }

// String httpGETRequest(const char *serverName)
// {
//     if (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.println("Weather: WiFi disconnected");
//         return "";
//     }

//     WiFiClient client;
//     HTTPClient http;

//     http.setTimeout(8000);

//     if (!http.begin(client, serverName))
//     {
//         Serial.println("Weather: HTTP begin failed");
//         return "";
//     }

//     int httpResponseCode = http.GET();

//     Serial.printf("Weather HTTP code: %d\n", httpResponseCode);

//     String payload = "";

//     if (httpResponseCode == HTTP_CODE_OK)
//     {
//         payload = http.getString();
//         Serial.println("Weather API response:");
//         Serial.println(payload);
//     }
//     else
//     {
//         // Read the error response for diagnosis.
//         if (httpResponseCode > 0)
//         {
//             Serial.println(http.getString());
//         }
//         else
//         {
//             Serial.println(http.errorToString(httpResponseCode));
//         }
//     }

//     http.end();
//     return payload;
// }

// void getWeatherInfo()
// {
//     if (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.println("Weather: WiFi disconnected");
//         return;
//     }

//     String response = httpGETRequest(serverPath.c_str());

//     if (response.length() == 0)
//     {
//         Serial.println("Weather: Empty response");
//         return;
//     }

//     JSONVar data = JSON.parse(response);

//     if (JSON.typeof(data) == "undefined")
//     {
//         Serial.println("Weather: JSON parsing failed");
//         return;
//     }

//     // Check for valid weather data before accessing its fields.
//     if (JSON.typeof(data["main"]) == "undefined" ||
//         JSON.typeof(data["main"]["temp"]) == "undefined" ||
//         JSON.typeof(data["main"]["temp_max"]) == "undefined" ||
//         JSON.typeof(data["main"]["temp_min"]) == "undefined" ||
//         JSON.typeof(data["weather"]) == "undefined")
//     {
//         Serial.println("Weather: API response has no valid weather data");
//         Serial.println(response);
//         return;
//     }

//     double temp = (double)data["main"]["temp"];
//     double tempMax = (double)data["main"]["temp_max"];
//     double tempMin = (double)data["main"]["temp_min"];

//     // Reject impossible/unusable readings without overwriting old data.
//     if (!isfinite(temp) || !isfinite(tempMax) || !isfinite(tempMin) ||
//         temp < -100 || temp > 70 ||
//         tempMax < -100 || tempMax > 70 ||
//         tempMin < -100 || tempMin > 70 ||
//         tempMax < tempMin ||
//         JSON.typeof(data["weather"][0]["main"]) == "undefined")
//     {
//         Serial.println("Weather: Invalid temperature or condition");
//         return;
//     }

//     // Update existing global variables only after validation.
//     weatherTemp = temp;
//     weatherTempMax = tempMax;
//     weatherTempMin = tempMin;
//     weatherCity = (const char *)data["name"];
//     weatherCondition = (const char *)data["weather"][0]["main"];

//     weatherAvailable = true;

//     Serial.println("Weather updated successfully");
//     Serial.printf("Temperature: %.1f C\n", weatherTemp);
//     Serial.printf("Maximum: %.1f C\n", weatherTempMax);
//     Serial.printf("Minimum: %.1f C\n", weatherTempMin);
//     Serial.printf("City: %s\n", weatherCity.c_str());
//     Serial.printf("Condition: %s\n", weatherCondition.c_str());
// }
// static uint8_t weatherFrame = 0;

// // ============================================================
// // WEATHER ICON
// // ============================================================

// static void drawWeatherIcon(int x, int y)
// {
//     // Sun body
//     u8g2.drawCircle(x, y, 7);

//     // Sun rays - 8 evenly spaced rays
//     u8g2.drawLine(x, y - 11, x, y - 9); // top
//     u8g2.drawLine(x, y + 9, x, y + 11); // bottom

//     u8g2.drawLine(x - 11, y, x - 9, y); // left
//     u8g2.drawLine(x + 9, y, x + 11, y); // right

//     u8g2.drawLine(x - 8, y - 8, x - 6, y - 6); // top-left
//     u8g2.drawLine(x + 6, y - 6, x + 8, y - 8); // top-right

//     u8g2.drawLine(x - 8, y + 8, x - 6, y + 6); // bottom-left
//     u8g2.drawLine(x + 6, y + 6, x + 8, y + 8); // bottom-right
// }
// // ============================================================
// // WEATHER BACKGROUND
// // ============================================================

// static void drawWeatherBackground()
// {
//     // Slow horizontal atmospheric lines
//     int offset = (weatherFrame / 2) % 32;

//     u8g2.drawHLine(4 + offset, 17, 12);
//     u8g2.drawHLine(92 - offset, 47, 14);

//     // Tiny drifting particles
//     for (int i = 0; i < 6; i++)
//     {
//         int x = (i * 27 + weatherFrame / 4) % 128;
//         int y = 14 + ((i * 9) % 38);

//         // Don't disturb main temperature
//         if (x > 43 && x < 91 && y > 23 && y < 43)
//             continue;

//         u8g2.drawPixel(x, y);
//     }
// }

// // ============================================================
// // TEMPERATURE
// // ============================================================

// static void drawTemperature()
// {
//     // Main temperature
//     u8g2.setFont(u8g2_font_logisoso28_tf);
//     char tempText[16];

//     snprintf(
//         tempText,
//         sizeof(tempText),
//         "%.0f",
//         weatherTemp);
//     u8g2.drawStr(43, 42, tempText);

//     // Degree symbol
//     u8g2.drawCircle(81, 20, 2);

//     // Celsius
//     u8g2.setFont(u8g2_font_7x14B_tf);
//     u8g2.drawStr(86, 40, "C");
// }

// // ============================================================
// // WEATHER INFORMATION
// // ============================================================

// static void drawWeatherInfo()
// {
//     u8g2.setFont(u8g2_font_5x8_tf);

//     // Location
//     u8g2.drawStr(5, 9, city.c_str());

//     // Condition
//     u8g2.drawStr(91, 9, weatherCondition.c_str());
//     char maxText[15];

//     snprintf(
//         maxText,
//         sizeof(maxText),
//         "L:%.0f",
//         weatherTempMax);
//     // Bottom information
//     u8g2.drawStr(5, 61, maxText);

//     char minText[15];

//     snprintf(
//         minText,
//         sizeof(minText),
//         "L:%.0f",
//         weatherTempMin);

//     u8g2.drawStr(102, 61, minText);
// }

// // ============================================================
// // WEATHER ACCENTS
// // ============================================================

// static void drawWeatherAccents()
// {
//     // Small left vertical marker
//     u8g2.drawVLine(3, 23, 19);

//     // Small right vertical marker
//     u8g2.drawVLine(124, 23, 19);

//     // Bottom center indicator
//     u8g2.drawDisc(64, 52, 1);
// }

// // ============================================================
// // MAIN WEATHER UI
// // ============================================================

// void weatherUI()
// {
//     u8g2.clearBuffer();

//     drawWeatherBackground();

//     drawWeatherInfo();

//     // Weather icon slightly behind / beside temperature
//     drawWeatherIcon(22, 31);

//     drawTemperature();

//     drawWeatherAccents();

//     u8g2.sendBuffer();

//     weatherFrame++;
// }
