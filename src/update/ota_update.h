#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

String fetchLatestVersion();

void checkfirmwareUpdate();

void downloadAndApplyFirmware(
    const String &version
);

bool startOTAUpdate(
    WiFiClient *client,
    int contentLength
);