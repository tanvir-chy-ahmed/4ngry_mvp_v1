#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "core/Global.h"

namespace Global
{
    // Persistent storage and web portal
    static Preferences preferences;
    static WebServer server(80);

    static WiFiState wifiState = WIFI_NO_CREDENTIALS;

    static String savedSSID = "";
    static String savedPassword = "";

    static bool portalStarted = false;
    static bool wifiStarted = false;
    static bool portalRequested = false;
    static bool credentialsSubmitted = false;

    static uint32_t connectStartedAt = 0;
    static uint32_t lastRetryAt = 0;
    static uint32_t lastStatusCheck = 0;

    static uint8_t failedAttempts = 0;

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;

    static uint32_t getRetryIntervalMs()
    {
        if (failedAttempts <= 1)
            return 10000; // 10s
        if (failedAttempts == 2)
            return 20000; // 20s
        if (failedAttempts == 3)
            return 30000; // 30s
        return 60000;     // 60s max backoff
    }

    // --------------------------------------------------------
    // Web page
    // --------------------------------------------------------

    static String makeSetupPage()
    {
        String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>4ngryROBO Wi-Fi Setup</title>
<style>
body {
    margin: 0;
    padding: 24px;
    font-family: Arial, sans-serif;
    background: #101318;
    color: #f2f2f2;
}
main {
    max-width: 380px;
    margin: 35px auto;
    padding: 24px;
    background: #1c222b;
    border-radius: 16px;
    box-shadow: 0 4px 20px rgba(0,0,0,0.5);
}
h2 { margin-top: 0; color: #f0b429; }
p { color: #b8c0cc; line-height: 1.5; font-size: 14px; }
input, button {
    box-sizing: border-box;
    width: 100%;
    padding: 13px;
    margin-top: 12px;
    border-radius: 8px;
    font-size: 16px;
}
input {
    border: 1px solid #485363;
    background: #11161d;
    color: white;
}
button {
    border: 0;
    background: #f0b429;
    color: #161616;
    font-weight: bold;
    cursor: pointer;
}
</style>
</head>
<body>
<main>
<h2>4ngryROBO Setup</h2>
<p>Enter your 2.4GHz Wi-Fi credentials to connect 4ngryROBO. Settings are securely stored.</p>
<form action="/save" method="POST">
<label style="font-size:13px;color:#a0aec0">Wi-Fi network name (SSID)</label>
<input name="ssid" maxlength="32" required placeholder="e.g. MyHomeWiFi">
<label style="font-size:13px;color:#a0aec0">Wi-Fi password</label>
<input name="password" type="password" maxlength="64" placeholder="Password">
<button type="submit">Save &amp; Connect</button>
</form>
</main>
</body>
</html>
)rawliteral";

        return page;
    }

    // --------------------------------------------------------
    // Start setup access point
    // --------------------------------------------------------

    void startSetupPortal()
    {
        Serial.println("Starting 4ngryROBO setup portal...");

        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_AP_STA);

        bool apStarted = WiFi.softAP("4ngryROBO", "12345678");

        if (!apStarted)
        {
            Serial.println("Failed to start setup access point.");
            return;
        }

        portalStarted = true;
        portalRequested = false;
        wifiState = WIFI_SETUP_PORTAL;

        Serial.print("Setup AP IP: ");
        Serial.println(WiFi.softAPIP());

        server.on("/", HTTP_GET, []()
                  { server.send(200, "text/html", makeSetupPage()); });

        server.on("/save", HTTP_POST, []()
                  {
            String ssid = server.arg("ssid");
            String password = server.arg("password");

            ssid.trim();

            if (ssid.length() == 0 ||
                ssid.length() > 32 ||
                password.length() > 64)
            {
                server.send(
                    400,
                    "text/plain",
                    "Invalid Wi-Fi credentials. Go back and try again."
                );
                return;
            }

            preferences.begin("4ngryROBO", false);
            preferences.putString("ssid", ssid);
            preferences.putString("password", password);
            preferences.end();

            savedSSID = ssid;
            savedPassword = password;
            credentialsSubmitted = true;

            server.send(
                200,
                "text/html",
                "<html><body style='font-family:Arial;padding:24px;background:#101318;color:#fff'>"
                "<h2 style='color:#f0b429'>Credentials Saved!</h2>"
                "<p>4ngryROBO is now connecting to <b>" + ssid + "</b>.</p>"
                "<p>You can disconnect from this network.</p>"
                "</body></html>"
            );

            Serial.println("Wi-Fi credentials saved: " + ssid); });

        server.onNotFound([]()
                          {
            server.sendHeader("Location", "/", true);
            server.send(302, "text/plain", ""); });

        server.begin();

        Serial.println("Setup page ready at http://192.168.4.1");
    }

    // --------------------------------------------------------
    // Read credentials from NVS
    // --------------------------------------------------------

    static bool loadCredentials()
    {
        if (!preferences.begin("4ngryROBO", true))
            return false;

        savedSSID = preferences.getString("ssid", "");
        savedPassword = preferences.getString("password", "");

        preferences.end();
        String name = "Hello";

        return savedSSID.length() > 0;
    }

