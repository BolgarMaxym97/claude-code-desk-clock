#include "fetch.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include "device.h"
#include "net.h"

static Info info;
static uint32_t nextWx = 10000, nextAlert = 5000;  // first fetch a few seconds after boot
static const uint32_t WX_EVERY = 15u * 60000u, WX_RETRY = 5u * 60000u, ALERT_EVERY = 30000, ALERT_RETRY = 60000;
static const uint16_t TIMEOUT_MS = 2500;

Info &infoState() { return info; }

void fetchSoon() {
    uint32_t now = millis();
    nextWx = nextAlert = now;
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
    if (http.begin(cl, url) && http.GET() == 200) {
        String body = http.getString();  // ~500 bytes
        WeatherNow w;
        if (parseOwm(body.c_str(), w)) { info.onWeather(w, now); ok = true; }
    }
    http.end();
    nextWx = now + (ok ? WX_EVERY : WX_RETRY);
}

static void fetchAlert(uint32_t now) {
    WiFiClient cl;
    HTTPClient http;
    http.setTimeout(TIMEOUT_MS);
    bool ok = false;
    if (http.begin(cl, "http://ubilling.net.ua/aerialalerts/") && http.GET() == 200) {
        // ~4 KB of 26 regions: stream to our region's key instead of keeping the whole body in RAM
        WiFiClient &s = http.getStream();
        s.setTimeout(TIMEOUT_MS);
        std::string key = jsonKey(deviceRegion());
        if (s.find(key.c_str())) {
            char tail[64];
            size_t n = s.readBytes(tail, sizeof tail - 1);
            tail[n] = 0;
            int r = parseAlertTail(tail);
            if (r >= 0) { info.onAlert(r == 1, now); ok = true; }
        }
    }
    http.end();
    nextAlert = now + (ok ? ALERT_EVERY : ALERT_RETRY);
}

void fetchTick(uint32_t now) {
    if (WiFi.status() != WL_CONNECTED || netSetupMode()) return;
    if (*deviceRegion() && due(now, nextAlert)) { fetchAlert(now); return; }  // one request per loop
    if (*deviceOwm() && *deviceLoc() && due(now, nextWx)) fetchWeather(now);
}
