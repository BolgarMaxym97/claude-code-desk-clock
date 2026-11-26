#include "api.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include "config.h"
#include "device.h"
#include "fetch.h"
#include "text.h"
#include "net.h"
#include "portal.h"
#include "store.h"

extern bool safeBoot;
extern "C" uint32_t _FS_start, _FS_end, _FS_block, _FS_page;

// Read-only peek at the filesystem area (superblock magic / block size) for diagnostics.
static String fsDiag() {
    uint32_t start = uint32_t(&_FS_start) - 0x40200000, blk = uint32_t(&_FS_block);
    String o = String("start=0x") + String(start, HEX) + " end=0x" + String(uint32_t(&_FS_end) - 0x40200000, HEX) +
               " blk=" + blk + " page=" + uint32_t(&_FS_page);
    for (uint32_t off : {0u, 4096u, 8192u}) {
        uint8_t b[32];
        if (!ESP.flashRead(start + off, reinterpret_cast<uint32_t *>(b), sizeof b)) continue;
        o += String(" @") + off + ":";
        for (uint8_t c : b) { char h[3]; snprintf(h, sizeof h, "%02x", c); o += h; }
    }
    for (uint32_t sec = 0x3F0000; sec < 0x3FA000; sec += 0x1000) {  // non-erased bytes per tail sector
        uint32_t used = 0;
        for (uint32_t off = 0; off < 4096; off += 256) {
            uint8_t b[256];
            ESP.flashRead(sec + off, reinterpret_cast<uint32_t *>(b), sizeof b);
            for (uint8_t c : b) used += c != 0xFF;
        }
        o += String(" 0x") + String(sec, HEX) + "=" + used;
    }
    return o;
}

static ESP8266WebServer *srv;
static Model *mdl;
static ESP8266HTTPUpdateServer updater;

static const char *SCREEN_NAMES[] = {"Boot", "FirstSetup", "Clock", "Sleep", "Working", "Multi", "Permission",
                                     "YourTurn", "Error", "Done", "Hello", "LimitReached", "MacSilent"};

static bool authorized() {
    if (srv->header("X-Clock-Token") == deviceToken()) return true;
    srv->send(401, "application/json", "{\"error\":\"token\"}");
    return false;
}

static bool parseBody(JsonDocument &doc) {
    if (deserializeJson(doc, srv->arg("plain")) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        srv->send(400, "application/json", "{\"error\":\"json\"}");
        return false;
    }
    return true;
}

static void ok() { srv->send(200, "application/json", "{\"ok\":true}"); }

static void handleEvent() {
    if (!authorized()) return;
    JsonDocument doc;
    if (!parseBody(doc)) return;
    EventIn e;
    e.session = doc["session"] | "";
    e.project = utf8Truncate(doc["project"] | "", 32);
    e.branch = utf8Truncate(doc["branch"] | "", 32);
    e.tool = utf8Truncate(doc["tool"] | "", 48);
    e.target = utf8Truncate(doc["target"] | "", 22);
    e.ev = parseEvent(doc["event"] | "", doc["kind"] | "");
    e.kinds = utf8Truncate(doc["kinds"] | "", 48);
    mdl->onEvent(e, millis());
    ok();
}

static void handleLimits() {
    if (!authorized()) return;
    JsonDocument doc;
    if (!parseBody(doc)) return;
    mdl->onLimits(doc["h5"]["pct"] | 0, doc["h5"]["resets_at"] | 0u, doc["d7"]["pct"] | 0, doc["d7"]["resets_at"] | 0u,
                  millis());
    ok();
}

static void handleStatus() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    LocalTime lt{t.tm_year >= 120, t.tm_hour, t.tm_min, t.tm_mon + 1, t.tm_mday, t.tm_wday};
    View v = mdl->view(millis(), lt);
    JsonDocument doc;
    doc["fw"] = FW_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["maxblock"] = ESP.getMaxFreeBlockSize();
    doc["reset"] = ESP.getResetReason();  // why the clock last restarted (stability check)
    doc["uptime"] = millis() / 1000;
    doc["sessions"] = mdl->count();
    doc["busy"] = mdl->busyCount();
    doc["screen"] = SCREEN_NAMES[int(v.screen)];
    static const char *REACT[] = {"", "tests-ok", "tests-fail", "commit", "push", "install", "docker"};
    if (mdl->reaction() != Reaction::None && millis() - mdl->reactionAtMs() <= 10000) doc["reaction"] = REACT[int(mdl->reaction())];
    doc["h5"] = mdl->limit5h().pct;
    doc["d7"] = mdl->limit7d().pct;
    JsonArray list = doc["list"].to<JsonArray>();
    for (const auto &cs : mdl->sessions()) {
        JsonObject o = list.add<JsonObject>();
        o["id"] = cs.id.substr(0, 8);
        o["project"] = cs.project;
        o["state"] = int(cs.state);
        o["tool"] = cs.tool;
        o["agents"] = cs.agents;
        o["idle_s"] = (millis() - cs.lastMs) / 1000;
    }
    char tb[6];
    snprintf(tb, sizeof tb, "%02d:%02d", t.tm_hour, t.tm_min);
    doc["time"] = lt.synced ? tb : "--:--";
    NetInfo n = netInfo();
    doc["setup"] = n.apOn;
    doc["fails"] = n.fails;
    doc["ssid"] = n.ssid;
    doc["fs"] = storeOk();
    doc["safe"] = safeBoot;
    doc["settings"] = deviceSettingsSaved();
    doc["tz"] = deviceTz();
    doc["lang"] = deviceLang();
    {
        uint32_t now = millis();
        const Info &in = infoState();
        doc["weather"] = !*deviceOwm() ? "off" : in.hasWeather(now) ? String(weatherText(in.weatherNow()).c_str()) : String("waiting");
        doc["alerts"] = *deviceRegion() ? deviceRegion() : "off";  // region name, never the key
        static const char *const LV[] = {"none", "alert", "drone", "missile"};
        doc["alert"] = LV[int(in.alert(now))];
        Info::Card c = in.card(now, (lt.month) * 32 + lt.day);
        doc["card"] = String(c.line1.c_str()) + " / " + c.line2.c_str();
    }
    if (srv->hasArg("fs")) doc["fsdiag"] = fsDiag();
    String out;
    serializeJson(doc, out);
    srv->send(200, "application/json", out);
}

