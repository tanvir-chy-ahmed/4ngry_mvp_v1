#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "version.h"
#include "core/global.h"
#include "update/ota_update.h"
#include "update/ota_ui.h"
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
static String latestFirmwareAssetUrl;

const unsigned long updateCheckInterval =
    60UL * 60UL * 1000UL; // 1 hour

// ESP image header fields are defined by the ESP-IDF image format.
// chip_id is a little-endian uint16 at byte offset 12 in the 24-byte header.
static int expectedChipId()
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    return 0;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    return 2;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    return 5;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    return 9;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    return 13;
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    return 16;
#else
    return -1;
#endif
}

bool validateFirmwareImageHeader(const uint8_t *header, size_t length, String &reason)
{
    if (header == nullptr || length < 24)
    {
        reason = "Firmware header is incomplete (need 24 bytes).";
        return false;
    }
    if (header[0] != 0xE9)
    {
        reason = "Invalid ESP firmware image: magic byte is not 0xE9.";
        return false;
    }

    const int foundChipId = (int)header[12] | ((int)header[13] << 8);
    const int expected = expectedChipId();
    if (expected < 0)
    {
        reason = "This build target is not recognized; refusing OTA for safety.";
        return false;
    }
    if (foundChipId != expected)
    {
        reason = "Chip mismatch: running target expects image chip_id " + String(expected) +
                 ", uploaded image contains chip_id " + String(foundChipId) +
                 ". Build the .bin for the same board as this firmware.";
        return false;
    }

    reason = "OK";
    return true;
}

// ============================================================
// Fetch Latest GitHub Release Version
// ============================================================

