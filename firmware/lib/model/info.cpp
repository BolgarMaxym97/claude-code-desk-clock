#include "info.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "i18n.h"
#include "model.h"
#include "settings.h"

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

int32_t daysFromCivil(int y, int m, int d) {  // H. Hinnant's algorithm
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void WeekLog::set(int32_t day, uint32_t busyMs, int answers, int commits) {
    if (last_ < 0 || day < last_ - DAYS) *this = WeekLog{}, last_ = day;  // first day, or a wrong date was saved once
    if (day > last_) {  // move the window forward, new days start empty
        int32_t shift = day - last_;
        for (int i = 0; i < DAYS; i++) d_[i] = i + shift < DAYS ? d_[i + shift] : Day{};
        last_ = day;
    }
    int i = DAYS - 1 - int(last_ - day);
    if (i < 0) return;  // older than the log
    auto cap = [](long v, long m) { return uint16_t(v < 0 ? 0 : v > m ? m : v); };
    d_[i] = Day{cap(long(busyMs / 60000), 1440), cap(answers, 9999), cap(commits, 999)};
}

WeekLog::Sum WeekLog::lastWeek(int32_t today) const {
    Sum s{0, 0, 0};
    for (int i = 0; i < DAYS && last_ >= 0; i++) {
        int32_t day = last_ - (DAYS - 1 - i);
        if (day < today - 7 || day > today - 1) continue;
        s.busyMin += d_[i].busyMin;
        s.answers += d_[i].answers;
        s.commits += d_[i].commits;
    }
    return s;
}

std::string WeekLog::encode() const {
    if (last_ < 0) return "";
    char b[24];
    snprintf(b, sizeof b, "%ld:", long(last_));
    std::string s = b;
    for (int i = 0; i < DAYS; i++) {
        snprintf(b, sizeof b, "%s%u,%u,%u", i ? ";" : "", d_[i].busyMin, d_[i].answers, d_[i].commits);
        s += b;
    }
    return s;
}

bool WeekLog::decode(const std::string &s) {
    if (!validWeek(s)) return false;
    WeekLog w;
    w.last_ = atol(s.c_str());
    const char *p = strchr(s.c_str(), ':') + 1;
    int n = 0;
    Day tmp[DAYS] = {};
    while (*p && n < DAYS) {
        unsigned b, a, c;
        if (sscanf(p, "%u,%u,%u", &b, &a, &c) != 3) return false;
        tmp[n++] = Day{uint16_t(b), uint16_t(a), uint16_t(c)};
        p = strchr(p, ';');
        if (!p) break;
        p++;
    }
    for (int i = 0; i < n; i++) w.d_[DAYS - n + i] = tmp[i];  // fewer days: the newest are at the end
    *this = w;
    return true;
}

void Info::onMessage(const std::string &title, const std::string &detail, uint32_t now, uint32_t forMs) {
    msg_ = forMs ? title : "";
    msgDetail_ = msg_.empty() ? "" : detail;
    msgAt_ = now;
    msgFor_ = forMs;
}

std::string forecastHour(int hour) { return std::to_string(hour) + (lang() == Lang::En ? "h" : ""); }

Weather precipOf(int id) {
    int g = id / 100;
    return g == 2 ? Weather::Storm : (g == 3 || g == 5) ? Weather::Rain : g == 6 ? Weather::Snow : Weather::Unknown;
}

void Info::onForecast(Weather kind, uint32_t atEpoch, const std::string &hm, uint32_t now) {
    fcKind_ = kind;
    fcAt_ = atEpoch;
    fcRx_ = now;
    fcHour_ = hm.size() >= 2 ? atoi(hm.c_str()) : -1;
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

int parseAlertTail(const std::string &tail, std::string *changed) {
    size_t i = tail.find("\"alertnow\":");
    if (i == std::string::npos || i > 40) return -1;
    i += 11;
    int r = tail.compare(i, 4, "true") == 0 ? 1 : tail.compare(i, 5, "false") == 0 ? 0 : -1;
    if (changed) {
        changed->clear();
        size_t c = tail.find("\"changed\":\"");
        if (c != std::string::npos) {
            size_t e = tail.find('"', c + 11);
            if (e != std::string::npos) *changed = tail.substr(c + 11, e - c - 11);
        }
    }
    return r;
}

bool parseStamp(const std::string &s, int &y, int &mo, int &d, int &h, int &mi, int &se) {
    return s.size() == 19 && sscanf(s.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &y, &mo, &d, &h, &mi, &se) == 6 && mo >= 1 &&
           mo <= 12 && d >= 1 && d <= 31 && h < 24 && mi < 60 && se < 61;
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

void Info::onAlert(bool active, uint32_t now, uint32_t sinceEpoch) {
    ubOn_ = active;
    ubAt_ = now;
    hasUb_ = true;
    if (!active) srcSince_ = 0;
    else if (sinceEpoch && !srcSince_) srcSince_ = sinceEpoch;  // kept for the whole alert
    track(now);
}

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

void Info::onMeetings(const Meeting *m, int n) {
    nMeetings_ = 0;
    for (int i = 0; i < n && nMeetings_ < MAX_MEETINGS; i++)
        if (m[i].end > m[i].start) meetings_[nMeetings_++] = m[i];
}

const Meeting *Info::nextMeeting(uint32_t epoch) const {
    if (!epoch) return nullptr;
    const Meeting *best = nullptr;
    for (int i = 0; i < nMeetings_; i++)
        if (meetings_[i].end > epoch && (!best || meetings_[i].start < best->start)) best = &meetings_[i];
    return best;
}

bool Info::inMeeting(uint32_t epoch) const {
    const Meeting *m = nextMeeting(epoch);
    return m && m->start <= epoch;
}

int Info::meetingSoonMin(uint32_t epoch) const {
    const Meeting *m = nextMeeting(epoch);
    if (!m || m->start <= epoch) return -1;
    int min = int((m->start - epoch + 59) / 60);
    return min <= SOON_MIN ? min : -1;
}

static std::string minutes(uint32_t secs) { return resetShort(long((secs + 59) / 60 * 60), false); }

static const char *commitWord(int n) {
    if (lang() == Lang::En) return n == 1 ? tr(T_COMMIT1) : tr(T_COMMIT2);
    int d = n % 10, h = n % 100;
    if (d == 1 && h != 11) return tr(T_COMMIT1);
    if (d >= 2 && d <= 4 && (h < 12 || h > 14)) return tr(T_COMMIT2);
    return tr(T_COMMIT5);
}

bool Info::alertsUnknown(uint32_t now) const {
    if (region_.empty() || now < ALERT_FRESH_MS) return false;
    bool ub = hasUb_ && now - ubAt_ < ALERT_FRESH_MS, mac = hasMac_ && now - macAt_ < MAC_LEVEL_MS;
    return !ub && !mac;
}

AlertLevel Info::alert(uint32_t now) const {
    if (hasMac_ && now - macAt_ < MAC_LEVEL_MS) return mac_;
    if (hasUb_ && now - ubAt_ < ALERT_FRESH_MS && ubOn_) return AlertLevel::Alert;
    return AlertLevel::None;
}

void Info::track(uint32_t now) {
    bool on = isAlert(alert(now));
    if (on && !on_) { since_ = now; ended_ = false; }
    if (!on && on_) { endedAt_ = now; ended_ = true; }
    on_ = on;
}

Info::Card Info::card(uint32_t now, int dayKey, uint32_t epoch) const {
    AlertLevel a = alert(now);
    Card c;
    if (a == AlertLevel::Threat) {  // yellow level: no duration, the alert may not come
        c.icon = InfoIcon::Siren;
        c.line1 = tr(T_ALERT_THREAT);
        c.line2 = region_;
        size_t o = c.line2.find(" область");
        if (o != std::string::npos) c.line2 = c.line2.substr(0, o) + " обл.";
        c.tone = 5;
        return c;
    }
    if (a != AlertLevel::None) {
        Txt t = a == AlertLevel::Drone ? T_ALERT_DRONE : a == AlertLevel::Missile ? T_ALERT_MISSILE
              : a == AlertLevel::Partial ? T_ALERT_PARTIAL : T_ALERT;
        c.icon = InfoIcon::Siren;
        long secs = epoch && srcSince_ && srcSince_ <= epoch ? long(epoch - srcSince_) : long((now - since_) / 1000);
        c.line1 = std::string(tr(t)) + " " + resetShort(secs, secs >= 86400);  // a day or more: in days
        c.line2 = region_;
        size_t o = c.line2.find(" область");  // "Дніпропетровська область" -> "Дніпропетровська обл."
        if (o != std::string::npos) c.line2 = c.line2.substr(0, o) + " обл.";
        c.tone = a == AlertLevel::Drone ? 1 : a == AlertLevel::Partial ? 6 : 2;
        return c;
    }
    if (!msg_.empty() && now - msgAt_ < msgFor_) {  // after an alert, before everything else
        c.icon = InfoIcon::Message;
        c.line1 = msg_;
        c.line2 = msgDetail_;
        c.tone = 4;
        return c;
    }
    const Meeting *m = nextMeeting(epoch);
    if (m) {
        c.icon = InfoIcon::Calendar;
        c.line2 = m->title.empty() ? tr(T_MEETING) : m->title;
        if (m->start <= epoch) {  // during the meeting: pinned
            std::string left = minutes(m->end - epoch);
            c.line1 = std::string(tr(T_NOW)) + " · " +
                      (lang() == Lang::En ? left + " " + tr(T_LEFT) : std::string(tr(T_LEFT)) + " " + left);
            c.tone = 3;
            return c;
        }
        uint32_t ahead = m->start - epoch;
        c.line1 = ahead <= 3600 ? std::string(tr(T_IN_TIME)) + " " + minutes(ahead) : std::string(tr(T_AT)) + " " + m->hm;
        if (ahead <= uint32_t(SOON_MIN) * 60) {  // soon: pinned
            c.tone = 3;
            return c;
        }
    }
    bool wx = hasWeather(now), meet = m != nullptr,
         today = day_ == dayKey && (answers_ > 0 || commits_ > 0 || busyMs_ >= 60000);
    WeekLog::Sum ws = week_.lastWeek(dayNum_);
    bool week = wday_ == 1 && dayNum_ >= 0 && (ws.answers > 0 || ws.busyMin > 0);  // Mondays only
    c.items = uint8_t(wx + meet + today + week);
    if (!c.items) return Card{};
    c.index = uint8_t((now / CARD_STEP_MS) % c.items);
    int k = c.index;
    if (wx && k-- == 0) {
        Card w;
        w.items = c.items;
        w.index = c.index;
        w.icon = weatherIcon(wx_);
        w.line1 = weatherText(wx_);
        w.line2 = weatherDetail(wx_);
        bool fc = fcKind_ != Weather::Unknown && fcHour_ >= 0 && epoch && fcAt_ > epoch && now - fcRx_ < FORECAST_FRESH_MS &&
                  precipOf(wx_.id) != fcKind_;  // not "rain at" while it already rains
        if (fc)  // keep "feels", drop the wind: "feels +1° · rain at 18:00"
            w.line2 = std::string(tr(T_FEELS_SHORT)) + " " + deg(wx_.feels) + " · " +
                      tr(fcKind_ == Weather::Snow ? T_FC_SNOW : fcKind_ == Weather::Storm ? T_FC_STORM : T_FC_RAIN) + forecastHour(fcHour_);
        return w;
    }
    if (meet && k-- == 0) return c;
    if (today) k--;
    if (week && k == 0) {
        c.icon = InfoIcon::Star;
        c.line1 = std::string(tr(T_LAST_WEEK)) + " " + std::to_string((ws.busyMin + 30) / 60) + tr(T_HOURS_SHORT);
        char b[48];
        snprintf(b, sizeof b, "%d %s · %d %s", ws.answers, tr(T_ANSWERS), ws.commits, commitWord(ws.commits));
        c.line2 = b;
        return c;
    }
    c.line2.clear();
    c.icon = InfoIcon::Claude;
    std::string t = resetShort(long(busyMs_ / 1000), false);
    c.line1 = std::string(tr(T_TODAY)) + " " + t;
    char b[48];
    snprintf(b, sizeof b, "%d %s · %d %s", answers_, tr(T_ANSWERS), commits_, commitWord(commits_));
    c.line2 = b;
    return c;
}
