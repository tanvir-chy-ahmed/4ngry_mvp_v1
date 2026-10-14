#pragma once

#include <Arduino.h>

enum OtaState
{
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_FLASHING,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR
};

void otaDrawScreen(OtaState state,
                   int progress,
                   size_t bytesDone = 0,
                   size_t bytesTotal = 0,
                   const char *version = nullptr,
                   const char *msg = nullptr);

void otaUpdateScreen(int progress);