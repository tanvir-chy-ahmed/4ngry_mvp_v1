#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "version.h"
#include "core/Global.h"
#include "update/ota_update.h"

// ============================================================
// GitHub OTA Configuration
// ============================================================

const char *latestReleaseUrl =
    "https://api.github.com/repos/tanvir-chy-ahmed/4ngry_mvp_v1/releases/latest";

const char *githubFirmwareBaseUrl =
    "https://github.com/tanvir-chy-ahmed/4ngry_mvp_v1/releases/download/";

// ============================================================
// OTA Check Timer
// ============================================================

unsigned long lastUpdateCheck = 0;

// Check every 1 hour
const unsigned long updateCheckInterval =
    60UL * 60UL * 1000UL;

// ============================================================
// Fetch Latest Version From GitHub
// ============================================================

String fetchLatestVersion()
{
    HTTPClient http;

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    http.begin(latestReleaseUrl);

    // GitHub API requires a User-Agent
    http.addHeader("User-Agent", "4ngry-ESP32");

    int responseCode = http.GET();

    Serial.printf(
        "GitHub API HTTP code: %d\n",
        responseCode
    );

    if (responseCode != HTTP_CODE_OK)
    {
        Serial.printf(
            "Failed to fetch latest release. HTTP: %d\n",
            responseCode
        );

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
            "GitHub JSON parsing failed: %s\n",
            error.c_str()
        );

        return "";
    }

    // Example:
    //
    // "tag_name": "v1.0.3"
    //

    String latestVersion =
        doc["tag_name"].as<String>();

    if (latestVersion == "")
    {
        Serial.println(
            "GitHub release tag not found."
        );

        return "";
    }

    // Remove "v"
    if (latestVersion.startsWith("v"))
    {
        latestVersion.remove(0, 1);
    }

    Serial.println(
        "Latest GitHub version: " +
        latestVersion
    );

    return latestVersion;
}

// ============================================================
// Check Firmware Update
// ============================================================

void checkfirmwareUpdate()
{
    // --------------------------------------------------------
    // Check WiFi
    // --------------------------------------------------------

    if (!WiFiOn())
    {
        Serial.println(
            "OTA check skipped: WiFi not connected."
        );

        return;
    }

    Serial.println();
    Serial.println("==============================");
    Serial.println("Checking Firmware Update...");
    Serial.println("==============================");

    // --------------------------------------------------------
    // Get latest GitHub version
    // --------------------------------------------------------

    String latestVersion =
        fetchLatestVersion();

    if (latestVersion == "")
    {
        Serial.println(
            "Could not determine latest firmware version."
        );

        return;
    }

    // --------------------------------------------------------
    // Current firmware
    // --------------------------------------------------------

    String currentVersion =
        String(FIRMWARE_VERSION);

    Serial.println(
        "Current Firmware: " +
        currentVersion
    );

    Serial.println(
        "Latest Firmware:  " +
        latestVersion
    );

    // --------------------------------------------------------
    // Compare
    // --------------------------------------------------------

    if (latestVersion != currentVersion)
    {
        Serial.println();
        Serial.println(
            "New firmware available!"
        );

        Serial.println(
            "Starting OTA update..."
        );

        downloadAndApplyFirmware(
            latestVersion
        );
    }
    else
    {
        Serial.println(
            "Firmware is already up to date."
        );
    }

    Serial.println("==============================");
}

// ============================================================
// Download And Apply Firmware
// ============================================================

