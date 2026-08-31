#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "version.h"
#include "core/Global.h"
#include "update/ota_update.h"
#include "prefs/version_controller.h"

// ============================================================
// GitHub Configuration
// ============================================================

const char *latestReleaseUrl =
    "https://api.github.com/repos/tanvir-chy-ahmed/4ngry_mvp_v1/releases/latest";

const char *firmwareBaseUrl =
    "https://github.com/tanvir-chy-ahmed/4ngry_mvp_v1/releases/download/";

// ============================================================
// OTA Check Timer
// ============================================================

unsigned long lastUpdateCheck = 0;

const unsigned long updateCheckInterval =
    60UL * 60UL * 1000UL; // 1 hour

// ============================================================
// Fetch Latest GitHub Release Version
// ============================================================

String fetchLatestVersion()
{
    HTTPClient http;

    http.begin(latestReleaseUrl);

    // GitHub API User-Agent
    http.addHeader(
        "User-Agent",
        "4ngry-ESP32");

    int httpCode = http.GET();

    Serial.printf(
        "GitHub API HTTP code: %d\n",
        httpCode);

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf(
            "Failed to fetch latest release. HTTP code: %d\n",
            httpCode);

        http.end();

        return "";
    }

    String payload = http.getString();

    http.end();

    // --------------------------------------------------------
    // Parse JSON
    // --------------------------------------------------------

    JsonDocument doc;

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error)
    {
        Serial.printf(
            "JSON parsing failed: %s\n",
            error.c_str());

        return "";
    }

    String tagName =
        doc["tag_name"].as<String>();

    if (tagName == "")
    {
        Serial.println(
            "GitHub tag_name not found.");

        return "";
    }

    // GitHub gives:
    //
    // v1.0.1
    //
    // We need:
    //
    // 1.0.1

    if (tagName.startsWith("v"))
    {
        tagName.remove(0, 1);
    }

    Serial.println(
        "Latest GitHub version: " + tagName);

    return tagName;
}

// ============================================================
// Check Firmware Update
// ============================================================

void checkfirmwareUpdate()
{
    if (!WiFiOn())
    {
        Serial.println(
            "OTA check skipped: WiFi not connected.");

        return;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("Checking Firmware Update...");
    Serial.println("==============================");

    // --------------------------------------------------------
    // Get latest version
    // --------------------------------------------------------

    String latestVersion =
        fetchLatestVersion();

    if (latestVersion == "")
    {
        Serial.println(
            "Failed to fetch latest version.");

        return;
    }

    // --------------------------------------------------------
    // Current version
    // --------------------------------------------------------

    String currentVersion =
        String(FIRMWARE_VERSION);

    Serial.println(
        "Current Firmware Version: " +
        currentVersion);

    Serial.println(
        "Latest Firmware Version: " +
        latestVersion);

    // --------------------------------------------------------
    // Compare
    // --------------------------------------------------------

    if (latestVersion != currentVersion)
    {
        Serial.println(
            "New firmware available!");

        Serial.println(
            "Starting OTA update...");

        downloadAndApplyFirmware(
            latestVersion);
    }
    else
    {
        Serial.println(
            "Device is up to date.");
    }
}

// ============================================================
// Download Firmware
// ============================================================

void downloadAndApplyFirmware(
    const String &version)
{
    // --------------------------------------------------------
    // Build firmware URL
    // --------------------------------------------------------
    //
    // version = 1.0.1
    //
    // Result:
    //
    // https://github.com/tanvir-chy-ahmed/
    // 4ngry_mvp_v1/releases/download/
    // v1.0.1/4ngry-1.0.1.bin
    //
    // --------------------------------------------------------

    String firmwareUrl =
        String(firmwareBaseUrl) +
        "v" +
        version +
        "/4ngry-" +
        version +
        ".bin";

    Serial.println();
    Serial.println("Firmware URL:");
    Serial.println(firmwareUrl);

    HTTPClient http;

    // GitHub Release redirects to its
    // actual asset server.
    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS);

    // SAME METHOD AS YOUR WORKING CODE
    http.begin(firmwareUrl);

    int httpCode = http.GET();

    Serial.printf(
        "HTTP GET code: %d\n",
        httpCode);

    // --------------------------------------------------------
    // Download successful
    // --------------------------------------------------------

    if (httpCode == HTTP_CODE_OK)
    {
        int contentLength =
            http.getSize();

        Serial.printf(
            "Firmware size: %d bytes\n",
            contentLength);

        if (contentLength > 0)
        {
            WiFiClient *stream =
                http.getStreamPtr();

            if (startOTAUpdate(
                    stream,
                    contentLength))
            {
                Serial.println("OTA update successful!");

                // Save the version that was just installed
                VersionController::saveVersion(version);

                Serial.println(
                    "Installed firmware version: " +
                    VersionController::getVersion());

                Serial.println("Restarting...");

                http.end();

                delay(2000);

                ESP.restart();
            }
            else
            {
                Serial.println(
                    "OTA update failed.");
            }
        }
        else
        {
            Serial.println(
                "Invalid firmware size.");
        }
    }
    else
    {
        Serial.printf(
            "Failed to fetch firmware. HTTP code: %d\n",
            httpCode);
    }

    http.end();
}

