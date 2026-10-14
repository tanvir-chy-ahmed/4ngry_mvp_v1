#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "version.h"
#include "core/Global.h"
#include "update/ota_update.h"
#include "update/ota_ui.h"
#include "prefs/version_controller.h"

// ============================================================
// GitHub Configuration
// ============================================================

static const char *latestReleaseUrl =
    "https://api.github.com/repos/tanvir-chy-ahmed/4ngry_mvp_v1/releases/latest";

static const char *firmwareBaseUrl =
    "https://github.com/tanvir-chy-ahmed/4ngry_mvp_v1/releases/download/";

// ============================================================
// OTA Check Timer
// ============================================================

static unsigned long lastPeriodicUpdateCheck = 0;
static const unsigned long PERIODIC_CHECK_INTERVAL =
    6UL * 60UL * 60UL * 1000UL; // 6 hours

// ============================================================
// Fetch Latest GitHub Release Version
// ============================================================

String fetchLatestVersion()
{
    if (!Global::isWiFiConnected())
    {
        Serial.println("Fetch latest version skipped: WiFi not connected.");
        return "";
    }

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(latestReleaseUrl);

    // GitHub API requires a User-Agent header
    http.addHeader("User-Agent", "4ngry-ESP32-Client");

    int httpCode = http.GET();
    Serial.printf("GitHub API HTTP code: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("Failed to fetch release info. HTTP code: %d\n", httpCode);
        http.end();
        return "";
    }

    String payload = http.getString();
    http.end();

    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        Serial.printf("Release JSON parse failed: %s\n", error.c_str());
        return "";
    }

    String tagName = doc["tag_name"].as<String>();
    if (tagName.length() == 0)
    {
        Serial.println("Release tag_name was empty.");
        return "";
    }

    if (tagName.startsWith("v") || tagName.startsWith("V"))
    {
        tagName.remove(0, 1);
    }

    Serial.println("Latest GitHub release: " + tagName);
    return tagName;
}

// ============================================================
// Check Firmware Update
// ============================================================

