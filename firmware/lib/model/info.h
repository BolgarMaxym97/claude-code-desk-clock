// Stage 2 info: weather (OpenWeather, fetched by the clock), air-raid alert (ubilling, fetched by the clock;
// level from alerts.in.ua via the Mac), tokens used today (from the Mac) and the one-line info card. Pure logic.
#pragma once
#include <stdint.h>
#include <string>
#include "scene.h"

enum class AlertLevel : uint8_t { None, Alert, Drone, Missile };  // Alert: on, level unknown (ubilling)

struct WeatherNow {
    int id = 0;          // OpenWeather condition id (2xx storm ... 800 clear, 80x clouds)
    int temp = 0;        // °C, rounded
    int wind = 0;        // m/s, rounded
    int feels = 0;       // °C, rounded
    bool night = false;  // OpenWeather icon "..n"
};

enum class InfoIcon : uint8_t { None, Sun, Moon, SunCloud, MoonCloud, Cloud, Rain, Snow, Storm, Fog, Claude, Siren, Star, Calendar };

// A meeting from the computer's calendar (epoch seconds; hm = local start "14:00"; title "" = hidden).
struct Meeting {
    uint32_t start, end;
    std::string title, hm;
};

bool parseOwm(const std::string &json, WeatherNow &out);  // false: error answer / missing fields
Weather weatherKind(const WeatherNow &w);                  // for the weather scenes
std::string weatherText(const WeatherNow &w);              // "+3° cloudy" / "+3° хмарно"
std::string weatherDetail(const WeatherNow &w);            // "feels -4° · wind 5 m/s"
InfoIcon weatherIcon(const WeatherNow &w);

// ubilling keys are JSON-escaped ("м. К..."): the exact text to look for, quotes and colon included.
std::string jsonKey(const std::string &utf8);
int parseAlertTail(const std::string &tail);  // what follows the key: 1 alert, 0 none, -1 unreadable
bool parseAlerts(const std::string &json, const std::string &region, bool &active);  // false: region not found


// "Today with Claude" counters, kept across restarts by the device (RTC memory).
struct TodaySnap {
    int32_t day;
    uint32_t busyMs;
    int32_t answers, commits;
};

class Info {
  public:
    static constexpr uint32_t WEATHER_FRESH_MS = 3600000, ALERT_FRESH_MS = 300000, MAC_LEVEL_MS = 180000,
                              ENDED_SHOW_MS = 600000, CARD_STEP_MS = 5000;
    void onWeather(const WeatherNow &w, uint32_t now);
    void onAlert(bool active, uint32_t now);              // ubilling answer for the region
    void onAlertLevel(AlertLevel l, uint32_t now);        // from the Mac (alerts.in.ua): wins while fresh
    // "Today with Claude" (dayKey = month * 32 + day; a new day starts from zero). onBusy is called about once a
    // second: the time since the previous call counts when Claude was busy (gaps over a minute do not count).
    void onBusy(bool busy, uint32_t now, int dayKey);
    void onAnswer(int dayKey);
    void onCommit(int dayKey);
    void setRegion(const std::string &r) { region_ = r; }
    static constexpr int MAX_MEETINGS = 8, SOON_MIN = 10;
    void onMeetings(const Meeting *m, int n);           // replaces the list (n = 0: calendar off / nothing ahead)
    int meetingSoonMin(uint32_t epoch) const;           // minutes to the next meeting when <= 10, else -1
    bool inMeeting(uint32_t epoch) const;
    TodaySnap today() const { return TodaySnap{day_, busyMs_, answers_, commits_}; }
    void restore(const TodaySnap &t) { day_ = t.day; busyMs_ = t.busyMs; answers_ = t.answers; commits_ = t.commits; }
    void alertsOff() { hasUb_ = hasMac_ = on_ = ended_ = false; }  // region cleared: no alert, no all-clear

    bool hasWeather(uint32_t now) const { return hasWx_ && now - wxAt_ < WEATHER_FRESH_MS; }
    const WeatherNow &weatherNow() const { return wx_; }
    Weather weather(uint32_t now) const { return hasWeather(now) ? weatherKind(wx_) : Weather::Unknown; }
    AlertLevel alert(uint32_t now) const;
    bool alertEnded(uint32_t now) const { return ended_ && now - endedAt_ < ENDED_SHOW_MS; }

    struct Card {
        InfoIcon icon = InfoIcon::None;
        std::string line1, line2;  // line1 empty: no card
        uint8_t tone = 0;          // 0 normal, 1 yellow (drones), 2 red (alert / missiles)
        uint8_t items = 0, index = 0;  // rotation dots: how many, which one is shown (0 items: no dots)
    };
    Card card(uint32_t now, int dayKey, uint32_t epoch = 0) const;  // epoch 0: time unknown (no meetings)

  private:
    void track(uint32_t now);  // alert start / end from the effective level
    WeatherNow wx_;
    uint32_t wxAt_ = 0, ubAt_ = 0, macAt_ = 0, since_ = 0, endedAt_ = 0;
    bool hasWx_ = false, hasUb_ = false, hasMac_ = false, ubOn_ = false, on_ = false, ended_ = false;
    AlertLevel mac_ = AlertLevel::None;
    void day(int dayKey);
    int day_ = -1;
    uint32_t busyMs_ = 0, lastBusyCall_ = 0;
    bool busyCalled_ = false, prevBusy_ = false;
    int answers_ = 0, commits_ = 0;
    std::string region_;
    Meeting meetings_[MAX_MEETINGS];
    int nMeetings_ = 0;
    const Meeting *nextMeeting(uint32_t epoch) const;  // ongoing or next, nullptr: none
};