String fetchLatestVersion()
{
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // HTTPS transport; see integration notes about certificate pinning.
    HTTPClient http;
    http.setTimeout(12000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(secureClient, latestReleaseUrl))
        return "";

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

    String tagName = doc["tag_name"].as<String>();
    latestFirmwareAssetUrl = "";
    String onlyCandidateUrl = "";
    String targetCandidateUrl = "";
    uint8_t appCandidateCount = 0;
    JsonArray assets = doc["assets"].as<JsonArray>();
    const int targetId = expectedChipId();
    for (JsonObject asset : assets)
    {
        String name = asset["name"].as<String>();
        String normalizedName = name;
        normalizedName.toLowerCase();
        String url = asset["browser_download_url"].as<String>();
        bool looksLikeApp = normalizedName.indexOf("4ngry") >= 0 || normalizedName.indexOf("firmware") >= 0;
        bool looksLikeBootArtifact = normalizedName.indexOf("bootloader") >= 0 || normalizedName.indexOf("partition") >= 0 || normalizedName.indexOf("merged") >= 0 || normalizedName.indexOf("boot_app0") >= 0;
        if (!normalizedName.endsWith(".bin") || !looksLikeApp || looksLikeBootArtifact || !url.startsWith("https://"))
            continue;

        ++appCandidateCount;
        onlyCandidateUrl = url;
        bool hasC3 = normalizedName.indexOf("esp32-c3") >= 0 || normalizedName.indexOf("esp32c3") >= 0 || normalizedName.indexOf("_c3") >= 0 || normalizedName.indexOf("-c3") >= 0;
        bool hasS2 = normalizedName.indexOf("esp32-s2") >= 0 || normalizedName.indexOf("esp32s2") >= 0 || normalizedName.indexOf("-s2") >= 0;
        bool hasS3 = normalizedName.indexOf("esp32-s3") >= 0 || normalizedName.indexOf("esp32s3") >= 0 || normalizedName.indexOf("-s3") >= 0;
        bool hasC6 = normalizedName.indexOf("esp32-c6") >= 0 || normalizedName.indexOf("esp32c6") >= 0 || normalizedName.indexOf("-c6") >= 0;
        bool hasH2 = normalizedName.indexOf("esp32-h2") >= 0 || normalizedName.indexOf("esp32h2") >= 0 || normalizedName.indexOf("-h2") >= 0;
        bool targetMatch = false;
        if (targetId == 0) targetMatch = !hasC3 && !hasS2 && !hasS3 && !hasC6 && !hasH2 && normalizedName.indexOf("esp32") >= 0;
        else if (targetId == 2) targetMatch = hasS2;
        else if (targetId == 5) targetMatch = hasC3;
        else if (targetId == 9) targetMatch = hasS3;
        else if (targetId == 13) targetMatch = hasC6;
        else if (targetId == 16) targetMatch = hasH2;
        if (targetMatch) targetCandidateUrl = url;
    }

    // Prefer a board-labelled release asset. If only one app .bin exists, allow it
    // and validate its image header before touching flash. Never guess between builds.
    if (targetCandidateUrl.length())
        latestFirmwareAssetUrl = targetCandidateUrl;
    else if (appCandidateCount == 1)
        latestFirmwareAssetUrl = onlyCandidateUrl;
    else if (appCandidateCount > 1)
        Serial.println("OTA: multiple .bin assets but none is labelled for this chip target; refusing to guess.");

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

static bool isVersionNewer(const String &candidate, const String &current)
{
    int ci = 0, oi = 0;
    while (ci < candidate.length() || oi < current.length())
    {
        long a = 0, b = 0;
        while (ci < candidate.length() && candidate[ci] != '.')
        {
            if (candidate[ci] >= '0' && candidate[ci] <= '9')
                a = a * 10 + candidate[ci] - '0';
            ci++;
        }
        while (oi < current.length() && current[oi] != '.')
        {
            if (current[oi] >= '0' && current[oi] <= '9')
                b = b * 10 + current[oi] - '0';
            oi++;
        }
        if (a != b)
            return a > b;
        if (ci < candidate.length())
            ci++;
        if (oi < current.length())
            oi++;
    }
    return false;
}

// ============================================================
// Check Firmware Update
// ============================================================

void checkfirmwareUpdate()
{
    // Called once from setup(); do not reinitialize Wi-Fi or disrupt AP/STA state.
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("OTA check skipped: WiFi not connected.");
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

    if (isVersionNewer(latestVersion, currentVersion) && latestFirmwareAssetUrl.length() > 0)
    {
        Serial.println("New firmware available!");
        otaUpdateScreen(0);

        Serial.println(
            "Starting OTA update...");

        downloadAndApplyFirmware(
            latestVersion);
    }
    else
    {
        if (isVersionNewer(latestVersion, currentVersion) && latestFirmwareAssetUrl.isEmpty())
            Serial.println("Update exists but release has no downloadable .bin asset; skipping safely.");
        else
            Serial.println("Device is up to date.");
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

    String firmwareUrl = latestFirmwareAssetUrl;
    if (firmwareUrl.isEmpty())
    {
        Serial.println("OTA aborted: GitHub release did not provide a .bin asset URL.");
        return;
    }

    Serial.println();
    Serial.println("Firmware URL:");
    Serial.println(firmwareUrl);

    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // HTTPS encrypted; certificate is not pinned in this build.
    HTTPClient http;
    http.setTimeout(20000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(secureClient, firmwareUrl))
    {
        Serial.println("OTA: could not initialize HTTPS request.");
        otaUpdateFailedScreen("HTTPS init failed");
        return;
    }

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
            Client *stream =
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
                Serial.println("OTA update failed; current firmware was not intentionally restarted.");
                otaUpdateFailedScreen("Wrong board or bad .bin");
            }
        }
        else
        {
            Serial.println("Invalid firmware size.");
            otaUpdateFailedScreen("Invalid image size");
        }
    }
    else
    {
        Serial.printf("Failed to fetch firmware. HTTP code: %d\n", httpCode);
        otaUpdateFailedScreen("GitHub download failed");
    }

    http.end();
}

// ============================================================
// OTA Flash Write
// ============================================================

