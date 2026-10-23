#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ElegantOTA.h>

#include "core/global.h"
#include "features/DrawingCanvas.h"

namespace Global
{

    static Preferences preferences;
    static WebServer server(80);

    static WiFiState wifiState = WIFI_NO_CREDENTIALS;

    static String savedSSID;
    static String savedPassword;

    static bool portalStarted = false;
    static bool wifiStarted = false;
    static bool portalRequested = false;
    static bool closePortalRequested = false;
    static bool credentialsSubmitted = false;
    static bool routesRegistered = false;

    static uint32_t connectStartedAt = 0;
    static uint32_t lastRetryAt = 0;
    static uint32_t lastStatusCheck = 0;

    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
    static constexpr uint32_t RETRY_INTERVAL_MS = 10000;

    // ------------------------------------------------------------
    // Load saved Wi-Fi credentials
    // ------------------------------------------------------------
    static bool loadCredentials()
    {
        if (!preferences.begin("4ngryROBO", true))
        {
            savedSSID = "";
            savedPassword = "";
            return false;
        }

        savedSSID = preferences.getString("ssid", "");
        savedPassword = preferences.getString("password", "");
        preferences.end();

        savedSSID.trim();
        return savedSSID.length() > 0;
    }

    // ------------------------------------------------------------
    // Register routes once on the existing WebServer
    // ------------------------------------------------------------
    static void registerPortalRoutes()
    {
        if (routesRegistered)
            return;

        server.on("/", HTTP_GET, []()
                  {
        server.sendHeader("Location", "/canvas", true);
        server.send(302, "text/plain", "Opening 4ngryROBO admin console"); });

        server.on("/save", HTTP_POST, []()
                  {
        String ssid = server.arg("ssid");
        String password = server.arg("password");
        ssid.trim();

        if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64) {
            server.send(400, "text/plain",
                        "Invalid credentials. Go back and try again.");
            return;
        }

        if (!preferences.begin("4ngryROBO", false)) {
            server.send(500, "text/plain",
                        "Could not open credential storage.");
            return;
        }

        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();

        savedSSID = ssid;
        savedPassword = password;
        credentialsSubmitted = true;

        server.send(200, "text/html; charset=utf-8",
            "<!doctype html><html><body style='font-family:Arial;padding:24px'>"
            "<h2>Credentials saved</h2>"
            "<p>4ngryROBO is connecting to your Wi-Fi. You can leave this page.</p>"
            "</body></html>");

        Serial.println("[WiFi] Credentials saved."); });

        // Register canvas routes on this same WebServer instance.
        DrawingCanvas::registerRoutes(server);

        // Register ElegantOTA on the existing server without OTA-page authentication.
        ElegantOTA.begin(&server);
        Serial.println("[OTA] ElegantOTA routes registered at /update (no login required)");