void checkfirmwareUpdate()
{
    if (!Global::isWiFiConnected())
    {
        Serial.println("OTA check skipped: WiFi not connected.");
        return;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("Checking Firmware Update...");
    Serial.println("==============================");

    String latestVersion = fetchLatestVersion();

    if (latestVersion.length() == 0)
    {
        Serial.println("Failed to fetch latest version info.");
        return;
    }

    String currentVersion = String(FIRMWARE_VERSION);

    Serial.println("Current Version: " + currentVersion);
    Serial.println("Latest Version:  " + latestVersion);

    if (latestVersion != currentVersion)
    {
        Serial.println("New firmware detected! Starting OTA update...");
        downloadAndApplyFirmware(latestVersion);
    }
    else
    {
        Serial.println("Firmware is already up to date.");
    }
}

// ============================================================
// Periodic background check helper
// ============================================================

void checkFirmwareUpdatePeriodically()
{
    unsigned long now = millis();

    // Avoid checking immediately after booting; let system stabilize for 45s
    if (now < 45000)
        return;

    if (!Global::isWiFiConnected())
        return;

    if (now - lastPeriodicUpdateCheck >= PERIODIC_CHECK_INTERVAL || lastPeriodicUpdateCheck == 0)
    {
        lastPeriodicUpdateCheck = now;
        checkfirmwareUpdate();
    }
}

// ============================================================
// Download Firmware
// ============================================================

void downloadAndApplyFirmware(const String &version)
{
    String firmwareUrl =
        String(firmwareBaseUrl) +
        "v" + version +
        "/4ngry-" + version + ".bin";

    Serial.println("Firmware download URL: " + firmwareUrl);

    otaDrawScreen(OTA_STATE_CHECKING, 0, 0, 0, version.c_str(), "Connecting to GitHub...");

    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(firmwareUrl);
    http.addHeader("User-Agent", "4ngry-ESP32-Client");

    int httpCode = http.GET();
    Serial.printf("Firmware download HTTP GET code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK)
    {
        int contentLength = http.getSize();
        Serial.printf("Firmware size: %d bytes\n", contentLength);

        if (contentLength > 10000) // Minimum sane firmware size
        {
            WiFiClient *stream = http.getStreamPtr();

            if (startOTAUpdate(stream, contentLength, version))
            {
                Serial.println("OTA update completed successfully!");
                VersionController::saveVersion(version);
                otaDrawScreen(OTA_STATE_SUCCESS, 100, contentLength, contentLength, version.c_str());

                // 1. Close HTTP stream
                http.end();

                // 2. Show success screen for 2 seconds
                delay(2000);

                // 3. Cleanly shut down Wi-Fi to stop background DMA/FreeRTOS tasks
                Serial.println("Shutting down Wi-Fi before reboot...");
                WiFi.disconnect(true, true);
                WiFi.mode(WIFI_OFF);
                delay(150);

                // 4. CRITICAL FIX FOR ESP32-C3:
                // GPIO 9 (OLED_SCL) and GPIO 8 (OLED_SDA) are hardware strapping pins!
                // GPIO 9 MUST be HIGH at reset to boot from SPI Flash (if LOW, it enters UART Download Mode).
                // Release the hardware I2C bus and configure pins as inputs with pull-up resistors.
                Serial.println("Releasing I2C bus and pulling strapping pins HIGH...");
                Wire.end();
                pinMode(OLED_SCL, INPUT_PULLUP);
                pinMode(OLED_SDA, INPUT_PULLUP);

                // 5. Flush serial buffer
                Serial.println("Rebooting into new firmware...");
                Serial.flush();
                delay(200);

                // 6. Clean hardware reset
                esp_restart();
            }
            else
            {
                Serial.println("OTA update write/verify failed.");
                otaDrawScreen(OTA_STATE_ERROR, 0, 0, 0, version.c_str(), "Flash write failed");
                delay(3000);
            }
        }
        else
        {
            Serial.println("Invalid firmware size returned.");
            otaDrawScreen(OTA_STATE_ERROR, 0, 0, 0, version.c_str(), "Invalid file size");
            delay(3000);
        }
    }
    else
    {
        Serial.printf("Failed to download firmware. HTTP code: %d\n", httpCode);
        char errBuf[32];
        snprintf(errBuf, sizeof(errBuf), "HTTP error %d", httpCode);
        otaDrawScreen(OTA_STATE_ERROR, 0, 0, 0, version.c_str(), errBuf);
        delay(3000);
    }

    http.end();
}

// ============================================================
// OTA Flash Write
// ============================================================

bool startOTAUpdate(WiFiClient *client, int contentLength, const String &version)
{
    Serial.println("Initializing OTA flash partition...");

    if (!Update.begin(contentLength))
    {
        Serial.printf("Update.begin failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("Writing firmware to flash...");

    size_t written = 0;
    int progress = 0;
    int lastDrawnProgress = -1;
    unsigned long lastUiDrawTime = 0;

    const unsigned long timeoutDuration = 45000; // 45 seconds per packet timeout
    unsigned long lastDataTime = millis();

    // 1024-byte chunk buffer for fast transfer
    uint8_t buffer[1024];

    while (written < (size_t)contentLength)
    {
        int avail = client->available();
        if (avail > 0)
        {
            size_t bytesToRead = (avail > (int)sizeof(buffer)) ? sizeof(buffer) : avail;
            size_t len = client->read(buffer, bytesToRead);

            if (len > 0)
            {
                lastDataTime = millis();

                size_t writtenNow = Update.write(buffer, len);
                if (writtenNow != len)
                {
                    Serial.printf("Flash write mismatch! Expected %d, wrote %d\n", len, writtenNow);
                    Update.abort();
                    return false;
                }

                written += writtenNow;
                progress = (written * 100) / contentLength;

                // Throttled UI update (every 75ms or on 2% progress leap)
                // This keeps display responsive without slowing down the network stream
                unsigned long now = millis();
                if (now - lastUiDrawTime >= 75 || (progress - lastDrawnProgress >= 2))
                {
                    lastUiDrawTime = now;
                    lastDrawnProgress = progress;
                    otaDrawScreen(OTA_STATE_DOWNLOADING, progress, written, contentLength, version.c_str());
                }
            }
        }

        // Timeout check
        if (millis() - lastDataTime > timeoutDuration)
        {
            Serial.println("OTA Timeout: No data received.");
            Update.abort();
            return false;
        }

        yield();
    }

    Serial.println("Download complete. Finalizing flash...");
    otaDrawScreen(OTA_STATE_FLASHING, 100, written, contentLength, version.c_str());

    if (written != (size_t)contentLength)
    {
        Serial.printf("Write size mismatch: expected %d, got %d\n", contentLength, written);
        Update.abort();
        return false;
    }

    if (!Update.end(true))
    {
        Serial.printf("Update.end failed: %s\n", Update.errorString());
        return false;
    }

    const esp_partition_t *bootPart = esp_ota_get_boot_partition();
    if (bootPart != nullptr)
    {
        Serial.printf("Configured next boot partition: %s (address 0x%08X)\n",
                      bootPart->label, bootPart->address);
    }
    else
    {
        Serial.println("Warning: esp_ota_get_boot_partition returned null.");
    }

    Serial.println("Firmware flash successfully verified.");
    return true;
}