void downloadAndApplyFirmware(
    const String &version
)
{
    // --------------------------------------------------------
    // Construct firmware URL
    // --------------------------------------------------------
    //
    // Example:
    //
    // version = 1.0.3
    //
    // Result:
    //
    // https://github.com/tanvir-chy-ahmed/
    // 4ngry_mvp_v1/releases/download/
    // v1.0.3/4ngry-1.0.3.bin
    //
    // --------------------------------------------------------

    String firmwareUrl =
        String(githubFirmwareBaseUrl) +
        "v" +
        version +
        "/4ngry-" +
        version +
        ".bin";

    Serial.println();
    Serial.println("Firmware URL:");
    Serial.println(firmwareUrl);
    Serial.println();

    HTTPClient http;

    http.setFollowRedirects(
        HTTPC_STRICT_FOLLOW_REDIRECTS
    );

    http.begin(firmwareUrl);

    http.addHeader(
        "User-Agent",
        "4ngry-ESP32"
    );

    int responseCode =
        http.GET();

    Serial.printf(
        "Firmware HTTP code: %d\n",
        responseCode
    );

    // --------------------------------------------------------
    // Firmware downloaded successfully
    // --------------------------------------------------------

    if (responseCode == HTTP_CODE_OK)
    {
        int contentLength =
            http.getSize();

        Serial.printf(
            "Firmware size: %d bytes\n",
            contentLength
        );

        if (contentLength <= 0)
        {
            Serial.println(
                "Invalid firmware size."
            );

            http.end();

            return;
        }

        WiFiClient *stream =
            http.getStreamPtr();

        // ----------------------------------------------------
        // Start OTA
        // ----------------------------------------------------

        if (startOTAUpdate(
                stream,
                contentLength))
        {
            Serial.println();
            Serial.println(
                "OTA update successful!"
            );

            Serial.println(
                "Restarting device..."
            );

            delay(2000);

            ESP.restart();
        }
        else
        {
            Serial.println();
            Serial.println(
                "OTA update failed."
            );
        }
    }
    else
    {
        Serial.printf(
            "Failed to download firmware. HTTP: %d\n",
            responseCode
        );
    }

    http.end();
}

// ============================================================
// OTA Write
// ============================================================

bool startOTAUpdate(
    WiFiClient *client,
    int contentLength
)
{
    Serial.println(
        "Initializing firmware update..."
    );

    // --------------------------------------------------------
    // Start Update
    // --------------------------------------------------------

    if (!Update.begin(contentLength))
    {
        Serial.printf(
            "Update.begin() failed: %s\n",
            Update.errorString()
        );

        return false;
    }

    Serial.println(
        "Writing firmware..."
    );

    size_t written = 0;

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

    uint8_t buffer[1024];

    // --------------------------------------------------------
    // Write firmware
    // --------------------------------------------------------

    while (written < (size_t)contentLength)
    {
        // ----------------------------------------------------
        // Data available
        // ----------------------------------------------------

        if (client->available())
        {
            size_t len =
                client->read(
                    buffer,
                    sizeof(buffer)
                );

            if (len > 0)
            {
                // Reset timeout because
                // data was received
                lastDataTime = millis();

                // Write to flash
                size_t writtenNow =
                    Update.write(
                        buffer,
                        len
                    );

                // Verify write
                if (writtenNow != len)
                {
                    Serial.println(
                        "OTA flash write failed."
                    );

                    Serial.printf(
                        "Expected: %d bytes, "
                        "Written: %d bytes\n",
                        len,
                        writtenNow
                    );

                    Update.abort();

                    return false;
                }

                written += writtenNow;

                // ------------------------------------------------
                // Progress
                // ------------------------------------------------

                int progress =
                    (written * 100) /
                    contentLength;

                if (progress != lastProgress)
                {
                    Serial.printf(
                        "OTA Progress: %d%%\n",
                        progress
                    );

                    lastProgress =
                        progress;
                }
            }
        }

        // ----------------------------------------------------
        // Timeout
        // ----------------------------------------------------

        if (millis() - lastDataTime >
            timeoutDuration)
        {
            Serial.println(
                "OTA timeout: "
                "No data received."
            );

            Update.abort();

            return false;
        }

        // Allow ESP32 background tasks
        yield();
    }

    // --------------------------------------------------------
    // Verify downloaded size
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "Firmware download complete."
    );

    Serial.printf(
        "Expected: %d bytes\n",
        contentLength
    );

    Serial.printf(
        "Received: %d bytes\n",
        written
    );

    if (written !=
        (size_t)contentLength)
    {
        Serial.println(
            "Firmware write incomplete."
        );

        Update.abort();

        return false;
    }

    // --------------------------------------------------------
    // Finalize OTA
    // --------------------------------------------------------

    if (!Update.end())
    {
        Serial.printf(
            "Update.end() failed: %s\n",
            Update.errorString()
        );

        return false;
    }

    // --------------------------------------------------------
    // Verify
    // --------------------------------------------------------

    if (!Update.isFinished())
    {
        Serial.println(
            "OTA update not finished."
        );

        return false;
    }

    Serial.println();
    Serial.println(
        "================================"
    );
    Serial.println(
        " OTA UPDATE SUCCESSFUL"
    );
    Serial.println(
        "================================"
    );

    return true;
}