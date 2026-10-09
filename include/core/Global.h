#pragma once

#include <Arduino.h>

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

    bool WiFiOn();
    void updateWiFi();
    void WiFiOff();

    void startSetupPortal();
    void requestSetupPortal();

    WiFiState getWiFiState();
    bool isWiFiConnected();
}
