// Claude Clock firmware: shows what Claude Code is doing on the Mac, the time and Claude limits.
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "api.h"
#include "config.h"
#include "device.h"
#include "model.h"
#include "net.h"
#include "portal.h"
#include "store.h"
#include "wifisetup.h"
#include "panel.h"
#include "screens.h"

TFT_eSPI tft;
static ESP8266WebServer server(80);
static Model model;
static uint32_t lastFrame = 0, lastTick = 0, tick = 0;
static bool stableMarked = false;
bool safeBoot = false;  // crash-loop safe mode: flash records read-only (no erase/write); OTA still works

// Crash-loop guard. RTC user memory survives resets (not power loss). Slot 64 (byte 256) is clear of the
// OTA bootloader command at the start of that memory.
static const uint32_t RTC_SLOT = 64, RTC_MAGIC = 0xC10CC10C;
struct RtcGuard { uint32_t magic, count; };

static void bootGuard() {
    RtcGuard g{};
    ESP.rtcUserMemoryRead(RTC_SLOT, reinterpret_cast<uint32_t *>(&g), sizeof g);
    bool valid = g.magic == RTC_MAGIC && g.count < 256;
    uint8_t n = nextCrashCount(uint8_t(g.count), valid, int(ESP.getResetInfoPtr()->reason));
    safeBoot = safeMode(n);
    g = RtcGuard{RTC_MAGIC, n};
    ESP.rtcUserMemoryWrite(RTC_SLOT, reinterpret_cast<uint32_t *>(&g), sizeof g);
}

static void markStable() {  // 60 s without a crash -> reset the counter
    RtcGuard g{RTC_MAGIC, 0};
    ESP.rtcUserMemoryWrite(RTC_SLOT, reinterpret_cast<uint32_t *>(&g), sizeof g);
    stableMarked = true;
}

static LocalTime localNow(time_t &epoch) {
    epoch = time(nullptr);
    struct tm t;
    localtime_r(&epoch, &t);
    return LocalTime{t.tm_year >= 120, t.tm_hour, t.tm_min, t.tm_mon + 1, t.tm_mday, t.tm_wday, t.tm_year + 1900};
}

void setup() {
    Serial.begin(115200);
    bootGuard();
    panelInit(tft);
    tft.fillScreen(col::BG);
    screensBegin();
    storeBegin(safeBoot);  // raw records only, never format; safe mode: read-only
    deviceBegin();         // token (made on first boot), timezone, birthday
    netApplyTz();
    netBegin();
    apiBegin(server, model);
    portalBegin(server);
    server.begin();
}

void loop() {
    server.handleClient();
    uint32_t now = millis();
    netTick(now);
    if (!stableMarked && now > 60000) markStable();
    if (now - lastTick >= 1000) { lastTick = now; model.tick(now); }
    bool frame = now - lastFrame >= FRAME_MS;
    if (frame) { lastFrame = now; tick++; }
    time_t epoch;
    LocalTime lt = localNow(epoch);
    screensUpdate(model, now, lt, epoch, netInfo(), frame, tick);
}
