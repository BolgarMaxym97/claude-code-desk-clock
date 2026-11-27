#include "fetch.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include "device.h"
#include "net.h"
#include <time.h>

static Info info;
static uint32_t nextWx = 10000, nextAlert = 5000, nextFc = 20000;  // first fetch a few seconds after boot
static const uint32_t WX_EVERY = 15u * 60000u, WX_RETRY = 5u * 60000u, ALERT_EVERY = 30000, ALERT_RETRY = 60000,
                      FC_EVERY = 60u * 60000u, FC_RETRY = 10u * 60000u;
static const uint16_t TIMEOUT_MS = 2500;
static FetchDiag diag;
const FetchDiag &fetchDiag() { return diag; }

Info &infoState() { return info; }

void fetchSoon() {  // settings changed; 3 s later: claude-clock has just asked the same source (limit 2 req/s per address)
    uint32_t now = millis();
    nextWx = nextAlert = now + 3000;
    nextFc = now + 6000;
}

static bool due(uint32_t now, uint32_t at) { return int32_t(now - at) >= 0; }

static void fetchWeather(uint32_t now) {
    String loc = deviceLoc();
    int c = loc.indexOf(',');
    WiFiClient cl;
    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);
    String url = String("http://api.openweathermap.org/data/2.5/weather?units=metric&lat=") + loc.substring(0, c) +
                 "&lon=" + loc.substring(c + 1) + "&appid=" + deviceOwm();
    bool ok = false;
    int code = http.begin(cl, url) ? http.GET() : -1;
    diag.wxCode = code;
    if (code == 200) {
        String body = http.getString();  // ~500 bytes
        WeatherNow w;
        if (parseOwm(body.c_str(), w)) { info.onWeather(w, now); ok = true; }
    }
    http.end();
    nextWx = now + (ok ? WX_EVERY : WX_RETRY);
}

// Next 12 h (4 steps of 3 h): the first step with rain / snow / a storm -> "rain at 18:00" on the weather card.
static void fetchForecast(uint32_t now) {
    String loc = deviceLoc();
    int c = loc.indexOf(',');
    WiFiClient cl;
    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);
    http.useHTTP10(true);  // no chunked answer: the stream goes straight into the JSON parser
    String url = String("http://api.openweathermap.org/data/2.5/forecast?units=metric&cnt=4&lat=") + loc.substring(0, c) +
                 "&lon=" + loc.substring(c + 1) + "&appid=" + deviceOwm();
    bool ok = false;
    int code = http.begin(cl, url) ? http.GET() : -1;
    diag.fcCode = code;
    if (code == 200) {
        JsonDocument filter;  // keep only the step times and weather ids (~2.5 KB answer -> ~200 bytes)
        filter["list"][0]["dt"] = true;
        filter["list"][0]["weather"][0]["id"] = true;
        JsonDocument doc;
        if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
            Weather kind = Weather::Unknown;
            uint32_t at = 0;
            for (JsonObject step : doc["list"].as<JsonArray>()) {
                Weather p = precipOf(step["weather"][0]["id"] | 0);
                if (p != Weather::Unknown) { kind = p; at = step["dt"] | 0u; break; }
            }
            char hm[6] = "";
            if (at) {
                time_t t = at;
                struct tm lt;
                localtime_r(&t, &lt);
                snprintf(hm, sizeof hm, "%02d:%02d", lt.tm_hour, lt.tm_min);
            }
            info.onForecast(kind, at, hm, now);
            ok = true;
        }
    }
    http.end();
    nextFc = now + (ok ? FC_EVERY : FC_RETRY);
}

static void fetchAlert(uint32_t now) {
    WiFiClient cl;
    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);
    bool ok = false;
    int code = http.begin(cl, "http://ubilling.net.ua/aerialalerts/") ? http.GET() : -1;
    diag.alertCode = code;
    if (code == 200) {
        // ~4 KB of 26 regions: stream to our region's key instead of keeping the whole body in RAM
        WiFiClient &s = http.getStream();
        s.setTimeout(TIMEOUT_MS);
        std::string key = jsonKey(deviceRegion());
        if (s.find(key.c_str())) {
            char tail[96];  // {"alertnow":true,"changed":"2026-11-26 13:12:47"}
            size_t n = s.readBytes(tail, sizeof tail - 1);
            tail[n] = 0;
            std::string changed;
            int r = parseAlertTail(tail, &changed);
            uint32_t since = 0;
            struct tm t {};
            if (parseStamp(changed, t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)) {
                t.tm_year -= 1900;  // the source's local time (Kyiv) = the clock's time zone in Ukraine
                t.tm_mon -= 1;
                t.tm_isdst = -1;
                time_t e = mktime(&t);
                if (e > 1600000000) since = uint32_t(e);
            }
            if (r >= 0) { info.onAlert(r == 1, now, since); ok = true; }
        }
    }
    http.end();
    nextAlert = now + (ok ? ALERT_EVERY : ALERT_RETRY);
}

extern bool safeBoot;

void fetchTick(uint32_t now) {
    // safe mode (crash loop): no outside data at all, the parsers are the riskiest code
    if (safeBoot || WiFi.status() != WL_CONNECTED || netSetupMode()) return;
    if (*deviceRegion() && due(now, nextAlert)) { fetchAlert(now); return; }  // one request per loop
    if (*deviceOwm() && *deviceLoc() && due(now, nextWx)) { fetchWeather(now); return; }
    if (*deviceOwm() && *deviceLoc() && due(now, nextFc)) fetchForecast(now);
}
