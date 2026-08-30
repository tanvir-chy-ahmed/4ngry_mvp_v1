#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "version.h"
#include "core/Global.h"

const char *firmwareUrl = "https://github.com/tanvir-chy-ahmed/4ngry_mvp_v1/releases/download/v1.0.0/4ngry-1.0.0.bin";
const char *versionUrl = "https://raw.githubusercontent.com/ittipu/esp32_firmware/refs/heads/main/version.txt";

// --- Update Check Timer ---
unsigned long lastUpdateCheck = 0;
const long updateCheckInterval = 1 * 60 * 1000; // 5 minutes in milliseconds

void checkfirmwareUpdate()
{
    if (WiFiOn)
    {
        String latestVersion = fetchVersion();
        if (latestVersion == "")
        {
            Serial.println("Failed to fetch latest version");
            return;
        }
        Serial.println("Current Firmware Version: " + String(FIRMWARE_VERSION));
        Serial.println("Latest Firmware Version: " + latestVersion);
        // Step 2: Compare versions
        if (latestVersion != FIRMWARE_VERSION)
        {
            Serial.println("New firmware available. Starting OTA update...");
            downloadAndApplyFirmware();
        }
        else
        {
            Serial.println("Device is up to date.");
        }
    }
}

String fetchVersion()
{
    HTTPClient http;
    http.begin(versionUrl);
    int responseCode = http.GET();

    if (responseCode == HTTP_CODE_OK)
    {
        String latestVersion = http.getString();
        latestVersion.trim();
        http.end();
        return latestVersion;
    }
    else
    {
        Serial.printf("Failed to fetch version. HTTP code: %d\n", responseCode);
        http.end();
        return "";
    }
}

void downloadAndApplyFirmware()
{
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(firmwareUrl);
    int responseCode = http.GET();

    Serial.printf("HTTP GET code: %d\n", responseCode);

    if (responseCode == HTTP_CODE_OK)
    {

        int contentLength = http.getSize();
        Serial.printf("Firmware size: %d bytes\n", contentLength);

        if (contentLength > 0)
        {
            WiFiClient *stream = http.getStreamPtr();
            if (startOTAUpdate(stream, contentLength))
            {
                Serial.println("OTA update successful, restarting...");
                delay(2000);
                ESP.restart();
            }
            else
            {
                Serial.println("OTA update Failed");
            }
        }
        else
        {
            Serial.println("Invalid firmware size");
        }
    }
    else
    {
        Serial.printf("Failed to fetch firmware. HTTP code: %d\n", responseCode);
    }

    http.end();
}
bool startOTAUpdate(WiFiClient *client, int contentLength)
{
    Serial.println("Initializing update...");
    if (!Update.begin(contentLength))
    {
        Serial.printf("Update begin failed: %s\n", Update.errorString());
        return false;
    }
    Serial.println("Writing firmware....");
    size_t written = 0;
    int progress = 0;
    int lastProgress = 0;

    const unsigned long timeoutDuration = 120 * 1000; // 10s timeout
    unsigned long lastDataTime = millis();
    while (written < contentLength)
    {
        if (client->available())
        {
            uint8_t buffer[128];
            size_t len = client->read(buffer, sizeof(buffer));
            if (len > 0)
            {
                Update.write(buffer, len);
                written += len;
                progress = written * 100 / contentLength;
                if (progress != lastProgress)
                {
                    Serial.printf("Writing Progress: %d%%\n", progress);
                    lastProgress = progress;
                }
            }
        }
        if (millis() - lastDataTime > timeoutDuration)
        {
            Serial.println("Timeout: No data received for too long.Aborting update...");
            Update.abort();
            return false;
        }
        yield();
    }
    Serial.println("\nWriting complete");

    if (written != contentLength)
    {
        Serial.printf("Error: Write incomplete. Expected %d but got %d bytes\n", contentLength, written);
        Update.abort();
        return false;
    }

    if (!Update.end())
    {
        Serial.printf("Error: Update end failed: %s\n", Update.errorString());
        return false;
    }

    Serial.println("Update successfully completed");
    return true;
}