// Pairing a computer: start shows a code on the clock screen, the computer sends it back and gets the token.
static void handlePairStart() {
    if (!devicePairStart(millis())) { srv->send(423, "application/json", "{\"error\":\"locked\"}"); return; }
    ok();
}

static void handlePair() {
    JsonDocument doc;
    if (!parseBody(doc)) return;
    int r = devicePairCheck(doc["code"] | "", millis());
    if (r == 1) { srv->send(403, "application/json", "{\"error\":\"code\"}"); return; }
    if (r == 2) { srv->send(410, "application/json", "{\"error\":\"closed\"}"); return; }
    if (r == 3) { srv->send(423, "application/json", "{\"error\":\"locked\"}"); return; }
    JsonDocument out;
    out["token"] = deviceToken();
    out["fw"] = FW_VERSION;
    String body;
    serializeJson(out, body);
    srv->send(200, "application/json", body);
}

static void handleInfo() {  // {"alert": "none|drone|missile|alert"} (alert level from the computer, later)
    if (!authorized()) return;
    JsonDocument doc;
    if (!parseBody(doc)) return;
    uint32_t now = millis();
    if (doc["alert"].is<const char *>() && *deviceRegion()) {
        String a = doc["alert"].as<const char *>();
        AlertLevel l = a == "drone" ? AlertLevel::Drone : a == "missile" ? AlertLevel::Missile
                     : a == "alert" ? AlertLevel::Alert : AlertLevel::None;
        infoState().onAlertLevel(l, now);
    }
    ok();
}

static void handleSettings() {  // {"tz": "<POSIX TZ>", "birthday": "MM-DD" | "", "lang": "en" | "uk"}
    if (!authorized()) return;
    JsonDocument doc;
    if (!parseBody(doc)) return;
    if (doc["tz"].is<const char *>()) {
        if (!deviceSetTz(doc["tz"].as<const char *>())) { srv->send(400, "application/json", "{\"error\":\"tz\"}"); return; }
        netApplyTz();
    }
    if (doc["lang"].is<const char *>() && !deviceSetLang(doc["lang"].as<const char *>())) {
        srv->send(400, "application/json", "{\"error\":\"lang\"}");
        return;
    }
    if (doc["birthday"].is<const char *>() && !deviceSetBirthday(doc["birthday"].as<const char *>())) {
        srv->send(400, "application/json", "{\"error\":\"birthday\"}");
        return;
    }
    // weather / alerts: "" switches the source off
    struct F { const char *key; bool (*set)(const String &); } fields[] = {
        {"owm", deviceSetOwm}, {"loc", deviceSetLoc}, {"region", deviceSetRegion}};
    for (const F &f : fields) {
        if (!doc[f.key].is<const char *>()) continue;
        if (!f.set(doc[f.key].as<const char *>())) {
            srv->send(400, "application/json", String("{\"error\":\"") + f.key + "\"}");
            return;
        }
        if (!strcmp(f.key, "region") && !*deviceRegion()) infoState().alertsOff();
        fetchSoon();
    }
    srv->send(200, "application/json", deviceSettingsSaved() ? "{\"ok\":true}" : "{\"ok\":true,\"saved\":false}");
}

void apiBegin(ESP8266WebServer &server, Model &model) {
    srv = &server;
    mdl = &model;
    server.collectHeaders("X-Clock-Token");
    server.on("/api/event", HTTP_POST, handleEvent);
    server.on("/api/limits", HTTP_POST, handleLimits);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/api/pair/start", HTTP_POST, handlePairStart);
    server.on("/api/pair", HTTP_POST, handlePair);
    server.on("/api/settings", HTTP_POST, handleSettings);
    server.on("/api/info", HTTP_POST, handleInfo);
    server.on("/api/wifi-setup", HTTP_POST, []() {  // open the setup AP for 10 min (change network from the Mac)
        if (!authorized()) return;
        netOpenSetup(600000);
        ok();
    });
    server.on("/api/preview-newyear", HTTP_POST, []() {  // show the New Year look for 2 min
        if (!authorized()) return;
        screensPreviewNewYear(120000, srv->hasArg("midnight"));
        ok();
    });
    server.on("/api/preview-scene", HTTP_POST, []() {  // ?name=easter  or  ?all=1 (every idle scene in turn)
        if (!authorized()) return;
        bool ok = srv->hasArg("all") ? screensPreviewScene(nullptr) : screensPreviewScene(srv->arg("name").c_str());
        srv->send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"scene\"}");
    });
    server.on("/api/wifi-test-fail", HTTP_POST, []() {  // test: pretend the built-in Wi-Fi password is wrong
        if (!authorized()) return;
        ok();
        srv->client().flush();
        delay(100);
        netSimulateBadWifi();
    });
    server.on("/", HTTP_GET, []() {
        if (portalRedirect()) return;  // phone on the setup AP -> setup page
        srv->sendHeader("Location", "/status");
        srv->send(302);
    });
    updater.setup(&server, "/update", "clock", deviceToken());
}
