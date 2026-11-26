#include "info.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "i18n.h"
#include "model.h"

static bool numberAfter(const std::string &s, const char *key, double &out) {
    size_t i = s.find(key);
    if (i == std::string::npos) return false;
    const char *p = s.c_str() + i + strlen(key);
    char *end = nullptr;
    out = strtod(p, &end);
    return end != p;
}

bool parseOwm(const std::string &json, WeatherNow &out) {
    double id, temp, wind = 0, feels;
    if (!numberAfter(json, "\"weather\":[{\"id\":", id) || !numberAfter(json, "\"temp\":", temp)) return false;
    numberAfter(json, "\"speed\":", wind);
    if (!numberAfter(json, "\"feels_like\":", feels)) feels = temp;
    out.id = int(id);
    out.temp = int(lround(temp));
    out.wind = int(lround(wind));
    out.feels = int(lround(feels));
    size_t ic = json.find("\"icon\":\"");
    out.night = ic != std::string::npos && json.compare(ic + 10, 1, "n") == 0;
    return true;
}

Weather weatherKind(const WeatherNow &w) {
    int g = w.id / 100;
    if (g == 2) return Weather::Storm;
    if (g == 3 || g == 5) return Weather::Rain;
    if (g == 6) return Weather::Snow;
    if (g == 7) return Weather::Fog;
    if (w.wind >= 10) return Weather::Wind;
    if (w.temp >= 28) return Weather::Hot;
    return Weather::Clear;
}

std::string weatherText(const WeatherNow &w) {
    int g = w.id / 100;
    Txt t = g == 2 ? T_W_STORM : g == 3 ? T_W_DRIZZLE : g == 5 ? T_W_RAIN : g == 6 ? T_W_SNOW : g == 7 ? T_W_FOG
          : w.id == 800 ? T_W_CLEAR : T_W_CLOUDS;
    char b[16];
    snprintf(b, sizeof b, w.temp > 0 ? "+%d° " : "%d° ", w.temp);
    return std::string(b) + tr(t);
}

static std::string deg(int t) {
    char b[12];
    snprintf(b, sizeof b, t > 0 ? "+%d°" : "%d°", t);
    return b;
}

std::string weatherDetail(const WeatherNow &w) {
    char b[16];
    snprintf(b, sizeof b, " %d ", w.wind);
    return std::string(tr(T_FEELS)) + " " + deg(w.feels) + " ·" + b + tr(T_MPS);
}

InfoIcon weatherIcon(const WeatherNow &w) {
    int g = w.id / 100;
    if (g == 2) return InfoIcon::Storm;
    if (g == 3 || g == 5) return InfoIcon::Rain;
    if (g == 6) return InfoIcon::Snow;
    if (g == 7) return InfoIcon::Fog;
    if (w.id == 800) return w.night ? InfoIcon::Moon : InfoIcon::Sun;
    if (w.id <= 802) return w.night ? InfoIcon::MoonCloud : InfoIcon::SunCloud;
    return InfoIcon::Cloud;
}

std::string jsonKey(const std::string &s) {
    std::string o = "\"";
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        uint32_t cp;
        int n;
        if (c < 0x80) { o += char(c); i++; continue; }
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 2; }
        else { i++; continue; }  // 4-byte / broken: not in region names
        for (int k = 1; k <= n && i + k < s.size(); k++) cp = (cp << 6) | (uint8_t(s[i + k]) & 0x3F);
        i += n + 1;
        char b[8];
        snprintf(b, sizeof b, "\\u%04x", unsigned(cp));
        o += b;
    }
    return o + "\":";
}

int parseAlertTail(const std::string &tail) {
    size_t i = tail.find("\"alertnow\":");
    if (i == std::string::npos || i > 40) return -1;
    i += 11;
    if (tail.compare(i, 4, "true") == 0) return 1;
    if (tail.compare(i, 5, "false") == 0) return 0;
    return -1;
}

