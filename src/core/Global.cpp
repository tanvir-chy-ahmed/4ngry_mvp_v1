#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
namespace Global
{

    enum WiFiState
    {
        WIFI_NO_CREDENTIALS,
        WIFI_CONNECTING,
        WIFI_CONNECTED,
        WIFI_UNAVAILABLE,
        WIFI_SETUP_PORTAL
    };

    // Persistent storage and web portal
    static Preferences preferences;
    static WebServer server(80);

    static WiFiState wifiState = WIFI_NO_CREDENTIALS;

    static String savedSSID;
    static String savedPassword;

    static bool portalStarted = false;
    static bool wifiStarted = false;
    static bool portalRequested = false;
    static bool credentialsSubmitted = false;

    static uint32_t connectStartedAt = 0;
    static uint32_t lastRetryAt = 0;
    static uint32_t lastStatusCheck = 0;

    static uint8_t failedAttempts = 0;

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t RETRY_INTERVAL_MS = 10000;

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
}
h2 { margin-top: 0; }
p { color: #b8c0cc; line-height: 1.5; }
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
}
</style>
</head>
<body>
<main>
<h2>4ngryROBO Setup</h2>
<p>Enter your home Wi-Fi credentials. 4ngryROBO will save them and connect automatically next time.</p>
<form action="/save" method="POST">
<label>Wi-Fi network name</label>
<input name="ssid" maxlength="32" required placeholder="Your Wi-Fi SSID">
<label>Wi-Fi password</label>
<input name="password" type="password" maxlength="64" placeholder="Your Wi-Fi password">
<button type="submit">Save and Connect</button>
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
                "<html><body style='font-family:Arial;padding:24px'>"
                "<h2>Credentials saved</h2>"
                "<p>4ngryROBO is now connecting to your Wi-Fi. "
                "You can disconnect from the 4ngryROBO network.</p>"
                "</body></html>"
            );

            Serial.println("Wi-Fi credentials saved."); });

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

        // true means initialization started, not that Wi-Fi is connected.
        return true;
    }

    // --------------------------------------------------------
    // Optional: call from a button/long-press handler
    // to reopen setup mode if the user changes routers.
    // --------------------------------------------------------

    void requestSetupPortal()
    {
        portalRequested = true;
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
                Serial.println("Wi-Fi connected.");
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
            }

            wifiState = WIFI_CONNECTED;
            failedAttempts = 0;
            return;
        }

        if (!wifiStarted)
            return;

        if (wifiState == WIFI_CONNECTING &&
            now - connectStartedAt >= CONNECT_TIMEOUT_MS)
        {
            wifiState = WIFI_UNAVAILABLE;
            failedAttempts++;

            Serial.println("Wi-Fi unavailable. Will retry.");
            lastRetryAt = now;
        }

        if (wifiState == WIFI_UNAVAILABLE &&
            now - lastRetryAt >= RETRY_INTERVAL_MS)
        {
            Serial.println("Retrying saved Wi-Fi...");

            WiFi.disconnect(false, false);
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

            wifiState = WIFI_CONNECTING;
            connectStartedAt = now;
            lastRetryAt = now;
        }
    }

    // --------------------------------------------------------
    // Read the current state from your OLED/UI code
    // --------------------------------------------------------

    WiFiState getWiFiState()
    {
        return wifiState;
    }

    bool isWiFiConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    // --------------------------------------------------------
    // Existing function
    // --------------------------------------------------------

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