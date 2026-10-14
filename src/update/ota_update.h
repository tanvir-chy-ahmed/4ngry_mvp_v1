#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

String fetchLatestVersion();
void checkfirmwareUpdate();
void checkFirmwareUpdatePeriodically();
void downloadAndApplyFirmware(const String &version);
bool startOTAUpdate(WiFiClient *client, int contentLength, const String &version);