// ============================================================
// OTA Flash Write
// ============================================================

bool startOTAUpdate(
    WiFiClient *client,
    int contentLength)
{
    Serial.println(
        "Initializing update...");

    // --------------------------------------------------------
    // Start OTA
    // --------------------------------------------------------

    if (!Update.begin(contentLength))
    {
        Serial.printf(
            "Update begin failed: %s\n",
            Update.errorString());

        return false;
    }

    Serial.println(
        "Writing firmware...");

    size_t written = 0;

    int progress = 0;
    int lastProgress = -1;

    // --------------------------------------------------------
    // Timeout
    // --------------------------------------------------------

    const unsigned long timeoutDuration =
        120UL * 1000UL;

    unsigned long lastDataTime =
        millis();

    // --------------------------------------------------------
    // Download buffer
    // --------------------------------------------------------

    uint8_t buffer[128];

    // --------------------------------------------------------
    // Download + write
    // --------------------------------------------------------

    while (written < (size_t)contentLength)
    {
        if (client->available())
        {
            size_t len =
                client->read(
                    buffer,
                    sizeof(buffer));

            if (len > 0)
            {
                // Important:
                // reset timeout when data arrives
                lastDataTime = millis();

                // Write to flash
                size_t writtenNow =
                    Update.write(
                        buffer,
                        len);

                if (writtenNow != len)
                {
                    Serial.println(
                        "Flash write failed.");

                    Serial.printf(
                        "Expected: %d, Written: %d\n",
                        len,
                        writtenNow);

                    Update.abort();

                    return false;
                }

                written += writtenNow;

                // ------------------------------------------------
                // Progress
                // ------------------------------------------------

                progress =
                    (written * 100) /
                    contentLength;

                if (progress != lastProgress)
                {
                    Serial.printf(
                        "Writing Progress: %d%%\n",
                        progress);

                    lastProgress =
                        progress;
                }
            }
        }

        // --------------------------------------------------------
        // Timeout
        // --------------------------------------------------------

        if (millis() - lastDataTime >
            timeoutDuration)
        {
            Serial.println(
                "Timeout: No data received.");

            Update.abort();

            return false;
        }

        yield();
    }

    Serial.println();
    Serial.println(
        "Writing complete.");

    // --------------------------------------------------------
    // Verify size
    // --------------------------------------------------------

    if (written !=
        (size_t)contentLength)
    {
        Serial.printf(
            "Write incomplete. "
            "Expected %d but got %d bytes\n",
            contentLength,
            written);

        Update.abort();

        return false;
    }

    // --------------------------------------------------------
    // Finalize OTA
    // --------------------------------------------------------

    if (!Update.end())
    {
        Serial.printf(
            "Update end failed: %s\n",
            Update.errorString());

        return false;
    }

    // --------------------------------------------------------
    // Verify final state
    // --------------------------------------------------------

    if (!Update.isFinished())
    {
        Serial.println(
            "Update did not finish correctly.");

        return false;
    }

    Serial.println(
        "Update successfully completed.");

    return true;
}