        server.onNotFound([]()
                          {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", ""); });

        routesRegistered = true;
    }

    // ------------------------------------------------------------
    // Start the setup access point
    // ------------------------------------------------------------
    void startSetupPortal()
    {
        Serial.println("\n[WiFi] Starting 4ngryROBO setup AP...");

        portalStarted = false;
        wifiStarted = false;
        credentialsSubmitted = false;
        closePortalRequested = false;

        WiFi.disconnect(false, false);

        bool modeOK = WiFi.mode(WIFI_AP);
        Serial.printf("[WiFi] WIFI_AP mode: %s\n", modeOK ? "OK" : "FAILED");
        delay(200);

        bool apOK = WiFi.softAP("4ngryROBO", "12345678");
        Serial.printf("[WiFi] softAP: %s\n", apOK ? "SUCCESS" : "FAILED");

        if (!apOK)
        {
            Serial.println("[WiFi] ERROR: failed to create AP.");
            wifiState = WIFI_NO_CREDENTIALS;
            return;
        }

        registerPortalRoutes();
        server.begin();

        portalStarted = true;
        portalRequested = false;
        wifiState = WIFI_SETUP_PORTAL;

        Serial.print("[WiFi] AP IP: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("[WiFi] SSID: 4ngryROBO");
        Serial.println("[WiFi] Password: 12345678");
        Serial.println("[WiFi] Setup: http://192.168.4.1");
        Serial.println("[WiFi] Canvas: http://192.168.4.1/canvas");
    }

    // ------------------------------------------------------------
    // Start STA mode using the saved credentials
    // ------------------------------------------------------------
    static void beginConnection()
    {
        if (savedSSID.isEmpty())
        {
            Serial.println("[WiFi] No saved SSID; keeping setup AP active.");
            startSetupPortal();
            return;
        }

        Serial.print("[WiFi] Connecting to saved network: ");
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

    // ------------------------------------------------------------
    // Initialize Wi-Fi during setup()
    // ------------------------------------------------------------
    bool WiFiOn()
    {
        Serial.println("[WiFi] Initializing 4ngryROBO Wi-Fi...");
        WiFi.persistent(false);
        WiFi.setAutoReconnect(true);

        if (!loadCredentials())
        {
            Serial.println("[WiFi] No saved SSID; starting setup AP.");
            startSetupPortal();
            return portalStarted;
        }

        beginConnection();
        return true; // Initialization started; connection may still be pending.
    }

    // ------------------------------------------------------------
    // Request AP mode from a long-press handler
    // ------------------------------------------------------------
    void requestSetupPortal()
    {
        // If boot already started the setup AP (no saved credentials), reuse it.
        // Do not leave a stale request that would restart the AP after the user closes it.
        if (portalStarted)
        {
            portalRequested = false;
            return;
        }
        portalRequested = true;
    }

    // ------------------------------------------------------------
    // Request AP closure from a single tap while AP mode is active.
    // The AP is closed even without credentials; the Wi-Fi radio is then turned off.
    // ------------------------------------------------------------
    void requestCloseSetupPortal()
    {
        closePortalRequested = true;
    }

    // ------------------------------------------------------------
    // Call frequently from loop()
    // ------------------------------------------------------------
    void updateWiFi()
    {
        const uint32_t now = millis();

        if (portalStarted)
        {
            server.handleClient();
            ElegantOTA.loop();

            if (credentialsSubmitted)
            {
                credentialsSubmitted = false;

                server.stop();
                WiFi.softAPdisconnect(true);
                portalStarted = false;
                wifiStarted = false;

                beginConnection();
                return;
            }

            if (closePortalRequested)
            {
                closePortalRequested = false;
                portalRequested = false;

                Serial.println("[WiFi] Closing setup AP on user request.");
                server.stop();
                WiFi.softAPdisconnect(true);
                portalStarted = false;
                wifiStarted = false;

                if (savedSSID.isEmpty())
                {
                    WiFi.disconnect(true, false);
                    WiFi.mode(WIFI_OFF);
                    wifiState = WIFI_NO_CREDENTIALS;
                    Serial.println("[WiFi] No saved SSID; AP and radio are off to save battery.");
                }
                else
                {
                    beginConnection();
                }
                return;
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
                Serial.println("[WiFi] Connected.");
                Serial.print("[WiFi] Local IP: ");
                Serial.println(WiFi.localIP());
            }

            wifiState = WIFI_CONNECTED;
            return;
        }

        if (!wifiStarted)
            return;

        if (wifiState == WIFI_CONNECTING &&
            now - connectStartedAt >= CONNECT_TIMEOUT_MS)
        {
            wifiState = WIFI_UNAVAILABLE;
            lastRetryAt = now;
            Serial.println("[WiFi] Connection timed out; will retry.");
        }

        if (wifiState == WIFI_UNAVAILABLE &&
            now - lastRetryAt >= RETRY_INTERVAL_MS)
        {
            Serial.println("[WiFi] Retrying saved network...");

            WiFi.disconnect(false, false);
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

            wifiState = WIFI_CONNECTING;
            connectStartedAt = now;
            lastRetryAt = now;
        }
    }

    WiFiState getWiFiState()
    {
        return wifiState;
    }

    bool isWiFiConnected()
    {
        return WiFi.status() == WL_CONNECTED;
    }

    // ------------------------------------------------------------
    // Turn Wi-Fi off
    // ------------------------------------------------------------
    void WiFiOff()
    {
        server.stop();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);

        portalStarted = false;
        wifiStarted = false;
        portalRequested = false;
        closePortalRequested = false;
        credentialsSubmitted = false;
        wifiState = WIFI_NO_CREDENTIALS;
    }

} // namespace Global