    // --------------------------------------------------------
    // Start a non-blocking Wi-Fi connection
    // --------------------------------------------------------

    static void beginConnection()
    {
        if (savedSSID.length() == 0)
        {
            startSetupPortal();
            return;
        }

        Serial.print("Connecting to saved Wi-Fi: ");
        Serial.println(savedSSID);

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.persistent(false);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

        wifiState = WIFI_CONNECTING;
        wifiStarted = true;
        connectStartedAt = millis();
        lastRetryAt = connectStartedAt;
    }

    // --------------------------------------------------------
    // Existing function: call during device startup
    // --------------------------------------------------------

    bool WiFiOn()
    {
        Serial.println("Initializing 4ngryROBO Wi-Fi...");

        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);

        if (!loadCredentials())
        {
            Serial.println("No saved credentials found.");
            startSetupPortal();
            return true;
        }

        beginConnection();
        return true;
    }

    // --------------------------------------------------------
    // Request opening setup portal
    // --------------------------------------------------------

    void requestSetupPortal()
    {
        portalRequested = true;
    }

    void triggerImmediateReconnect()
    {
        if (savedSSID.length() > 0)
        {
            WiFi.disconnect(false, false);
            beginConnection();
        }
    }

    void resetWiFiCredentials()
    {
        Serial.println("Resetting Wi-Fi credentials from Preferences...");

        if (preferences.begin("4ngryROBO", false))
        {
            preferences.remove("ssid");
            preferences.remove("password");
            preferences.end();
        }

        savedSSID = "";
        savedPassword = "";
        failedAttempts = 0;
        wifiStarted = false;

        WiFi.disconnect(true, true);
        startSetupPortal();
    }

    // --------------------------------------------------------
    // Existing function: call frequently from loop()
    // --------------------------------------------------------

    void updateWiFi()
    {
        uint32_t now = millis();

        if (portalStarted)
        {
            server.handleClient();

            if (credentialsSubmitted)
            {
                credentialsSubmitted = false;

                server.stop();
                WiFi.softAPdisconnect(false);
                portalStarted = false;
                wifiStarted = false;
                failedAttempts = 0;

                beginConnection();
            }

            return;
        }

        if (portalRequested)
        {
            portalRequested = false;
            startSetupPortal();
            return;
        }

        if (now - lastStatusCheck < 250)
            return;

        lastStatusCheck = now;

        if (WiFi.status() == WL_CONNECTED)
        {
            if (wifiState != WIFI_CONNECTED)
            {
                Serial.println("Wi-Fi connected successfully!");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
                Serial.print("RSSI: ");
                Serial.println(WiFi.RSSI());
            }

            wifiState = WIFI_CONNECTED;
            failedAttempts = 0;
            return;
        }

        if (!wifiStarted)
            return;

        // If we were connecting and timed out
        if (wifiState == WIFI_CONNECTING &&
            now - connectStartedAt >= CONNECT_TIMEOUT_MS)
        {
            wifiState = WIFI_UNAVAILABLE;
            failedAttempts++;

            uint32_t nextInterval = getRetryIntervalMs();
            Serial.printf("Wi-Fi unavailable (attempt %d). Will retry in %ds.\n",
                          failedAttempts, (int)(nextInterval / 1000));
            lastRetryAt = now;
        }

        // If unavailable, retry with tiered backoff
        if (wifiState == WIFI_UNAVAILABLE &&
            now - lastRetryAt >= getRetryIntervalMs())
        {
            Serial.println("Retrying Wi-Fi connection...");

            WiFi.disconnect(false, false);
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

            wifiState = WIFI_CONNECTING;
            connectStartedAt = now;
            lastRetryAt = now;
        }
    }

    // --------------------------------------------------------
    // Helpers
    // --------------------------------------------------------

    WiFiState getWiFiState()
    {
        return wifiState;
    }

    bool isWiFiConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    int getWiFiRSSI()
    {
        if (WiFi.status() == WL_CONNECTED)
            return WiFi.RSSI();
        return -100;
    }

    String getSavedSSID()
    {
        return savedSSID;
    }

    uint32_t getSecondsUntilRetry()
    {
        if (wifiState != WIFI_UNAVAILABLE)
            return 0;

        uint32_t elapsed = millis() - lastRetryAt;
        uint32_t interval = getRetryIntervalMs();
        if (elapsed >= interval)
            return 0;

        return (interval - elapsed + 999) / 1000;
    }

    void WiFiOff()
    {
        server.stop();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);

        portalStarted = false;
        wifiStarted = false;
        wifiState = WIFI_NO_CREDENTIALS;
    }
}



/*



      - name: Prepare release
        run: |
          mkdir -p release

          cp .pio/build/*/firmware.bin \
             "release/4ngry-${{ steps.version.outputs.version }}.bin"

          echo "Created:"
          ls -lh release/
          
          
          
          */