bool parseAlerts(const std::string &json, const std::string &region, bool &active) {
    size_t i = json.find(jsonKey(region));
    if (i == std::string::npos) return false;
    int r = parseAlertTail(json.substr(i + jsonKey(region).size(), 80));
    if (r < 0) return false;
    active = r == 1;
    return true;
}


void Info::onWeather(const WeatherNow &w, uint32_t now) { wx_ = w; wxAt_ = now; hasWx_ = true; }

void Info::onAlert(bool active, uint32_t now) { ubOn_ = active; ubAt_ = now; hasUb_ = true; track(now); }

void Info::onAlertLevel(AlertLevel l, uint32_t now) { mac_ = l; macAt_ = now; hasMac_ = true; track(now); }

void Info::day(int dayKey) {
    if (dayKey == day_) return;
    day_ = dayKey;
    busyMs_ = 0;
    answers_ = commits_ = 0;
}

void Info::onBusy(bool busy, uint32_t now, int dayKey) {
    day(dayKey);
    uint32_t dt = now - lastBusyCall_;
    if (prevBusy_ && busyCalled_ && dt <= 60000) busyMs_ += dt;  // the interval counts by the state at its start
    lastBusyCall_ = now;
    busyCalled_ = true;
    prevBusy_ = busy;
}

void Info::onAnswer(int dayKey) { day(dayKey); answers_++; }
void Info::onCommit(int dayKey) { day(dayKey); commits_++; }

static const char *commitWord(int n) {
    if (lang() == Lang::En) return n == 1 ? tr(T_COMMIT1) : tr(T_COMMIT2);
    int d = n % 10, h = n % 100;
    if (d == 1 && h != 11) return tr(T_COMMIT1);
    if (d >= 2 && d <= 4 && (h < 12 || h > 14)) return tr(T_COMMIT2);
    return tr(T_COMMIT5);
}

AlertLevel Info::alert(uint32_t now) const {
    if (hasMac_ && now - macAt_ < MAC_LEVEL_MS) return mac_;
    if (hasUb_ && now - ubAt_ < ALERT_FRESH_MS && ubOn_) return AlertLevel::Alert;
    return AlertLevel::None;
}

void Info::track(uint32_t now) {
    bool on = alert(now) != AlertLevel::None;
    if (on && !on_) { since_ = now; ended_ = false; }
    if (!on && on_) { endedAt_ = now; ended_ = true; }
    on_ = on;
}

Info::Card Info::card(uint32_t now, int dayKey) const {
    AlertLevel a = alert(now);
    Card c;
    if (a != AlertLevel::None) {
        Txt t = a == AlertLevel::Drone ? T_ALERT_DRONE : a == AlertLevel::Missile ? T_ALERT_MISSILE : T_ALERT;
        c.icon = InfoIcon::Siren;
        c.line1 = std::string(tr(t)) + " " + resetShort(long((now - since_) / 1000), false);
        c.line2 = region_;
        size_t o = c.line2.find(" область");  // "Дніпропетровська область" -> "Дніпропетровська обл."
        if (o != std::string::npos) c.line2 = c.line2.substr(0, o) + " обл.";
        c.tone = a == AlertLevel::Drone ? 1 : 2;
        return c;
    }
    bool wx = hasWeather(now), today = day_ == dayKey && (answers_ > 0 || commits_ > 0 || busyMs_ >= 60000);
    c.items = uint8_t(wx + today);
    if (!c.items) return c;
    c.index = uint8_t((now / CARD_STEP_MS) % c.items);
    if (wx && c.index == 0) {
        c.icon = weatherIcon(wx_);
        c.line1 = weatherText(wx_);
        c.line2 = weatherDetail(wx_);
        return c;
    }
    c.icon = InfoIcon::Claude;
    std::string t = resetShort(long(busyMs_ / 1000), false);
    c.line1 = std::string(tr(T_TODAY)) + " " + t;
    char b[48];
    snprintf(b, sizeof b, "%d %s · %d %s", answers_, tr(T_ANSWERS), commits_, commitWord(commits_));
    c.line2 = b;
    return c;
}
