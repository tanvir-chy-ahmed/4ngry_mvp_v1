#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <time.h>
#define OLED_SDA 8
#define OLED_SCL 9
#define TOUCH_PIN 4
#define BUZZER_PIN 3
#define OLED_I2C_ADDR 0x3C
#define FW_VERSION "1.0.0"
// Default location used only until the owner saves a different location in setup.
static const char OPENWEATHER_API_KEY[] = "554eb7387cc38ab998746cb3522e1cd3";
static const char DEFAULT_CITY[] = "Sylhet";
static const char DEFAULT_COUNTRY[] = "BD";
static const char DEFAULT_TZ[] = "Asia/Dhaka"; // Human-friendly name; converted to Bangladesh's POSIX UTC+6 rule below.
static const float DEFAULT_LAT = 24.8949f;
static const float DEFAULT_LON = 91.8687f;
enum State
{
    BOOT,
    SETUP,
    PORTAL,
    PET,
    WATCH,
    TIMER_PICK,
    TIMER_RUN,
    ALERT,
    WEATHER,
    FORECAST,
    RESET,
    WEB_CONFIRM
};
enum Mood
{
    NEUTRAL,
    HAPPY,
    LAUGH,
    SAD,
    CRY,
    ANGRY,
    SLEEPY,
    SURPRISED,
    CURIOUS,
    LOVE
};
struct Config
{
    char ssid[33], pass[65], tz[48];
    float lat, lon;
    bool weather;
    uint32_t minutes;
};
struct Wx
{
    bool ok;
    float t, h, w, lo[3], hi[3];
    int code, dc[3];
};
U8G2_SSD1306_128X64_NONAME_F_HW_I2C d(U8G2_R0, U8X8_PIN_NONE);
WebServer web(80);
DNSServer dns;
IPAddress apIp(192, 168, 4, 1), apGateway(192, 168, 4, 1), apMask(255, 255, 255, 0);
Preferences nv;
Config c;
Wx wx = {};
State st = BOOT;
Mood mood = NEUTRAL;
uint32_t since, lastFrame, pressAt, changed, lastWeather, lastWxAttempt, lastWiFiAttempt, lastTimeAttempt, networkStart, deadline, rebootAt, wifiBeginAt, wifiAttemptAt, lastApStartAttempt;
bool raw, stable, done2, done5, portal, portalRequested, webStarted, wifiBeginPending, stationConnecting;
int pick;
const uint32_t presets[] = {60000, 300000, 600000, 900000, 1800000, 2700000, 3600000};
const int PRESET_COUNT = sizeof(presets) / sizeof(presets[0]);
static const uint32_t NETWORK_RETRY_WINDOW_MS = 300000UL;
static const uint32_t WIFI_RETRY_MS = 10000UL;
static const uint32_t TIME_RETRY_MS = 15000UL;
static const uint32_t WEATHER_RETRY_MS = 15000UL;
static const uint32_t MAIN_NAV_HOLD_MS = 700UL;
// Holding the touch pad for five uninterrupted seconds opens the local setup hotspot.
// Once opened, it stays available until the device is restarted after saving settings.
static const uint32_t WEB_ACCESS_HOLD_MS = 5000UL;
const char *WEB_USER = "admin";
const char *WEB_PASS = "4ngrydesk";
String otaStatus = "No update has been started.";
bool otaUploadOk = false;
// A stored network is usable only when both fields are present. This deliberately
// treats a missing password as not configured, as requested.
bool hasSavedWiFi() { return c.ssid[0] && c.pass[0]; }
void cp(char *x, size_t n, const String &s)
{
    strncpy(x, s.c_str(), n - 1);
    x[n - 1] = 0;
}
// ESP32's time library needs a POSIX rule, not an IANA region name.
// Bangladesh is UTC+6 all year and has no daylight-saving change.
const char *tzRule() { return strcmp(c.tz, "Asia/Dhaka") == 0 ? "BST-6" : c.tz; }
void load()
{
    memset(&c, 0, sizeof c);
    nv.begin("4ngry", true);
    cp(c.ssid, sizeof c.ssid, nv.getString("ssid", ""));
    cp(c.pass, sizeof c.pass, nv.getString("pass", ""));
    cp(c.tz, sizeof c.tz, nv.getString("tz", DEFAULT_TZ));
    c.lat = nv.getFloat("lat", DEFAULT_LAT);
    c.lon = nv.getFloat("lon", DEFAULT_LON);
    c.weather = nv.getBool("wx", true);
    c.minutes = nv.getUInt("min", 30);
    nv.end();
}
void save()
{
    nv.begin("4ngry", false);
    nv.putString("ssid", c.ssid);
    nv.putString("pass", c.pass);
    nv.putString("tz", c.tz);
    nv.putFloat("lat", c.lat);
    nv.putFloat("lon", c.lon);
    nv.putBool("wx", c.weather);
    nv.putUInt("min", c.minutes);
    nv.end();
}
void go(State x)
{
    st = x;
    since = millis();
}
void icon(int x, int y, int q)
{
    if (q <= 1)
    {
        d.drawCircle(x + 8, y + 8, 5);
        for (int i = 0; i < 4; i++)
        {
            d.drawLine(x + 8 + i * 3, y, x + 8 + i * 3, y + 3);
        }
    }
    else
    {
        d.drawDisc(x + 7, y + 8, 5);
        d.drawDisc(x + 13, y + 6, 6);
        d.drawBox(x + 3, y + 8, 17, 6);
    }
}
void pet()
{
    uint32_t t = millis();
    int y = 31 + (int)sin(t / 300.0);
    int l = 39, r = 89;
    if (mood == HAPPY)
    {
        d.drawLine(l - 12, y, l, y + 7);
        d.drawLine(l, y + 7, l + 12, y);
        d.drawLine(r - 12, y, r, y + 7);
        d.drawLine(r, y + 7, r + 12, y);
    }
    else if (mood == LAUGH || mood == SLEEPY)
    {
        d.drawBox(l - 12, y, 24, 2);
        d.drawBox(r - 12, y, 24, 2);
    }
    else if (mood == ANGRY)
    {
        d.drawBox(l - 11, y - 4, 22, 8);
        d.drawBox(r - 11, y - 4, 22, 8);
        d.drawLine(l - 14, y - 14, l + 10, y - 8);
        d.drawLine(r - 10, y - 8, r + 14, y - 14);
    }
    else if (mood == SURPRISED)
    {
        d.drawDisc(l, y, 12);
        d.drawDisc(r, y, 12);
    }
    else if (mood == CURIOUS)
    {
        d.drawDisc(l, y, 8);
        d.drawDisc(r + 4, y - 4, 12);
    }
    else if (mood == LOVE)
    {
        d.drawDisc(l - 5, y - 2, 5);
        d.drawDisc(l + 5, y - 2, 5);
        d.drawTriangle(l - 10, y, l + 10, y, l, y + 12);
        d.drawDisc(r - 5, y - 2, 5);
        d.drawDisc(r + 5, y - 2, 5);
        d.drawTriangle(r - 10, y, r + 10, y, r, y + 12);
    }
    else
    {
        d.drawDisc(l, y, 10);
        d.drawDisc(r, y, 10);
        if (mood == CRY)
        {
            int z = (t / 35) % 16;
            d.drawDisc(l, y + 14 + z, 2);
            d.drawDisc(r, y + 14 + z, 2);
        }
        if (mood == SAD)
        {
            d.drawLine(l - 12, y - 13, l + 9, y - 17);
            d.drawLine(r - 9, y - 17, r + 12, y - 13);
        }
    }
}
void playBootAnimation()
{
    const int cx = 64;
    const int cy = 32;

    auto drawMTRLogo = []()
    {
        d.setDrawColor(1);
        d.setFont(u8g2_font_helvB24_tf);

        const char *logo = "MTR";
        int logoWidth = d.getStrWidth(logo);
        int logoX = (128 - logoWidth) / 2;
        d.drawStr(logoX, 34, logo);

        int lineWidth = logoWidth + 6;
        d.drawHLine((128 - lineWidth) / 2, 39, lineWidth);

        d.setFont(u8g2_font_5x8_tf);
        const char *subtitle = "MR THORIN RAPTOR";
        int subWidth = d.getStrWidth(subtitle);
        d.drawStr((128 - subWidth) / 2, 48, subtitle);

        d.setFont(u8g2_font_6x10_tf);
        const char *signature = "4ngry";
        int sigWidth = d.getStrWidth(signature);
        d.drawStr((128 - sigWidth) / 2, 61, signature);
    };

    for (int r = 0; r <= 80; r += 4)
    {
        d.clearBuffer();
        d.setDrawColor(1);
        d.drawDisc(cx, cy, r);
        d.sendBuffer();
        delay(10);
    }

    for (int r = 0; r <= 80; r += 4)
    {
        d.clearBuffer();
        d.setDrawColor(1);
        d.drawDisc(cx, cy, 80);
        d.setDrawColor(0);
        d.drawDisc(cx, cy, r);
        d.sendBuffer();
        delay(10);
    }

    for (int width = 0; width <= 64; width += 8)
    {
        d.clearBuffer();
        d.setDrawColor(1);
        d.drawHLine(0, 15, width);
        d.drawHLine(128 - width, 48, width);
        if (width >= 24)
            drawMTRLogo();
        d.sendBuffer();
        delay(35);
    }

    d.clearBuffer();
    d.setDrawColor(1);
    d.drawBox(0, 0, 128, 64);
    d.setDrawColor(0);
    d.setFont(u8g2_font_helvB24_tf);
    const char *logo = "MTR";
    int logoWidth = d.getStrWidth(logo);
    d.drawStr((128 - logoWidth) / 2, 36, logo);
    d.sendBuffer();
    delay(100);

    d.clearBuffer();
    drawMTRLogo();
    d.sendBuffer();
    delay(2200);
}

