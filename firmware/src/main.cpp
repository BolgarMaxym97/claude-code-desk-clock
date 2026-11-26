// Claude Clock firmware: shows what Claude Code is doing on the Mac, the time and Claude limits.
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "api.h"
#include "config.h"
#include "device.h"
#include "fetch.h"
#include "settings.h"
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

// "Today with Claude" on the info card: busy time, answers and commits of the local day. Kept across restarts:
// RTC memory every second (survives reset, OTA and crashes), the flash settings record every 30 min (survives
// power loss; ~50 writes a day spread over two sectors).
static const uint32_t RTC_TODAY = 66, TODAY_MAGIC = 0x70DA7001, TODAY_FLASH_MS = 30u * 60000u;
struct RtcToday { uint32_t magic; TodaySnap t; uint32_t check; };

static uint32_t todayCheck(const TodaySnap &t) { return TODAY_MAGIC ^ uint32_t(t.day) * 2654435761u ^ t.busyMs ^ uint32_t(t.answers) << 8 ^ uint32_t(t.commits) << 20; }

static void restoreToday() {
    RtcToday r{};
    ESP.rtcUserMemoryRead(RTC_TODAY, reinterpret_cast<uint32_t *>(&r), sizeof r);
    TodaySnap t{-1, 0, 0, 0};
    int d, a, c;
    uint32_t b;
    if (r.magic == TODAY_MAGIC && r.check == todayCheck(r.t)) t = r.t;        // warm restart: the newest
    else if (parseToday(deviceToday(), d, b, a, c)) t = TodaySnap{d, b, a, c};  // after power loss
    infoState().restore(t);
}

static void feedToday(const LocalTime &lt, uint32_t now) {
    static uint32_t lastDone = 0, lastCommits = 0, lastFlash = 0;
    static TodaySnap rtc{-2, 0, 0, 0};
    Info &in = infoState();
    in.setRegion(deviceRegion());
    if (!lt.synced) return;
    int day = lt.month * 32 + lt.day;
    in.onBusy(model.busyCount() > 0, now, day);
    if (model.lastDoneAtMs() != lastDone) { lastDone = model.lastDoneAtMs(); in.onAnswer(day); }
    for (; lastCommits < model.commitCount(); lastCommits++) in.onCommit(day);
    TodaySnap t = in.today();
    if (memcmp(&t, &rtc, sizeof t)) {
        rtc = t;
        RtcToday r{TODAY_MAGIC, t, todayCheck(t)};
        ESP.rtcUserMemoryWrite(RTC_TODAY, reinterpret_cast<uint32_t *>(&r), sizeof r);
    }
    if (now - lastFlash >= TODAY_FLASH_MS) {
        lastFlash = now;
        char b[48];
        snprintf(b, sizeof b, "%d,%u,%d,%d", int(t.day), unsigned(t.busyMs), int(t.answers), int(t.commits));
        deviceSaveToday(b);  // writes only when it changed
    }
}

void setup() {
    Serial.begin(115200);
    bootGuard();
    panelInit(tft);
    tft.fillScreen(col::BG);
    screensBegin();
    storeBegin(safeBoot);  // raw records only, never format; safe mode: read-only
    deviceBegin();         // token (made on first boot), timezone, birthday
    restoreToday();
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
    fetchTick(now);
    model.setAlert(infoState().alert(now) != AlertLevel::None);
    if (!stableMarked && now > 60000) markStable();
    time_t epoch;
    LocalTime lt = localNow(epoch);
    if (now - lastTick >= 1000) {
        lastTick = now;
        model.tick(now);
        feedToday(lt, now);
    }
    bool frame = now - lastFrame >= FRAME_MS;
    if (frame) { lastFrame = now; tick++; }
    screensUpdate(model, now, lt, epoch, netInfo(), frame, tick);
}
