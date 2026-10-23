
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

namespace Global {

enum WiFiState {
    WIFI_NO_CREDENTIALS,
    WIFI_SETUP_PORTAL,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_UNAVAILABLE
};

// Initialize Wi-Fi using saved credentials.
// Returns true if initialization starts successfully.
bool WiFiOn();

// Turn Wi-Fi off.
void WiFiOff();

// Request setup AP mode, for example after a 5-second long press.
void requestSetupPortal();

// Request setup AP closure, for example after a tap on Screen 4.
void requestCloseSetupPortal();

// Call frequently from loop().
void updateWiFi();

// Get the current Wi-Fi state.
WiFiState getWiFiState();

// Check whether Wi-Fi is connected.
bool isWiFiConnected();

} // namespace Global
