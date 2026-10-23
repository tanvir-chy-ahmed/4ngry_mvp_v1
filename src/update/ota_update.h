#pragma once

#include <Arduino.h>
#include <Client.h>

String fetchLatestVersion();

void checkfirmwareUpdate();

void downloadAndApplyFirmware(
    const String &version
);

bool startOTAUpdate(Client *client, int contentLength);

// Validate an ESP image header against the chip target this firmware was built for.
bool validateFirmwareImageHeader(const uint8_t *header, size_t length, String &reason);