// Implemented in web_features.h, declared here because draw() runs before that header is included.
static void drawWebConfirm();
static bool isFeatureDisplayActive();
static void drawFeatureDisplay();
void draw()
{
    if (millis() - lastFrame < 50)
        return;
    lastFrame = millis();
    if (isFeatureDisplayActive())
    {
        drawFeatureDisplay();
        return;
    }
    d.clearBuffer();
    d.setFont(u8g2_font_6x12_tf);
    if (st == BOOT)
    {
        d.setFont(u8g2_font_7x14B_tf);
        d.drawStr(43, 20, "MTR");
        d.drawStr(38, 39, "4ngry");
        d.drawFrame(16, 54, 96, 6);
        d.drawBox(18, 56, min(92, (int)((millis() - since) / 15)), 2);
    }
    else if (st == SETUP)
    {
        d.drawStr(27, 15, c.ssid[0] ? "CONNECTING WIFI" : "SETUP MODE");
        d.drawStr(6, 33, "Join: 4ngry-Setup");
        d.drawStr(6, 48, "Open: 192.168.4.1");
        d.setFont(u8g2_font_5x8_tf);
        d.drawStr(6, 61, c.ssid[0] ? "Trying saved network..." : "Choose Wi-Fi in the portal");
    }
    else if (st == WEB_CONFIRM)
    {
        drawWebConfirm();
        return;
    }
    else if (st == PORTAL)
    {
        if (portal)
        {
            d.setFont(u8g2_font_7x14B_tf);
            d.drawStr(17, 16, "WEB PAGE READY");
            d.setFont(u8g2_font_6x12_tf);
            d.drawStr(8, 35, "Join: 4ngry-Setup");
            d.drawStr(8, 50, "Open: 192.168.4.1");
            d.setFont(u8g2_font_5x8_tf);
            d.drawStr(20, 62, "Tap to return to face");
        }
        else
        {
            d.setFont(u8g2_font_6x12_tf);
            d.drawStr(19, 22, "SETUP WIFI FAILED");
            d.setFont(u8g2_font_5x8_tf);
            d.drawStr(9, 40, "Keep holding to retry");
            d.drawStr(13, 55, "or tap back to face");
        }
    }
    else if (st == PET || st == RESET)
    {
        pet();
        if (st == RESET)
        {
            d.setFont(u8g2_font_5x8_tf);
            d.drawStr(22, 8, "HOLD TO RESET");
            d.drawFrame(14, 55, 100, 6);
            d.drawBox(16, 57, min(96, (int)((millis() - pressAt) * 96 / 5000)), 2);
        }
    }
    else if (st == WATCH)
    {
        struct tm tm;
        if (getLocalTime(&tm, 0))
        {
            char b[16];
            d.setFont(u8g2_font_logisoso24_tf);
            strftime(b, sizeof b, "%H:%M", &tm);
            d.drawStr(3, 34, b);
            d.setFont(u8g2_font_6x12_tf);
            strftime(b, sizeof b, "%a %d %b", &tm);
            d.drawStr(24, 54, b);
        }
        else
            d.drawStr(24, 32, "CLOCK NOT SET");
    }
    else if (st == TIMER_PICK)
    {
        char b[14];
        d.drawStr(25, 14, "SELECT TIMER");
        sprintf(b, "%lu min", presets[pick] / 60000);
        d.setFont(u8g2_font_logisoso20_tf);
        d.drawStr(27, 43, b);
        d.setFont(u8g2_font_5x8_tf);
        d.drawStr(8, 60, "tap next; hold start");
    }
    else if (st == TIMER_RUN)
    {
        uint32_t x = (deadline - millis()) / 1000;
        char b[10];
        sprintf(b, "%02lu:%02lu", x / 60, x % 60);
        d.setFont(u8g2_font_logisoso24_tf);
        d.drawStr(18, 42, b);
    }
    else if (st == ALERT)
    {
        d.setFont(u8g2_font_7x14B_tf);
        d.drawStr(20, 31, "TIME IS UP");
    }
    else if (!wx.ok)
        d.drawStr(14, 32, "WEATHER UNAVAILABLE");
    else if (st == WEATHER)
    {
        char b[22];
        sprintf(b, "%.1f C  %d%%", wx.t, (int)wx.h);
        d.drawStr(3, 15, b);
        sprintf(b, "Wind %.1f km/h", wx.w);
        d.drawStr(3, 34, b);
        icon(98, 5, wx.code);
    }
    else
        for (int i = 0; i < 3; i++)
        {
            char b[12];
            sprintf(b, "%d/%d", (int)wx.lo[i], (int)wx.hi[i]);
            d.drawStr(i * 42 + 2, 51, b);
            icon(i * 42 + 8, 15, wx.dc[i]);
        }
    d.sendBuffer();
}
#include "web_features.h"

