
#include <WiFi.h>
#include <Arduino.h>
#include <features/watch.h>
#include <features/weather.h>
#include "util/secret.h"

namespace Global
{

    void WiFiOff()
    {
        // No loonger needed the wifi
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }

    bool WiFiOn()
    {
        Serial.println();
        Serial.println("========== WiFiOn() CALLED ==========");

        WiFi.mode(WIFI_STA);
        WiFi.begin(Secret::ssid, Secret::pass);

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
}
