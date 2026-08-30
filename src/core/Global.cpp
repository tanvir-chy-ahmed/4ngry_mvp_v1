#include <WiFi.h>
#include <Arduino.h>
#include <features/watch.h>
#include <features/weather.h>
const char *ssid = "Tanvir Ahmed Chy";
const char *pass = "12345678HH";

void WiFiOff()
{
    // No loonger needed the wifi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

// void WiFiOn()
// {
//     WiFi.begin(ssid, pass);
//     while (WiFi.status() != WL_CONNECTED)
//     {
//         Serial.print(".");
//     }
//     Serial.println("\nWiFi Connected");

// }
bool WiFiOn()
{
    Serial.println();
    Serial.println("========== WiFiOn() CALLED ==========");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    Serial.print("Connecting to WiFi");

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < 15000)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi Connected");
        return true;
    }

    Serial.print("WiFi Failed. Status: ");
    Serial.println(WiFi.status());

    return false;
}
// void initFun()
// {
//     Serial.println();
//     Serial.println("========== initFun() CALLED ==========");
//     if (WiFiOn())
//     { 
       
//         getLocalTimeInfo();
//         delay(30000);
//         getWeatherInfo();
//     }
// }

/*


void WiFiOn()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < 15000)
    {
        delay(100);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
        Serial.println("\nWiFi Connected");
    else
        Serial.println("\nWiFi connection failed");
}

*/