String page() { return F(R"HTML(<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'><style>body{margin:0;background:linear-gradient(135deg,#17134b,#008a9a);font:16px system-ui;color:#f7fbff}.card{max-width:520px;margin:20px auto;padding:22px;background:#ffffff12;border:1px solid #ffffff38;border-radius:20px;box-shadow:0 12px 35px #0005}h1{margin:0;color:#67f6df}h2{font-size:18px;margin-top:26px}label{display:block;margin-top:12px;font-weight:600}input,select,button{box-sizing:border-box;width:100%;margin-top:5px;padding:12px;border:0;border-radius:10px;font:inherit}input,select{background:#fff;color:#15213a}button{background:#67f6df;color:#052e35;font-weight:800;cursor:pointer}.secondary{background:#ffffff26;color:#fff}.row{display:flex;gap:8px}.row>*{flex:1}.net{width:100%;text-align:left;margin-top:7px;background:#fff;color:#15213a}.hint{font-size:13px;color:#d5ebff}.status{min-height:22px;color:#ffe17a}.danger{background:#ffb1b1;color:#560000}</style></head><body><main class=card><h1>MTR - 4ngry</h1><p>Connect your robot, set its clock, and update its firmware.</p><h2>Wi-Fi</h2><button class=secondary onclick=scan()>Find nearby Wi-Fi</button><div id=nets class=hint>Tap "Find nearby Wi-Fi".</div><form id=cfg><label>Selected network<input required name=ssid id=ssid maxlength=32 autocomplete=off></label><label>Wi-Fi password<input required name=pass type=password maxlength=63 autocomplete=new-password placeholder='Enter the password for this Wi-Fi'></label><label>Time zone<input name=tz value='Asia/Dhaka' maxlength=47 placeholder='Asia/Dhaka'></label><div class=row><label>Latitude<input name=lat value='24.8949' inputmode=decimal></label><label>Longitude<input name=lon value='91.8687' inputmode=decimal></label></div><p class=hint>Weather defaults to Sylhet, Bangladesh in Celsius. Change the coordinates if you live elsewhere.</p><button>Save settings and restart</button></form><p id=msg class=status></p><h2>Firmware update</h2><p class=hint>Upload only a firmware .bin file that you built and trust. This page asks for the device administrator login.</p><form action=/update method=post enctype=multipart/form-data><input required name=firmware type=file accept=.bin,application/octet-stream><button>Install firmware update</button></form><p class=hint>Saved Wi-Fi and weather settings stay in place after a normal firmware update.</p></main><script>const q=s=>document.querySelector(s);async function scan(){q('#nets').textContent='Searching...';let r=await fetch('/scan');let j=await r.json();if(j.state==='starting'){setTimeout(scan,900);return}q('#nets').innerHTML=j.networks.map(n=>'<button type=button class=net data-s="'+n.s+'">'+n.s+' - '+n.r+' dBm '+(n.sec?'secured':'open')+'</button>').join('')||'No networks found.';document.querySelectorAll('.net').forEach(b=>b.onclick=()=>{q('#ssid').value=b.dataset.s;q('#msg').textContent='Network selected. Enter its password below.'})}q('#cfg').onsubmit=async e=>{e.preventDefault();q('#msg').textContent='Saving…';let r=await fetch('/save',{method:'POST',body:new FormData(e.target)});q('#msg').textContent=await r.text()};</script></body></html>)HTML"); }
void startWeb()
{
    if (webStarted)
        return;
    web.on("/scan", HTTP_GET, []
           {int n=WiFi.scanComplete();if(n==-2){WiFi.scanNetworks(true,true);web.send(200,"application/json","{\"state\":\"starting\"}");return;}if(n==-1){web.send(200,"application/json","{\"state\":\"starting\"}");return;}String out="{\"state\":\"done\",\"networks\":[";for(int i=0;i<n;i++){if(i)out+=',';String s=WiFi.SSID(i);s.replace("\\","\\\\");s.replace("\"","\\\"");out+="{\"s\":\""+s+"\",\"r\":"+String(WiFi.RSSI(i))+",\"sec\":"+(WiFi.encryptionType(i)==WIFI_AUTH_OPEN?"false":"true")+"}";}out+="]}";WiFi.scanDelete();web.send(200,"application/json",out); });
    web.on("/save", HTTP_POST, []
           {if(!web.hasArg("ssid")||web.arg("ssid").length()<1||web.arg("ssid").length()>32||web.arg("pass").length()<1||web.arg("pass").length()>63||web.arg("tz").length()>47){web.send(400,"text/plain","Enter both the Wi-Fi name and password, then check the time zone.");return;}cp(c.ssid,sizeof c.ssid,web.arg("ssid"));if(web.arg("pass").length())cp(c.pass,sizeof c.pass,web.arg("pass"));cp(c.tz,sizeof c.tz,web.arg("tz"));c.lat=web.arg("lat").toFloat();c.lon=web.arg("lon").toFloat();save();web.send(200,"text/plain","Saved. 4ngry is restarting now and will quietly retry your Wi-Fi, clock, and weather in the background.");rebootAt=millis()+1200UL; });
    web.on("/update", HTTP_POST, []
           {
  if(!web.authenticate(WEB_USER,WEB_PASS)){web.requestAuthentication();return;}
  if(!otaUploadOk || Update.hasError()){web.send(400,"text/plain",String("Update was not installed: ")+otaStatus+" The existing firmware is still running.");return;}
  web.send(200,"text/plain","Update verified and installed. Restarting now.");
  rebootAt=millis()+1200; }, []
           {
  if(!web.authenticate(WEB_USER,WEB_PASS))return;
  HTTPUpload& u=web.upload();
  if(u.status==UPLOAD_FILE_START){
    otaUploadOk=false; otaStatus="";
    if(!u.filename.endsWith(".bin")){otaStatus="choose an application firmware.bin file";return;}
    if(!Update.begin(UPDATE_SIZE_UNKNOWN)){otaStatus=String("the update partition is unavailable or the file is too large (error ")+String(Update.getError())+")";Update.printError(Serial);return;}
    otaStatus="receiving firmware";
  }else if(u.status==UPLOAD_FILE_WRITE){
    if(otaStatus=="receiving firmware" && Update.write(u.buf,u.currentSize)!=u.currentSize){otaStatus=String("the firmware data was rejected (error ")+String(Update.getError())+")";Update.abort();Update.printError(Serial);}
  }else if(u.status==UPLOAD_FILE_END){
    if(otaStatus!="receiving firmware")return;
    if(!Update.end(true)){otaStatus=String("the file is not a complete ESP32 application image (error ")+String(Update.getError())+")";Update.printError(Serial);return;}
    if(!Update.isFinished()){otaStatus="the upload ended before the whole firmware file arrived";return;}
    otaUploadOk=true;otaStatus="firmware verified";
  }else if(u.status==UPLOAD_FILE_ABORTED){
    Update.abort();otaStatus="the browser cancelled the upload";
  } });
    // Common Android, iPhone, Windows, and browser connectivity checks. Redirect
    // them to the local portal instead of leaving the phone at a blank test page.
    web.on("/generate_204", HTTP_ANY, []
           {web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain",""); });
    web.on("/hotspot-detect.html", HTTP_ANY, []
           {web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain",""); });
    web.on("/connecttest.txt", HTTP_ANY, []
           {web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain",""); });
    web.on("/ncsi.txt", HTTP_ANY, []
           {web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain",""); });
    // Register the primary page before the captive-portal fallback. This route
    // must exist exactly once, even when portalStart() retries AP startup.
    web.on("/", HTTP_GET, []
           { web.send(200, "text/html", featurePage()); });
    featureRoutes();
    web.onNotFound([]
                   {web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain",""); });
    web.begin();
    webStarted = true;
    Serial.println("Web server started on port 80");
}
// Start the setup network from a settled AP-only radio state. In particular,
// do not call softAPConfig() while an earlier station connection is still in
// progress: ESP32-C3 then reports "sta is connecting, cannot set config" and
// phones can see the SSID but fail to join it. Saved Wi-Fi resumes later from
// serviceSavedWiFi(), after this AP has been verified.

bool startSetupAp()
{
    // Stop pending station connection attempts before starting the AP.
    wifiBeginPending = false;
    stationConnecting = false;

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);

    // Clear any previous AP session.
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_MODE_NULL);
    delay(300);

    // Start with a clean, 2.4 GHz access-point-only radio state.
    if (!WiFi.mode(WIFI_AP))
    {
        Serial.println("[AP] Failed to set WIFI_AP mode");
        portal = false;
        return false;
    }

    delay(200);

    // Configure the setup network before advertising its SSID.
    bool configOK = WiFi.softAPConfig(
        apIp,
        apGateway,
        apMask);

    if (!configOK)
    {
        Serial.println("[AP] softAPConfig failed");
        portal = false;
        return false;
    }

    // Use channel 1 for broad 2.4 GHz compatibility.
    // Password must be at least 8 characters.
    bool apOK = WiFi.softAP(
        "4ngry-Setup",
        "4ngrydesk",
        1,     // Wi-Fi channel
        false, // Do not hide SSID
        4      // Maximum connected clients
    );

    delay(300);

    IPAddress actualIP = WiFi.softAPIP();

    portal = apOK && (actualIP == apIp);

    Serial.println();
    Serial.println("========== 4NGRY AP DEBUG ==========");
    Serial.printf("AP start: %s\n", apOK ? "OK" : "FAILED");
    Serial.printf("AP IP: %s\n", actualIP.toString().c_str());
    Serial.printf("WiFi mode: %d\n", (int)WiFi.getMode());
    Serial.printf("AP clients: %u\n", WiFi.softAPgetStationNum());
    Serial.printf("Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("SSID: 4ngry-Setup");
    Serial.println("Password: 4ngrydesk");
    Serial.println("====================================");

    if (!portal)
    {
        Serial.println("[AP] Startup verification failed");
        return false;
    }

    return true;
}

void portalStart()
{
    portalRequested = true;
    portalStopAt = 0;

    // Avoid repeatedly restarting a working access point.
    if (portal)
    {
        return;
    }

    lastApStartAttempt = millis();

    if (!startSetupAp())
    {
        Serial.println("[AP] Startup failed; retrying in 3 seconds");
        return;
    }

    // Start DNS and HTTP only after the AP is ready.
    dns.stop();

    bool dnsOK = dns.start(53, "*", apIp);

    Serial.printf(
        "[AP] DNS server: %s\n",
        dnsOK ? "OK" : "FAILED");

    startWeb();

    Serial.printf(
        "[AP] READY: http://%s/\n",
        WiFi.softAPIP().toString().c_str());

    if (st == SETUP || st == WEB_CONFIRM)
    {
        go(PORTAL);
    }
}

void beginSavedWiFi()
{
    if (!hasSavedWiFi() || wifiBeginPending || stationConnecting || WiFi.status() == WL_CONNECTED || (lastWiFiAttempt && millis() - lastWiFiAttempt < WIFI_RETRY_MS))
        return;
    wifiBeginPending = true;
    wifiBeginAt = millis();
    lastWiFiAttempt = millis();
}
void serviceSavedWiFi()
{
    if (!wifiBeginPending || millis() < wifiBeginAt)
        return;
    // The verified setup AP remains running while the saved network is tried.
    // Changing mode only happens after AP startup has completed.
    WiFi.mode(portal ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(c.ssid, c.pass);
    wifiBeginPending = false;
    stationConnecting = true;
    wifiAttemptAt = millis();
    Serial.printf("Station attempt started for saved SSID: %s\n", c.ssid);
}
void maintainNetwork()
{
    serviceSavedWiFi();
    if (!hasSavedWiFi() || millis() - networkStart > NETWORK_RETRY_WINDOW_MS)
        return;
    if (WiFi.status() != WL_CONNECTED)
    {
        // Do not call WiFi.begin() again while the previous attempt is in flight.
        if (stationConnecting && millis() - wifiAttemptAt >= 15000UL)
        {
            stationConnecting = false;
            Serial.printf("Station attempt ended with status %d; retrying later\n", (int)WiFi.status());
        }
        if (!stationConnecting && (!lastWiFiAttempt || millis() - lastWiFiAttempt >= WIFI_RETRY_MS))
            beginSavedWiFi();
        // A saved network may fail quietly, but provide recovery after 15 seconds.
        if (!portal && millis() - networkStart >= 15000UL)
            portalStart();
        return;
    }
    if (stationConnecting)
        Serial.printf("Station connected: %s\n", WiFi.localIP().toString().c_str());
    stationConnecting = false;
    // Keep the AP and DNS alive after station success; only Save and restart may
    // intentionally close the temporary portal.
    struct tm tm;
    if (!getLocalTime(&tm, 0) && (!lastTimeAttempt || millis() - lastTimeAttempt >= TIME_RETRY_MS))
    {
        configTzTime(tzRule(), "pool.ntp.org", "time.nist.gov");
        lastTimeAttempt = millis();
        Serial.println("Retrying clock sync");
    }
}
bool getJson(WiFiClientSecure &s, const String &u, DynamicJsonDocument &j)
{
    HTTPClient h;
    h.setTimeout(3500);
    bool ok = false;
    if (h.begin(s, u))
    {
        int status = h.GET();
        if (status == HTTP_CODE_OK && !deserializeJson(j, h.getStream()))
            ok = true;
        h.end();
    }
    return ok;
}
void weather()
{
    if (!c.weather || WiFi.status() != WL_CONNECTED)
        return;
    // Until the first valid report arrives, retry every 15 seconds for the same
    // three-minute connection window used by Wi-Fi and the clock. A successful
    // report then returns to the user's normal refresh interval.
    if (!wx.ok && millis() - networkStart > NETWORK_RETRY_WINDOW_MS)
        return;
    uint32_t interval = wx.ok ? c.minutes * 60000UL : WEATHER_RETRY_MS;
    if (lastWxAttempt && millis() - lastWxAttempt < interval)
        return;
    lastWxAttempt = millis();
    WiFiClientSecure s;
    s.setInsecure();
    String place = String(DEFAULT_CITY) + "," + DEFAULT_COUNTRY;
    String base = "https://api.openweathermap.org/data/2.5/";
    String query;
    if (fabs(c.lat - DEFAULT_LAT) < 0.0001f && fabs(c.lon - DEFAULT_LON) < 0.0001f)
        query = "?q=" + place;
    else
        query = "?lat=" + String(c.lat, 4) + "&lon=" + String(c.lon, 4);
    query += "&units=metric&lang=en&appid=" + String(OPENWEATHER_API_KEY);
    DynamicJsonDocument now(2048);
    if (!getJson(s, base + "weather" + query, now))
        return;
    float t = now["main"]["temp"] | NAN, h = now["main"]["humidity"] | NAN, w = now["wind"]["speed"] | NAN;
    if (isnan(t) || isnan(h) || isnan(w))
        return;
    Wx next = wx;
    next.t = t;
    next.h = h;
    next.w = w * 3.6f;
    next.code = now["weather"][0]["id"] | 0;
    DynamicJsonDocument fc(8192);
    if (getJson(s, base + "forecast" + query, fc))
    {
        JsonArray list = fc["list"].as<JsonArray>();
        for (int day = 0; day < 3; day++)
        {
            float lo = 1000, hi = -1000;
            int code = 0;
            for (int slot = 0; slot < 8; slot++)
            {
                JsonObject item = list[day * 8 + slot];
                if (item.isNull())
                    break;
                float x = item["main"]["temp"] | NAN;
                if (!isnan(x))
                {
                    if (x < lo)
                        lo = x;
                    if (x > hi)
                        hi = x;
                }
                if (!code)
                    code = item["weather"][0]["id"] | 0;
            }
            if (lo < 1000)
            {
                next.lo[day] = lo;
                next.hi[day] = hi;
                next.dc[day] = code;
            }
        }
    }
    next.ok = true;
    wx = next;
    lastWeather = millis();
    Serial.println("Weather updated");
}
// Main screen order is always: face/expressions -> clock -> weather -> face.
// A quick tap on the face changes only its expression; it never changes screen.
void nextMainScreen()
{
    if (st == PET)
        go(WATCH);
    else if (st == WATCH)
        go(WEATHER);
    else
        go(PET);
}
void touch()
{
    bool v = digitalRead(TOUCH_PIN);
    if (v != raw)
    {
        raw = v;
        changed = millis();
    }
    if (millis() - changed < 30 || v == stable)
        return;
    stable = v;
    if (v)
    {
        pressAt = millis();
        done2 = done5 = false;
        return;
    }
    if (st == WEB_CONFIRM)
    {
        go(PET);
        return;
    }
    if (isFeatureDisplayActive())
    {
        clearFeatureDisplay();
        go(PET);
        return;
    }
    if (done2 || done5)
        return;
    if (st == PORTAL)
    {
        go(PET);
    }
    else if (st == PET)
        mood = (Mood)((mood + 1) % 10);
    else if (st == WATCH)
        go(TIMER_PICK);
    else if (st == TIMER_PICK)
        pick = (pick + 1) % PRESET_COUNT;
    else if (st == TIMER_RUN || st == ALERT)
        go(WATCH);
}
void gestures()
{
    if (!stable)
        return;
    uint32_t h = millis() - pressAt;
    // A single deliberate five-second hold starts the local setup hotspot directly.
    // portalStart() leaves it running, so the web page remains available afterwards.
    if ((st == PET || st == WATCH || st == WEATHER) && h >= WEB_ACCESS_HOLD_MS && !done5)
    {
        done5 = true;
        portalStart();
        go(PORTAL);
        return;
    }
    if ((st == PET || st == WATCH || st == WEATHER) && h > MAIN_NAV_HOLD_MS && !done2)
    {
        done2 = true;
        nextMainScreen();
    }
    else if (st == TIMER_PICK && h > MAIN_NAV_HOLD_MS && !done2)
    {
        done2 = true;
        deadline = millis() + presets[pick];
        go(TIMER_RUN);
    }
}
void setup()
{
    Serial.begin(115200);
    WiFi.persistent(false);
    pinMode(TOUCH_PIN, INPUT);
    ledcAttach(BUZZER_PIN, 2000, 8);
    Wire.begin(OLED_SDA, OLED_SCL);
    d.setI2CAddress(OLED_I2C_ADDR << 1);
    d.begin();
    playBootAnimation();
    load();
    nv.begin("4ngry", true);
    alarmSong = nv.getUChar("song", 0);
    oledBrightness = nv.getUChar("bright", 180);
    nv.end();
    if (alarmSong > 11)
        alarmSong = 0;
    d.setContrast(oledBrightness);
    networkStart = millis();
    if (hasSavedWiFi())
    {
        beginSavedWiFi();
        go(PET);
    }
    else
    {
        portalStart();
        go(PORTAL);
    }
}
void loop()
{
    if (portal)
        dns.processNextRequest();
    if (webStarted)
        web.handleClient();
    if (rebootAt && millis() >= rebootAt)
        ESP.restart();
    maintainNetwork();
    if (portalRequested && !portal && millis() - lastApStartAttempt >= 3000UL)
        portalStart();
    if (st == SETUP && WiFi.status() == WL_CONNECTED)
        go(PET);
    touch();
    gestures();
    if (st == RESET && !stable)
        go(PET);
    if (st == TIMER_RUN && millis() >= deadline)
    {
        go(ALERT);
        startAlarmSong();
    }
    if (st != ALERT)
    {
        melodyPlaying = false;
        ledcWriteTone(BUZZER_PIN, 0);
    }
    updateAlarmSong();
    weather();
    draw();
}

// #include <Arduino.h>
// #include <Wire.h>
// #include <WiFi.h>
// #include <WebServer.h>
// #include <DNSServer.h>
// #include <HTTPClient.h>
// #include <WiFiClientSecure.h>
// #include <Preferences.h>
// #include <Update.h>
// #include <time.h>
// #include <ArduinoJson.h>
// #include <U8g2lib.h>
// #include "config.h"
// #include "portal_html.h"

// #if OLED_SH1106
// U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// #else
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
// #endif
// WebServer server(80);
// DNSServer dns;
// Preferences prefs;

// enum View
// {
//     BOOT,
//     FACE,
//     CLOCK_VIEW,
//     MENU,
//     TIMER_SETUP_H,
//     TIMER_SETUP_M,
//     COUNTDOWN,
//     WEB_CONFIG,
//     MESSAGE_VIEW,
//     ALERT,
//     CONFIRM_ERASE
// };
// enum Event
// {
//     NONE,
//     SHORT_PRESS,
//     LONG_PRESS,
//     MENU_HOLD,
//     HOLD_5S
// };
// View view = BOOT, previous = FACE;
// uint8_t expression = 0, menuIndex = 0, brightness = 255;
// bool soundOn = true, use24Hour = true, portalOn = false, timeOK = false, weatherOK = false, weatherCached = false;
// String weatherTemp = "--", weatherPlace = "Sylhet", lastError = "";
// String ssid, pass, apiKey, city, country, timezoneName;
// uint32_t bootAt, lastPaint = 0, lastClock = 0, lastWeather = 0, countdownEnd = 0, timerPauseAt = 0;
// bool timerRunning = false;
// uint8_t setHours = 0, setMinutes = 1;
// uint8_t bitmap[1024];
// bool bitmapValid = false;
// String liveText;
// uint8_t textX = 0, textY = 0;
// // Non-blocking passive-piezo pattern player. GPIO3 drives the + lead; the - lead is GND.
// bool tonePlaying = false;
// uint8_t toneSteps = 0, toneStep = 0;
// uint32_t toneNextAt = 0;
// String cleanDisplayText(const String &raw);

// struct TouchInput
// {
//     bool raw = false, latched = false, longSent = false, menuSent = false, holdSent = false;
//     uint32_t changed = 0, pressedAt = 0;
//     Event poll()
//     {
//         bool now = digitalRead(TOUCH_PIN) == (TOUCH_ACTIVE_HIGH ? HIGH : LOW);
//         uint32_t t = millis();
//         if (now != raw)
//         {
//             raw = now;
//             changed = t;
//         }
//         if (t - changed < 25)
//             return NONE;
//         if (raw && !latched)
//         {
//             latched = true;
//             pressedAt = t;
//             longSent = false;
//             menuSent = false;
//             holdSent = false;
//         }
//         if (raw && latched && !holdSent && t - pressedAt >= WEB_EXIT_HOLD_MS)
//         {
//             holdSent = true;
//             return HOLD_5S;
//         }
//         if (raw && latched && !menuSent && t - pressedAt >= MENU_HOLD_MS)
//         {
//             menuSent = true;
//             return MENU_HOLD;
//         }
//         if (raw && latched && !longSent && t - pressedAt >= LONG_PRESS_MS)
//         {
//             longSent = true;
//             return LONG_PRESS;
//         }
//         if (!raw && latched)
//         {
//             latched = false;
//             if (!longSent)
//                 return SHORT_PRESS;
//         }
//         return NONE;
//     }
// } touch;

// void beep(uint8_t pattern = 1)
// {
//     if (!soundOn)
//         return;
//     toneSteps = (pattern == 2) ? 3 : 1; // one tone, or tone-gap-tone
//     toneStep = 0;
//     tonePlaying = true;
//     toneNextAt = 0;
// }
// void updateBuzzer()
// {
//     if (!tonePlaying)
//         return;
//     uint32_t now = millis();
//     if (toneNextAt && (int32_t)(now - toneNextAt) < 0)
//         return;
//     if ((toneStep & 1) == 0)
//     {
//         ledcWriteTone(BUZZER_PIN, 2200); // audible, safe piezo frequency
//         toneNextAt = now + 90;
//     }
//     else
//     {
//         ledcWriteTone(BUZZER_PIN, 0);
//         toneNextAt = now + 70;
//     }
//     toneStep++;
//     if (toneStep >= toneSteps * 2)
//     {
//         ledcWriteTone(BUZZER_PIN, 0);
//         tonePlaying = false;
//     }
// }
// void loadSettings()
// {
//     if (!prefs.begin("4ngry", false))
//     {
//         lastError = "STORAGE: INIT ERROR";
//         return;
//     }
//     ssid = prefs.getString("ssid", "");
//     pass = prefs.getString("pass", "");
//     apiKey = prefs.getString("key", WEATHER_DEFAULT_KEY);
//     city = prefs.getString("city", WEATHER_DEFAULT_CITY);
//     country = prefs.getString("country", WEATHER_DEFAULT_COUNTRY);
//     timezoneName = prefs.getString("tz", TIMEZONE_DEFAULT);
//     brightness = prefs.getUChar("bright", 255);
//     soundOn = prefs.getBool("sound", true);
//     expression = prefs.getUChar("expr", 0) % 10;
//     use24Hour = prefs.getBool("clock24", true);
// }
// void saveUI()
// {
//     prefs.putUChar("bright", brightness);
//     prefs.putBool("sound", soundOn);
//     prefs.putUChar("expr", expression);
//     prefs.putBool("clock24", use24Hour);
// }
// void text(const String &a, const String &b = "", const String &c = "")
// {
//     oled.clearBuffer();
//     oled.setFont(u8g2_font_6x12_tf);
//     oled.drawStr(0, 14, a.c_str());
//     oled.drawStr(0, 31, b.c_str());
//     oled.drawStr(0, 48, c.c_str());
//     oled.sendBuffer();
// }

// // MTR / 4ngry boot ident. This is intentionally a short, self-contained startup animation.
// void playBootAnimation()
// {
//     const int cx = 64;
//     const int cy = 32;

//     auto drawMTRLogo = []()
//     {
//         oled.setDrawColor(1);
//         oled.setFont(u8g2_font_helvB24_tf);
//         const char *logo = "MTR";
//         int logoWidth = oled.getStrWidth(logo);
//         int logoX = (128 - logoWidth) / 2;
//         oled.drawStr(logoX, 34, logo);
//         int lineWidth = logoWidth + 6;
//         oled.drawHLine((128 - lineWidth) / 2, 39, lineWidth);
//         oled.setFont(u8g2_font_5x8_tf);
//         const char *subtitle = "MR THORIN RAPTOR";
//         int subWidth = oled.getStrWidth(subtitle);
//         oled.drawStr((128 - subWidth) / 2, 48, subtitle);
//         oled.setFont(u8g2_font_6x10_tf);
//         const char *signature = "4ngry";
//         int sigWidth = oled.getStrWidth(signature);
//         oled.drawStr((128 - sigWidth) / 2, 61, signature);
//     };

//     for (int r = 0; r <= 80; r += 4)
//     {
//         oled.clearBuffer();
//         oled.setDrawColor(1);
//         oled.drawDisc(cx, cy, r);
//         oled.sendBuffer();
//         delay(10);
//     }
//     for (int r = 0; r <= 80; r += 4)
//     {
//         oled.clearBuffer();
//         oled.setDrawColor(1);
//         oled.drawDisc(cx, cy, 80);
//         oled.setDrawColor(0);
//         oled.drawDisc(cx, cy, r);
//         oled.sendBuffer();
//         delay(10);
//     }
//     for (int width = 0; width <= 64; width += 8)
//     {
//         oled.clearBuffer();
//         oled.setDrawColor(1);
//         oled.drawHLine(0, 15, width);
//         oled.drawHLine(128 - width, 48, width);
//         if (width >= 24)
//             drawMTRLogo();
//         oled.sendBuffer();
//         delay(35);
//     }
//     oled.clearBuffer();
//     oled.setDrawColor(1);
//     oled.drawBox(0, 0, 128, 64);
//     oled.setDrawColor(0);
//     oled.setFont(u8g2_font_helvB24_tf);
//     const char *logo = "MTR";
//     int logoWidth = oled.getStrWidth(logo);
//     oled.drawStr((128 - logoWidth) / 2, 36, logo);
//     oled.sendBuffer();
//     delay(100);

//     oled.clearBuffer();
//     drawMTRLogo();
//     oled.sendBuffer();
//     delay(2200);
// }
// void face(uint8_t e)
// {
//     oled.clearBuffer();
//     int dx = (millis() / 500 % 3) - 1;
//     oled.setDrawColor(1); // eyes and expressions use primitives, not bitmaps
//     if (e == 7)
//     {
//         oled.drawDisc(38, 28, 10);
//         oled.drawDisc(90, 28, 10);
//         oled.setDrawColor(0);
//         oled.drawTriangle(34, 25, 42, 25, 38, 34);
//         oled.drawTriangle(86, 25, 94, 25, 90, 34);
//         oled.setDrawColor(1);
//     }
//     else if (e == 2)
//     {
//         oled.drawHLine(25, 30, 24);
//         oled.drawHLine(79, 30, 24);
//     }
//     else if (e == 9)
//     {
//         oled.drawDisc(38, 28, 9);
//         oled.drawCircle(90, 28, 10);
//     }
//     else
//     {
//         oled.drawDisc(38 + dx, 28, 9);
//         oled.drawDisc(90 + dx, 28, 9);
//     }
//     if (e == 0 || e == 1)
//     {
//         oled.drawCircle(64, 33, 17, U8G2_DRAW_LOWER_RIGHT);
//         oled.drawHLine(48, 33, 32);
//     }
//     else if (e == 3)
//     {
//         oled.drawCircle(64, 54, 17, U8G2_DRAW_UPPER_RIGHT);
//         oled.drawHLine(48, 54, 32);
//     }
//     else if (e == 4)
//     {
//         oled.drawCircle(30, 43, 3);
//         oled.drawCircle(98, 43, 3);
//     }
//     else if (e == 5)
//     {
//         oled.drawBox(29, 43, 18, 3);
//         oled.drawBox(81, 43, 18, 3);
//     }
//     else if (e == 6)
//     {
//         oled.drawCircle(64, 46, 7);
//     }
//     else if (e == 8)
//     {
//         oled.drawStr(50, 48, "?");
//     }
//     else if (e == 9)
//     {
//         oled.drawCircle(64, 33, 12, U8G2_DRAW_LOWER_RIGHT);
//         oled.drawHLine(52, 33, 24);
//     }
//     oled.setFont(u8g2_font_5x7_tf);
//     const char *n[] = {"HAPPY", "LAUGH", "SLEEPY", "SAD", "CRY", "ANGRY", "WOW", "LOVE", "CURIOUS", "WINK"};
//     oled.drawStr(2, 62, n[e]);
//     oled.sendBuffer();
// }
// void drawClock()
// {
//     char timeBuf[12], dateBuf[24], secondBuf[4], ampm[4];
//     tm t;
//     oled.clearBuffer();
//     if (timeOK && getLocalTime(&t, 0))
//     {
//         if (use24Hour)
//         {
//             strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
//             ampm[0] = 0;
//         }
//         else
//         {
//             strftime(timeBuf, sizeof(timeBuf), "%I:%M", &t);
//             if (timeBuf[0] == '0')
//                 memmove(timeBuf, timeBuf + 1, strlen(timeBuf));
//             strftime(ampm, sizeof(ampm), "%p", &t);
//         }
//         oled.setFont(u8g2_font_logisoso28_tf);
//         oled.drawStr(2, 35, timeBuf);
//         oled.setFont(u8g2_font_5x7_tf);
//         strftime(secondBuf, sizeof(secondBuf), "%S", &t);
//         oled.drawStr(104, 12, secondBuf);
//         if (!use24Hour)
//             oled.drawStr(104, 21, ampm);
//         oled.setFont(u8g2_font_6x12_tf);
//         strftime(dateBuf, sizeof(dateBuf), "%A, %d %B %Y", &t);
//         oled.drawStr(2, 51, dateBuf);
//     }
//     else
//     {
//         oled.setFont(u8g2_font_6x12_tf);
//         oled.drawStr(4, 30, "TIME UNAVAILABLE");
//     }
//     oled.setFont(u8g2_font_6x12_tf);
//     String w = weatherTemp + (weatherCached ? " cached" : "");
//     oled.drawStr(2, 63, w.c_str());
//     oled.sendBuffer();
// }
// void drawMenu()
// {
//     const char *m[] = {"Web Config", "Timer", "Brightness", "Sound", "Weather", "Ping Testing", "Wi-Fi Status", "OTA Update", "About", "Erase Settings", "Return Home"};
//     oled.clearBuffer();
//     oled.setFont(u8g2_font_6x12_tf);
//     for (int i = 0; i < 5; i++)
//     {
//         int n = (menuIndex / 5) * 5 + i;
//         if (n >= 11)
//             break;
//         int y = 12 + i * 11;
//         if (n == menuIndex)
//         {
//             oled.drawBox(0, y - 9, 128, 11);
//             oled.setDrawColor(0);
//         }
//         oled.drawStr(3, y, m[n]);
//         oled.setDrawColor(1);
//     }
//     oled.sendBuffer();
// }
// void drawTimer()
// {
//     oled.clearBuffer();
//     oled.setFont(u8g2_font_6x12_tf);
//     if (view == TIMER_SETUP_H || view == TIMER_SETUP_M)
//     {
//         oled.drawStr(2, 13, "TIMER SETUP");
//         char b[22];
//         sprintf(b, "%02u:%02u", setHours, setMinutes);
//         oled.setFont(u8g2_font_logisoso24_tf);
//         oled.drawStr(18, 45, b);
//         oled.setFont(u8g2_font_5x7_tf);
//         oled.drawStr(2, 62, view == TIMER_SETUP_H ? "tap hours, hold next" : "tap minutes, hold start");
//     }
//     else
//     {
//         uint32_t r = timerRunning ? (countdownEnd - millis()) : timerPauseAt;
//         char b[16];
//         sprintf(b, "%02lu:%02lu", r / 3600000UL, (r / 60000UL) % 60);
//         oled.setFont(u8g2_font_logisoso24_tf);
//         oled.drawStr(16, 38, b);
//         oled.setFont(u8g2_font_6x12_tf);
//         oled.drawStr(3, 60, timerRunning ? "tap pause; hold cancel" : "tap resume; hold cancel");
//     }
//     oled.sendBuffer();
// }
// void startPortal()
// {
//     // Web Config is deliberately AP-only: opening it disconnects any STA session.
//     // Do not call this again while active; connected phones must keep their AP link.
//     if (portalOn)
//         return;
//     WiFi.disconnect();
//     WiFi.mode(WIFI_AP);
//     IPAddress ip(192, 168, 4, 1), gw(192, 168, 4, 1), mask(255, 255, 255, 0);
//     if (!WiFi.softAPConfig(ip, gw, mask) || !WiFi.softAP(AP_SSID, AP_PASSWORD))
//     {
//         lastError = "AP: START FAILED";
//         return;
//     }
//     dns.start(53, "*", ip);
//     portalOn = true;
//     lastError = "";
// }
// void stopPortal()
// {
//     dns.stop();
//     server.stop();
//     WiFi.softAPdisconnect(true);
//     portalOn = false;
//     WiFi.mode(WIFI_STA);
//     if (ssid.length())
//         WiFi.begin(ssid.c_str(), pass.c_str());
//     server.begin();
//     view = FACE;
// }
// bool auth()
// {
//     if (server.authenticate("admin", AP_PASSWORD))
//         return true;
//     server.requestAuthentication();
//     return false;
// }
// void weatherTask(void *)
// {
//     if (WiFi.status() != WL_CONNECTED)
//     {
//         vTaskDelete(NULL);
//         return;
//     }
//     WiFiClientSecure c;
//     c.setInsecure();
//     HTTPClient h;
//     String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "," + country + "&units=metric&lang=en&appid=" + apiKey;
//     h.setConnectTimeout(5000);
//     h.setTimeout(6000);
//     if (h.begin(c, url))
//     {
//         int rc = h.GET();
//         if (rc == 200)
//         {
//             StaticJsonDocument<1024> d;
//             DeserializationError e = deserializeJson(d, h.getString());
//             if (!e && d["main"]["temp"].is<float>())
//             {
//                 weatherTemp = String(d["main"]["temp"].as<float>(), 1) + "C";
//                 weatherPlace = d["name"].as<String>();
//                 weatherOK = true;
//                 weatherCached = false;
//                 lastError = "";
//             }
//             else
//                 lastError = "WEATHER: BAD RESPONSE";
//         }
//         else if (rc == 401)
//             lastError = "WEATHER: INVALID API KEY";
//         else
//             lastError = "WEATHER: HTTP " + String(rc);
//         h.end();
//     }
//     vTaskDelete(NULL);
// }
// void scheduleWeather()
// {
//     if (WiFi.status() == WL_CONNECTED && millis() - lastWeather > WEATHER_PERIOD_MS)
//     {
//         lastWeather = millis();
//         xTaskCreate(weatherTask, "weather", 6144, nullptr, 1, nullptr);
//     }
// }
// void connectSaved()
// {
//     if (ssid.length())
//     {
//         WiFi.mode(WIFI_STA);
//         WiFi.begin(ssid.c_str(), pass.c_str());
//     }
// }
// void setTime() { configTzTime(timezoneName.c_str(), "pool.ntp.org", "time.nist.gov"); }
// void handlePortal()
// {
//     dns.processNextRequest();
//     server.handleClient();
// }
// String statusJson()
// {
//     DynamicJsonDocument d(512);
//     d["device"] = DEVICE_NAME;
//     d["firmware"] = FW_VERSION;
//     d["ap"] = portalOn;
//     d["clients"] = WiFi.softAPgetStationNum();
//     d["sta"] = WiFi.status() == WL_CONNECTED;
//     d["ip"] = WiFi.localIP().toString();
//     d["apip"] = WiFi.softAPIP().toString();
//     d["time"] = timeOK;
//     d["weatherAge"] = weatherOK ? String((millis() - lastWeather) / 1000) : "unavailable";
//     d["heap"] = ESP.getFreeHeap();
//     d["timer"] = timerRunning;
//     String s;
//     serializeJson(d, s);
//     return s;
// }
// void routes()
// {
//     server.on("/", []
//               { server.send_P(200, "text/html", PORTAL_HTML); });
//     server.on("/api/status", []
//               { server.send(200, "application/json", statusJson()); });
//     server.on("/api/scan", []
//               {
//   // Scanning is read-only, so it is available without a browser login prompt.
//   // Keep the existing AP alive while the radio briefly scans for stations.
//   int n=WiFi.scanNetworks(false, true);
//   if(n < 0){
//     server.send(503,"application/json","{\"error\":\"scan unavailable\"}");
//     return;
//   }
//   DynamicJsonDocument d(3072);
//   JsonArray a=d.to<JsonArray>();
//   for(int i=0;i<n && i<20;i++){
//     String networkName=WiFi.SSID(i);
//     if(!networkName.length()) continue;
//     JsonObject o=a.createNestedObject();
//     o["s"]=networkName;
//     o["r"]=WiFi.RSSI(i);
//     o["e"]=WiFi.encryptionType(i)!=WIFI_AUTH_OPEN;
//   }
//   WiFi.scanDelete();
//   String out;
//   serializeJson(a,out);
//   server.send(200,"application/json",out); });
//     server.on("/api/wifi", HTTP_POST, []
//               {if(!auth())return;String s=server.arg("ssid");String newPass=server.arg("pass");if(!s.length()||s.length()>32){server.send(400,"text/plain","Invalid network name");return;}bool wroteSsid=prefs.putString("ssid",s)==s.length();bool wrotePass=prefs.putString("pass",newPass)==newPass.length();if(!wroteSsid||!wrotePass||prefs.getString("ssid","")!=s||prefs.getString("pass","")!=newPass){server.send(500,"application/json","{\"saved\":false,\"message\":\"Could not save Wi-Fi settings. Your previous saved settings were kept where possible.\"}");return;}ssid=s;pass=newPass;WiFi.begin(ssid.c_str(),pass.c_str());server.send(202,"application/json","{\"saved\":true,\"message\":\"Wi-Fi settings saved on 4ngry. Testing the connection while setup Wi-Fi stays available.\"}"); });
//     server.on("/api/clock", HTTP_POST, []
//               {if(!auth())return;String mode=server.arg("mode");if(mode=="12")use24Hour=false;else if(mode=="24")use24Hour=true;else{server.send(400,"text/plain","Choose 12 or 24 hour mode.");return;}saveUI();server.send(200,"text/plain",use24Hour?"Clock set to 24-hour mode.":"Clock set to 12-hour mode."); });
//     server.on("/api/weather", HTTP_POST, []
//               {if(!auth())return;String k=server.arg("key");if(k.length())apiKey=k;city=server.arg("city");country=server.arg("country");timezoneName=server.arg("tz");if(!city.length()||country.length()!=2||timezoneName.length()<3){server.send(400,"text/plain","Check city, 2-letter country, and timezone.");return;}prefs.putString("key",apiKey);prefs.putString("city",city);prefs.putString("country",country);prefs.putString("tz",timezoneName);setTime();lastWeather=0;server.send(200,"text/plain","Saved; weather refresh scheduled."); });
//     server.on("/api/text", HTTP_POST, []
//               {
//   if(!auth())return;
//   String incoming=server.arg("text");
//   if(!incoming.length() || incoming.length()>160){server.send(400,"text/plain","Use 1 to 160 characters.");return;}
//   int requestedX=server.arg("x").toInt(), requestedY=server.arg("y").toInt();
//   textX=(uint8_t)constrain(requestedX,0,120);
//   textY=(uint8_t)constrain(requestedY,0,52);
//   liveText=cleanDisplayText(incoming);
//   bitmapValid=false;
//   view=MESSAGE_VIEW;
//   server.send(200,"text/plain","Message is now on the OLED."); });
//     server.on("/api/bitmap", HTTP_POST, []
//               {if(!auth())return;String h=server.arg("plain");if(h.length()!=2048){server.send(400,"text/plain","Expected exactly 1024 monochrome bytes.");return;}for(int i=0;i<1024;i++){char p[3]={h[i*2],h[i*2+1],0};char*end;long v=strtol(p,&end,16);if(*end){server.send(400,"text/plain","Invalid bitmap data");return;}bitmap[i]=v;}bitmapValid=true;view=MESSAGE_VIEW;server.send(200,"text/plain","Drawing sent. Touch to return."); });
//     server.on("/api/beep", []
//               {if(!auth())return;beep(server.arg("p").toInt());server.send(200,"text/plain","Sound command sent"); });
//     server.on("/api/sound", []
//               {if(!auth())return;soundOn=!soundOn;saveUI();server.send(200,"text/plain",soundOn?"Sound enabled":"Sound disabled"); });
//     server.on("/api/ota", HTTP_POST, []
//               {if(!auth())return;if(Update.hasError()){server.send(500,"text/plain","Update failed; existing firmware remains active.");}else {server.send(200,"text/plain","Update verified. Restarting now.");delay(700);ESP.restart();} }, []
//               {if(!auth())return;HTTPUpload&u=server.upload();if(u.status==UPLOAD_FILE_START){if(!u.filename.endsWith(".bin")||!Update.begin(UPDATE_SIZE_UNKNOWN)){Update.printError(Serial);}}else if(u.status==UPLOAD_FILE_WRITE){if(Update.write(u.buf,u.currentSize)!=u.currentSize)Update.printError(Serial);}else if(u.status==UPLOAD_FILE_END){if(!Update.end(true))Update.printError(Serial);} });
//     server.onNotFound([]()
//                       {if(server.uri().startsWith("/api/")){server.send(404,"text/plain","Not found");}else server.sendHeader("Location","http://192.168.4.1/");server.send(302,"text/plain",""); });
//     server.begin();
// }
// String cleanDisplayText(const String &raw)
// {
//     String out;
//     out.reserve(raw.length());
//     for (size_t i = 0; i < raw.length(); i++)
//     {
//         uint8_t c = (uint8_t)raw[i];
//         // Preserve intentional line breaks; this font has printable ASCII only.
//         if (c >= 32 && c <= 126)
//             out += (char)c;
//         else if (c == '\n')
//             out += '\n';
//         else if (c == '\r')
//         { /* Ignore CR in browser CRLF line endings. */
//         }
//         else
//             out += '?';
//     }
//     return out;
// }
// void messageView()
// {
//     oled.clearBuffer();
//     if (bitmapValid)
//     {
//         // XBM is least-significant-bit first. The portal sends this exact order.
//         oled.drawXBMP(0, 0, 128, 64, bitmap);
//     }
//     else
//     {
//         String remaining = liveText;
//         oled.setFont(u8g2_font_6x12_tf);
//         const uint8_t charsPerLine = 20;
//         uint8_t row = 0;
//         while (remaining.length() && row < 5)
//         {
//             int newline = remaining.indexOf('\n');
//             String paragraph = (newline >= 0) ? remaining.substring(0, newline) : remaining;
//             if (newline >= 0)
//                 remaining.remove(0, newline + 1);
//             else
//                 remaining = "";
//             if (!paragraph.length())
//             {
//                 row++;
//                 continue;
//             }
//             while (paragraph.length() && row < 5)
//             {
//                 uint8_t take = min((size_t)charsPerLine, paragraph.length());
//                 int breakAt = paragraph.lastIndexOf(' ', take);
//                 if (breakAt > 0 && take == charsPerLine)
//                     take = breakAt;
//                 String line = paragraph.substring(0, take);
//                 line.trim();
//                 if (textY + 11 + row * 12 <= 63)
//                     oled.drawStr(textX, textY + 11 + row * 12, line.c_str());
//                 paragraph.remove(0, take);
//                 paragraph.trim();
//                 row++;
//             }
//         }
//     }
//     oled.sendBuffer();
// }
// void activateMenu()
// {
//     switch (menuIndex)
//     {
//     case 0:
//         startPortal();
//         view = WEB_CONFIG;
//         break;
//     case 1:
//         view = TIMER_SETUP_H;
//         break;
//     case 2:
//         brightness = (brightness == 255 ? 96 : 255);
//         oled.setContrast(brightness);
//         saveUI();
//         break;
//     case 3:
//         soundOn = !soundOn;
//         saveUI();
//         break;
//     case 4:
//         lastWeather = 0;
//         break;
//     case 5:
//         text("PING TESTING", WiFi.status() == WL_CONNECTED ? "STA CONNECTED" : "STA NOT CONNECTED", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Connect Wi-Fi in Web Config");
//         break;
//     case 6:
//         text(WiFi.status() == WL_CONNECTED ? "WIFI CONNECTED" : "WIFI OFFLINE", WiFi.localIP().toString());
//         break;
//     case 7:
//         startPortal();
//         view = WEB_CONFIG;
//         break;
//     case 8:
//         text("4ngry DeskBuddy", FW_VERSION, "ESP32-C3");
//         break;
//     case 9:
//         view = CONFIRM_ERASE;
//         break;
//     default:
//         view = FACE;
//     }
// }
// void input(Event e)
// {
//     if (e == NONE)
//         return;
//     if (view == WEB_CONFIG && e == HOLD_5S)
//     {
//         stopPortal();
//         return;
//     }
//     // The compact menu is deliberately hidden from normal navigation. Hold either
//     // main screen for three seconds to open it; a five-second hold in Web Config
//     // remains reserved for closing the access point.
//     if (e == MENU_HOLD && (view == FACE || view == CLOCK_VIEW))
//     {
//         view = MENU;
//         return;
//     }
//     if (view == MESSAGE_VIEW || view == ALERT)
//     {
//         if (e == SHORT_PRESS || e == LONG_PRESS)
//         {
//             bitmapValid = false;
//             view = FACE;
//         }
//         return;
//     }
//     if (view == FACE && e == SHORT_PRESS)
//     {
//         expression = (expression + 1) % 10;
//         saveUI();
//     }
//     else if (view == FACE && e == LONG_PRESS)
//         view = CLOCK_VIEW;
//     else if (view == CLOCK_VIEW && e == SHORT_PRESS)
//         view = TIMER_SETUP_H;
//     else if (view == CLOCK_VIEW && e == LONG_PRESS)
//         view = FACE;
//     else if (view == MENU)
//     {
//         if (e == SHORT_PRESS)
//             menuIndex = (menuIndex + 1) % 11;
//         else if (e == LONG_PRESS)
//             activateMenu();
//     }
//     else if (view == TIMER_SETUP_H)
//     {
//         if (e == SHORT_PRESS)
//             setHours = (setHours + 1) % 100;
//         else if (e == LONG_PRESS)
//             view = TIMER_SETUP_M;
//     }
//     else if (view == TIMER_SETUP_M)
//     {
//         if (e == SHORT_PRESS)
//             setMinutes = (setMinutes + 1) % 60;
//         else if (e == LONG_PRESS)
//         {
//             if (setHours || setMinutes)
//             {
//                 countdownEnd = millis() + ((uint32_t)setHours * 3600UL + setMinutes * 60UL) * 1000UL;
//                 timerRunning = true;
//                 view = COUNTDOWN;
//             }
//         }
//     }
//     else if (view == COUNTDOWN)
//     {
//         if (e == SHORT_PRESS)
//         {
//             if (timerRunning)
//             {
//                 timerPauseAt = countdownEnd - millis();
//                 timerRunning = false;
//             }
//             else
//             {
//                 countdownEnd = millis() + timerPauseAt;
//                 timerRunning = true;
//             }
//         }
//         else if (e == LONG_PRESS)
//             view = CLOCK_VIEW;
//     }
//     else if (view == CONFIRM_ERASE)
//     {
//         if (e == LONG_PRESS)
//         {
//             prefs.clear();
//             text("SETTINGS ERASED", "Restarting...");
//             delay(600);
//             ESP.restart();
//         }
//         else if (e == SHORT_PRESS)
//             view = MENU;
//     }
// }
// void setup()
// {
//     Serial.begin(115200);
//     pinMode(TOUCH_PIN, INPUT);
//     ledcAttach(BUZZER_PIN, 2200, 8);
//     ledcWriteTone(BUZZER_PIN, 0);
//     Wire.begin(OLED_SDA, OLED_SCL);
//     oled.setI2CAddress(OLED_ADDRESS << 1);
//     oled.begin();
//     playBootAnimation();
//     loadSettings();
//     oled.setContrast(brightness);
//     bootAt = millis();
//     connectSaved();
//     setTime();
//     routes();
// }
// void loop()
// {
//     uint32_t now = millis();
//     Event e = touch.poll();
//     input(e);
//     updateBuzzer();
//     if (portalOn)
//         handlePortal();
//     if (WiFi.status() == WL_CONNECTED && !timeOK)
//     {
//         tm t;
//         if (getLocalTime(&t, 0))
//             timeOK = true;
//     }
//     if (WiFi.status() != WL_CONNECTED && ssid.length() && now > WIFI_CONNECT_TIMEOUT_MS && lastError.length() == 0)
//         lastError = "WIFI: CONNECT TIMEOUT";
//     scheduleWeather();
//     if (timerRunning && ((int32_t)(now - countdownEnd) >= 0))
//     {
//         timerRunning = false;
//         view = ALERT;
//         beep(2);
//     }
//     if (now - lastPaint >= 50)
//     {
//         lastPaint = now;
//         if (view == BOOT)
//         {
//             view = FACE;
//             if (!ssid.length())
//             {
//                 lastError = "WIFI: NO SAVED NETWORK";
//                 startPortal();
//             }
//         }
//         else if (view == FACE)
//             face(expression);
//         else if (view == CLOCK_VIEW)
//             drawClock();
//         else if (view == MENU)
//             drawMenu();
//         else if (view == TIMER_SETUP_H || view == TIMER_SETUP_M || view == COUNTDOWN)
//             drawTimer();
//         else if (view == WEB_CONFIG)
//         {
//             oled.clearBuffer();
//             oled.setFont(u8g2_font_6x12_tf);
//             String a = "AP: 4ngry  clients: " + String(WiFi.softAPgetStationNum());
//             String b = WiFi.softAPgetStationNum() ? "DEVICE CONNECTED" : "Open 192.168.4.1";
//             oled.drawStr(0, 14, a.c_str());
//             oled.drawStr(0, 31, b.c_str());
//             oled.setFont(u8g2_font_5x7_tf);
//             oled.drawStr(0, 43, "Hold touch 5 seconds to exit");
//             uint32_t held = (touch.raw && touch.latched) ? (now - touch.pressedAt) : 0;
//             if (held)
//             {
//                 uint8_t w = (uint8_t)min(124UL, (held * 124UL) / WEB_EXIT_HOLD_MS);
//                 oled.drawFrame(2, 52, 124, 9);
//                 oled.drawBox(2, 52, w, 9);
//             }
//             oled.sendBuffer();
//         }
//         else if (view == MESSAGE_VIEW)
//             messageView();
//         else if (view == ALERT)
//             text("TIMER COMPLETE", "Touch to dismiss");
//         else if (view == CONFIRM_ERASE)
//             text("ERASE ALL SETTINGS?", "hold touch to confirm", "tap to cancel");
//     }
//     delay(1);
// }
