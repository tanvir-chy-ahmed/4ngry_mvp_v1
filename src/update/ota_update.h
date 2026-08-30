#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

void checkfirmwareUpdate();
String fetchVersion();

void downloadAndApplyFirmware();
bool startOTAUpdate(WiFiClient *client, int contentLength);