bool startOTAUpdate(Client *client, int contentLength)
{
    if (client == nullptr || contentLength < 24)
    {
        Serial.println("[OTA] Refusing update: invalid client or image size.");
        return false;
    }

    Serial.printf("[OTA] Build target expects chip_id %d.\n", expectedChipId());
    Serial.println("[OTA] Reading and validating firmware image header before flash writes...");

    uint8_t imageHeader[24] = {};
    size_t headerRead = 0;
    unsigned long lastDataTime = millis();
    const unsigned long noDataTimeoutMs = 20000UL;

    while (headerRead < sizeof(imageHeader))
    {
        int available = client->available();
        if (available > 0)
        {
            size_t wanted = sizeof(imageHeader) - headerRead;
            if ((size_t)available < wanted) wanted = (size_t)available;
            int got = client->read(imageHeader + headerRead, wanted);
            if (got > 0)
            {
                headerRead += (size_t)got;
                lastDataTime = millis();
            }
        }
        else
        {
            if (!client->connected() && headerRead < sizeof(imageHeader))
            {
                Serial.println("[OTA] Download stream closed before image header arrived.");
                return false;
            }
            if (millis() - lastDataTime > noDataTimeoutMs)
            {
                Serial.println("[OTA] Timed out waiting for firmware header.");
                return false;
            }
            delay(1);
        }
        yield();
    }

    String headerError;
    if (!validateFirmwareImageHeader(imageHeader, sizeof(imageHeader), headerError))
    {
        Serial.printf("[OTA] Image rejected before flash write: %s\n", headerError.c_str());
        return false;
    }

    Serial.println("[OTA] Image header matches this chip.");
    if (!Update.begin((size_t)contentLength))
    {
        Serial.printf("[OTA] Update.begin failed: %s\n", Update.errorString());
        return false;
    }

    size_t written = Update.write(imageHeader, sizeof(imageHeader));
    if (written != sizeof(imageHeader))
    {
        Serial.printf("[OTA] Could not write image header (%u/%u): %s\n",
                      (unsigned)written, (unsigned)sizeof(imageHeader), Update.errorString());
        Update.abort();
        return false;
    }

    uint8_t buffer[1024];
    int lastProgress = -1;
    while (written < (size_t)contentLength)
    {
        int available = client->available();
        if (available > 0)
        {
            size_t wanted = sizeof(buffer);
            size_t remaining = (size_t)contentLength - written;
            if (wanted > remaining) wanted = remaining;
            if ((size_t)available < wanted) wanted = (size_t)available;

            int got = client->read(buffer, wanted);
            if (got > 0)
            {
                size_t wrote = Update.write(buffer, (size_t)got);
                if (wrote != (size_t)got)
                {
                    Serial.printf("[OTA] Flash write failed: %s\n", Update.errorString());
                    Update.abort();
                    return false;
                }
                written += wrote;
                lastDataTime = millis();
                int progress = (int)((written * 100ULL) / (size_t)contentLength);
                if (progress != lastProgress)
                {
                    Serial.printf("Writing Progress: %d%%\n", progress);
                    otaUpdateScreen(progress);
                    lastProgress = progress;
                }
            }
        }
        else
        {
            if (!client->connected() && written < (size_t)contentLength)
            {
                Serial.printf("[OTA] Download stream closed early (%u/%u bytes).\n",
                              (unsigned)written, (unsigned)contentLength);
                Update.abort();
                return false;
            }
            if (millis() - lastDataTime > noDataTimeoutMs)
            {
                Serial.println("[OTA] Timed out while downloading firmware.");
                Update.abort();
                return false;
            }
            delay(1);
        }
        yield();
    }

    if (written != (size_t)contentLength)
    {
        Serial.printf("[OTA] Size mismatch: expected %d, wrote %u.\n",
                      contentLength, (unsigned)written);
        Update.abort();
        return false;
    }

    Serial.println("[OTA] Download complete; validating image and finalizing OTA...");
    if (!Update.end())
    {
        Serial.printf("[OTA] Update.end failed: %s\n", Update.errorString());
        Update.abort();
        return false;
    }
    if (!Update.isFinished())
    {
        Serial.println("[OTA] Update library reports the image is not finished.");
        return false;
    }

    Serial.println("[OTA] Image validated and staged successfully.");
    return true;
}
