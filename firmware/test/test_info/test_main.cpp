// Stage 2 info: weather (OpenWeather), air-raid alert (ubilling / alerts.in.ua), tokens today, the info card.
#include <unity.h>
#include "info.h"
#include "settings.h"
#include "i18n.h"

void setUp() { setLang(Lang::En); }
void tearDown() { setLang(Lang::En); }

static const char *OWM =
    "{\"coord\":{\"lon\":30.52,\"lat\":50.45},\"weather\":[{\"id\":803,\"main\":\"Clouds\",\"description\":\"broken\"}],"
    "\"main\":{\"temp\":2.6,\"feels_like\":-1.2,\"humidity\":80},\"wind\":{\"speed\":4.6,\"deg\":200},\"cod\":200}";

void test_owm_parses_id_temp_wind() {
    WeatherNow w;
    TEST_ASSERT_TRUE(parseOwm(OWM, w));
    TEST_ASSERT_EQUAL(803, w.id);
    TEST_ASSERT_EQUAL(3, w.temp);
    TEST_ASSERT_EQUAL(5, w.wind);
    std::string neg = "{\"weather\":[{\"id\":600}],\"main\":{\"temp\":-7.5},\"wind\":{\"speed\":1}}";
    TEST_ASSERT_TRUE(parseOwm(neg, w));
    TEST_ASSERT_EQUAL(-8, w.temp);
}

void test_owm_errors_are_rejected() {
    WeatherNow w;
    TEST_ASSERT_FALSE(parseOwm("{\"cod\":401,\"message\":\"Invalid API key\"}", w));
    TEST_ASSERT_FALSE(parseOwm("", w));
    TEST_ASSERT_FALSE(parseOwm("{\"weather\":[{\"id\":800}]}", w));  // no temperature
}

void test_weather_kind_for_scenes() {
    TEST_ASSERT_EQUAL(int(Weather::Storm), int(weatherKind(WeatherNow{211, 15, 3, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Rain), int(weatherKind(WeatherNow{301, 10, 3, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Rain), int(weatherKind(WeatherNow{502, 10, 3, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Snow), int(weatherKind(WeatherNow{601, -2, 3, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Fog), int(weatherKind(WeatherNow{741, 5, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Clear), int(weatherKind(WeatherNow{800, 20, 2, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Hot), int(weatherKind(WeatherNow{800, 31, 2, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Wind), int(weatherKind(WeatherNow{803, 12, 12, 0, false})));
    TEST_ASSERT_EQUAL(int(Weather::Clear), int(weatherKind(WeatherNow{803, 12, 3, 0, false})));  // clouds: no special scene
}

void test_weather_text_both_languages() {
    TEST_ASSERT_EQUAL_STRING("+3° cloudy", weatherText(WeatherNow{803, 3, 5, 0, false}).c_str());
    TEST_ASSERT_EQUAL_STRING("-8° snow", weatherText(WeatherNow{600, -8, 1, 0, false}).c_str());
    TEST_ASSERT_EQUAL_STRING("0° clear", weatherText(WeatherNow{800, 0, 1, 0, false}).c_str());
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL_STRING("+3° хмарно", weatherText(WeatherNow{803, 3, 5, 0, false}).c_str());
    TEST_ASSERT_EQUAL_STRING("+12° гроза", weatherText(WeatherNow{202, 12, 5, 0, false}).c_str());
}

static const char *ALERTS =
    "{\"source\":\"x\",\"cachedat\":\"2026-11-26 13:12:47\",\"states\":{"
    "\"\\u0421\\u0435\\u0432\\u0430\\u0441\\u0442\\u043e\\u043f\\u043e\\u043b\\u044c\":{\"alertnow\":true,\"changed\":\"2022\"},"
    "\"\\u041a\\u0438\\u0457\\u0432\\u0441\\u044c\\u043a\\u0430 \\u043e\\u0431\\u043b\\u0430\\u0441\\u0442\\u044c\":{\"alertnow\":true,\"changed\":\"x\"},"
    "\"\\u043c. \\u041a\\u0438\\u0457\\u0432\":{\"alertnow\":false,\"changed\":\"2026\"}}}";

void test_json_key_escapes_like_the_api() {
    TEST_ASSERT_EQUAL_STRING("\"\\u043c. \\u041a\\u0438\\u0457\\u0432\":", jsonKey("м. Київ").c_str());
}

void test_alerts_by_region() {
    bool a = true;
    TEST_ASSERT_TRUE(parseAlerts(ALERTS, "м. Київ", a));
    TEST_ASSERT_FALSE(a);
    TEST_ASSERT_TRUE(parseAlerts(ALERTS, "Київська область", a));
    TEST_ASSERT_TRUE(a);
    TEST_ASSERT_FALSE(parseAlerts(ALERTS, "Атлантида", a));  // unknown region: no answer
    TEST_ASSERT_EQUAL(1, parseAlertTail("{\"alertnow\":true,\"changed\":\"x\"}"));
    TEST_ASSERT_EQUAL(0, parseAlertTail("{\"alertnow\":false}"));
    TEST_ASSERT_EQUAL(-1, parseAlertTail("{\"oops\":1}"));
}

static const uint32_t MIN = 60000;
static const int DAY = 11 * 32 + 26;

void test_weather_details_icon_and_night() {
    WeatherNow w;
    std::string j = "{\"weather\":[{\"id\":803,\"main\":\"Clouds\",\"icon\":\"04n\"}],\"main\":{\"temp\":0.2,"
                    "\"feels_like\":-4.4},\"wind\":{\"speed\":5.2}}";
    TEST_ASSERT_TRUE(parseOwm(j, w));
    TEST_ASSERT_EQUAL(-4, w.feels);
    TEST_ASSERT_TRUE(w.night);
    TEST_ASSERT_EQUAL_STRING("feels -4° · 5 m/s", weatherDetail(w).c_str());
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL_STRING("відчув. -4° · 5 м/с", weatherDetail(w).c_str());
    TEST_ASSERT_EQUAL(int(InfoIcon::Cloud), int(weatherIcon(w)));
    TEST_ASSERT_EQUAL(int(InfoIcon::Moon), int(weatherIcon(WeatherNow{800, 0, 1, 0, true})));
    TEST_ASSERT_EQUAL(int(InfoIcon::Sun), int(weatherIcon(WeatherNow{800, 0, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(InfoIcon::SunCloud), int(weatherIcon(WeatherNow{801, 0, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(InfoIcon::MoonCloud), int(weatherIcon(WeatherNow{802, 0, 1, 0, true})));
    TEST_ASSERT_EQUAL(int(InfoIcon::Rain), int(weatherIcon(WeatherNow{500, 0, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(InfoIcon::Snow), int(weatherIcon(WeatherNow{600, 0, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(InfoIcon::Storm), int(weatherIcon(WeatherNow{200, 0, 1, 0, false})));
    TEST_ASSERT_EQUAL(int(InfoIcon::Fog), int(weatherIcon(WeatherNow{741, 0, 1, 0, false})));
}

void test_rain_forecast_replaces_the_detail_line() {
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL(int(Weather::Unknown), int(precipOf(803)));
    TEST_ASSERT_EQUAL(int(Weather::Rain), int(precipOf(501)));
    TEST_ASSERT_EQUAL(int(Weather::Snow), int(precipOf(600)));
    TEST_ASSERT_EQUAL(int(Weather::Storm), int(precipOf(211)));
    Info in;
    in.onWeather(WeatherNow{803, 3, 2, 1, false}, 0);
    const uint32_t E = 1764000000;
    in.onForecast(Weather::Rain, E + 3 * 3600, "18:00", 0);
    TEST_ASSERT_EQUAL_STRING("відч. +1° · дощ о 18", in.card(0, 1, E).line2.c_str());
    TEST_ASSERT_EQUAL_STRING("відчув. +1° · 2 м/с", in.card(0, 1, E + 4 * 3600).line2.c_str());  // it already began
    in.onForecast(Weather::Snow, E + 3600, "15:00", 0);
    TEST_ASSERT_EQUAL_STRING("відч. +1° · сніг о 15", in.card(0, 1, E).line2.c_str());
    TEST_ASSERT_EQUAL_STRING("відчув. +1° · 2 м/с", in.card(0, 1, 0).line2.c_str());  // time unknown
    in.onWeather(WeatherNow{601, 0, 2, -3, false}, 1000);  // snowing now: no "snow at"
    TEST_ASSERT_EQUAL_STRING("відчув. -3° · 2 м/с", in.card(1000, 1, E).line2.c_str());
    in.onForecast(Weather::Unknown, 0, "", 2000);           // dry ahead
    in.onWeather(WeatherNow{803, 3, 2, 1, false}, 2000);
    TEST_ASSERT_EQUAL_STRING("відчув. +1° · 2 м/с", in.card(2000, 1, E).line2.c_str());
    setLang(Lang::En);
}

void test_today_counts_busy_time_answers_and_commits() {
    Info in;
    TEST_ASSERT_TRUE(in.card(0, DAY).line1.empty());  // nothing yet: no card
    for (uint32_t t = 0; t <= 20 * MIN; t += 1000) in.onBusy(t < 12 * MIN, t, DAY);  // busy 12 of 20 minutes
    in.onAnswer(DAY);
    in.onCommit(DAY);
    Info::Card c = in.card(20 * MIN, DAY);
    TEST_ASSERT_EQUAL(int(InfoIcon::Claude), int(c.icon));
    TEST_ASSERT_EQUAL_STRING("Today 12m", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("1 ans. · 1 commit", c.line2.c_str());
    for (uint32_t t = 20 * MIN; t <= 120 * MIN; t += 1000) in.onBusy(true, t, DAY);
    for (int i = 0; i < 26; i++) in.onAnswer(DAY);
    for (int i = 0; i < 3; i++) in.onCommit(DAY);
    setLang(Lang::Uk);
    c = in.card(120 * MIN, DAY);
    TEST_ASSERT_EQUAL_STRING("Сьогодні 1:52", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("27 відп. · 4 коміти", c.line2.c_str());
    for (int i = 0; i < 17; i++) in.onCommit(DAY);
    TEST_ASSERT_EQUAL_STRING("27 відп. · 21 коміт", in.card(120 * MIN, DAY).line2.c_str());
    in.onCommit(DAY + 1);  // a new day starts from zero
    TEST_ASSERT_EQUAL_STRING("0 відп. · 1 коміт", in.card(121 * MIN, DAY + 1).line2.c_str());
    TEST_ASSERT_TRUE(in.card(121 * MIN, DAY).line1.empty());  // stale day is never shown
}

void test_busy_gap_after_a_pause_is_not_counted() {
    Info in;
    in.onBusy(true, 0, DAY);
    in.onBusy(true, 10 * MIN, DAY);  // no call for 10 min (screen off, Wi-Fi setup...): not counted
    in.onBusy(true, 10 * MIN + 1000, DAY);
    in.onAnswer(DAY);
    TEST_ASSERT_EQUAL_STRING("Today 0m", in.card(11 * MIN, DAY).line1.c_str());
}

void test_card_rotates_with_dots() {
    Info in;
    in.onWeather(WeatherNow{803, 3, 5, 1, false}, 0);
    Info::Card c = in.card(1000, DAY);
    TEST_ASSERT_EQUAL_STRING("+3° cloudy", c.line1.c_str());
    TEST_ASSERT_EQUAL(1, c.items);
    in.onAnswer(DAY);
    c = in.card(1000, DAY);
    TEST_ASSERT_EQUAL(2, c.items);
    TEST_ASSERT_EQUAL(0, c.index);
    c = in.card(6000, DAY);
    TEST_ASSERT_EQUAL(1, c.index);
    TEST_ASSERT_EQUAL_STRING("Today 0m", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("+3° cloudy", in.card(11000, DAY).line1.c_str());
    TEST_ASSERT_TRUE(in.card(61 * MIN, DAY + 1).line1.empty());  // old weather, other day: no card
    TEST_ASSERT_EQUAL(0, in.card(1000, DAY).tone);
}

void test_week_log_keeps_8_days_and_sums_last_week() {
    TEST_ASSERT_EQUAL(20418, daysFromCivil(2025, 11, 26));
    TEST_ASSERT_EQUAL(0, daysFromCivil(1970, 1, 1));
    WeekLog w;
    const int32_t MON = daysFromCivil(2026, 11, 30);  // a Monday
    for (int i = 7; i >= 1; i--) w.set(MON - i, 3600000u, 10, 2);  // Mon..Sun: 1 h, 10 answers, 2 commits a day
    w.set(MON - 8, 999999999u, 999, 99);                        // older than the log: ignored
    w.set(MON, 60000u, 1, 0);                                   // today does not count
    WeekLog::Sum s = w.lastWeek(MON);
    TEST_ASSERT_EQUAL(7 * 60, int(s.busyMin));
    TEST_ASSERT_EQUAL(70, s.answers);
    TEST_ASSERT_EQUAL(14, s.commits);
    WeekLog back;
    TEST_ASSERT_TRUE(back.decode(w.encode()));
    TEST_ASSERT_TRUE(validWeek(w.encode()));
    TEST_ASSERT_EQUAL(70, back.lastWeek(MON).answers);
    w.set(MON + 20, 0, 0, 0);                                   // long gap: old days drop out
    TEST_ASSERT_EQUAL(0, w.lastWeek(MON + 20).answers);
    TEST_ASSERT_FALSE(back.decode("x"));
    WeekLog bad;  // a date far in the future was saved once (a clock glitch): the log starts again from today
    bad.set(MON + 3000, 3600000u, 50, 5);
    bad.set(MON - 1, 3600000u, 10, 2);
    TEST_ASSERT_EQUAL(10, bad.lastWeek(MON).answers);
}

void test_monday_card_shows_last_week() {
    Info in;
    const int32_t MON = daysFromCivil(2026, 11, 30);
    for (int i = 7; i >= 1; i--) in.week().set(MON - i, 3 * 3600000u, 20, 4);
    in.setDay(MON, 1);
    in.onAnswer(DAY);
    Info::Card c = in.card(1000, DAY);
    TEST_ASSERT_EQUAL(2, c.items);                       // today + last week
    c = in.card(6000, DAY);
    TEST_ASSERT_EQUAL_STRING("Week 21h", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("140 ans. · 28 commits", c.line2.c_str());
    in.setDay(MON + 1, 2);                                // Tuesday: gone
    TEST_ASSERT_EQUAL(1, in.card(6000, DAY).items);
}

void test_message_takes_the_card_for_a_while() {
    Info in;
    in.onWeather(WeatherNow{803, 3, 5, 1, false}, 0);
    in.onMessage("Build done", "api · 4m 12s", 1000, 60000);
    Info::Card c = in.card(2000, DAY);
    TEST_ASSERT_EQUAL_STRING("Build done", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("api · 4m 12s", c.line2.c_str());
    TEST_ASSERT_EQUAL(int(InfoIcon::Message), int(c.icon));
    TEST_ASSERT_EQUAL(4, c.tone);              // green: pinned, no rotation dots
    TEST_ASSERT_EQUAL(0, c.items);
    TEST_ASSERT_EQUAL_STRING("+3° cloudy", in.card(61001, DAY).line1.c_str());  // time is up
    in.onMessage("Again", "", 70000, 60000);
    in.onMessage("", "", 71000, 0);            // cleared
    TEST_ASSERT_EQUAL_STRING("+3° cloudy", in.card(72000, DAY).line1.c_str());
    in.setRegion("x");
    in.onMessage("Hi", "", 80000, 60000);
    in.onAlert(true, 80000);
    TEST_ASSERT_EQUAL(2, in.card(81000, DAY).tone);  // an alert wins over a message
}

void test_alert_takes_the_card_and_counts_minutes() {
    Info in;
    in.setRegion("м. Київ");
    in.onWeather(WeatherNow{803, 3, 5, 1, false}, 0);
    in.onAlert(true, 10 * MIN);
    TEST_ASSERT_EQUAL(int(AlertLevel::Alert), int(in.alert(10 * MIN)));
    in.onAlert(true, 20 * MIN);  // polled every 30 s: still on, since stays
    Info::Card c = in.card(22 * MIN + 5000, DAY);
    TEST_ASSERT_EQUAL_STRING("Air raid alert 12m", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("м. Київ", c.line2.c_str());
    TEST_ASSERT_EQUAL(int(InfoIcon::Siren), int(c.icon));
    TEST_ASSERT_EQUAL(2, c.tone);
    TEST_ASSERT_EQUAL(0, c.items);  // no dots while the alert holds the card
    in.onAlert(true, 30 * MIN);
    TEST_ASSERT_EQUAL_STRING("Air raid alert 20m", in.card(30 * MIN, DAY).line1.c_str());
    setLang(Lang::Uk);
    in.onAlert(true, 74 * MIN);
    TEST_ASSERT_EQUAL_STRING("Тривога 1:05", in.card(75 * MIN, DAY).line1.c_str());
}

void test_alert_end_feeds_the_all_clear_scene_for_10_min() {
    Info in;
    in.onAlert(true, 0);
    TEST_ASSERT_FALSE(in.alertEnded(MIN));
    in.onAlert(false, 20 * MIN);
    TEST_ASSERT_EQUAL(int(AlertLevel::None), int(in.alert(20 * MIN)));
    TEST_ASSERT_TRUE(in.alertEnded(25 * MIN));
    TEST_ASSERT_FALSE(in.alertEnded(31 * MIN));
    Info quiet;
    quiet.onAlert(false, 0);
    TEST_ASSERT_FALSE(quiet.alertEnded(MIN));  // never was on
}

void test_alert_level_from_the_mac_overrides_for_3_min() {
    Info in;
    in.onAlert(true, 0);
    in.onAlertLevel(AlertLevel::Drone, MIN);
    Info::Card c = in.card(2 * MIN, DAY);
    TEST_ASSERT_EQUAL(int(AlertLevel::Drone), int(in.alert(2 * MIN)));
    TEST_ASSERT_EQUAL_STRING("Drones 2m", c.line1.c_str());
    TEST_ASSERT_EQUAL(1, c.tone);
    in.onAlertLevel(AlertLevel::Missile, 2 * MIN);
    TEST_ASSERT_EQUAL(2, in.card(3 * MIN, DAY).tone);
    in.onAlert(true, 5 * MIN);
    TEST_ASSERT_EQUAL(int(AlertLevel::Alert), int(in.alert(6 * MIN)));  // Mac gone: back to ubilling
    in.onAlertLevel(AlertLevel::None, 7 * MIN);                          // the Mac says all clear: wins
    TEST_ASSERT_EQUAL(int(AlertLevel::None), int(in.alert(8 * MIN)));
}

void test_threat_and_partial_levels() {
    setLang(Lang::Uk);
    Info in;
    in.setRegion("м. Київ");
    in.onAlertLevel(AlertLevel::Threat, 0);  // yellow: a possible alert, not an alert
    TEST_ASSERT_FALSE(isAlert(in.alert(MIN)));
    Info::Card c = in.card(MIN, DAY);
    TEST_ASSERT_EQUAL_STRING("Загроза", c.line1.c_str());
    TEST_ASSERT_EQUAL(5, c.tone);
    TEST_ASSERT_FALSE(in.alertEnded(MIN));
    in.onAlertLevel(AlertLevel::Partial, 2 * MIN);  // some hromadas of the oblast
    TEST_ASSERT_TRUE(isAlert(in.alert(2 * MIN)));
    c = in.card(4 * MIN, DAY);
    TEST_ASSERT_EQUAL_STRING("Частково 2хв", c.line1.c_str());
    TEST_ASSERT_EQUAL(6, c.tone);
    setLang(Lang::En);
}

void test_stale_alert_source_is_not_an_alert() {
    Info in;
    in.onAlert(true, 0);
    TEST_ASSERT_EQUAL(int(AlertLevel::None), int(in.alert(6 * MIN)));  // no fresh answer for 5 min
}

void test_alerts_unknown_when_the_source_is_silent() {
    Info in;
    TEST_ASSERT_FALSE(in.alertsUnknown(10 * MIN));  // no region: alerts are off, nothing to warn about
    in.setRegion("x");
    TEST_ASSERT_FALSE(in.alertsUnknown(2 * MIN));   // just booted: give the first fetches time
    TEST_ASSERT_TRUE(in.alertsUnknown(6 * MIN));    // 5 min after boot still no answer
    in.onAlert(false, 6 * MIN);
    TEST_ASSERT_FALSE(in.alertsUnknown(7 * MIN));
    TEST_ASSERT_TRUE(in.alertsUnknown(12 * MIN));   // answers stopped for over 5 min
    in.onAlertLevel(AlertLevel::None, 12 * MIN);    // the Mac's source still answers
    TEST_ASSERT_FALSE(in.alertsUnknown(13 * MIN));
}

void test_today_survives_a_restart_through_a_snapshot() {
    Info a;
    for (uint32_t t = 0; t <= 30 * MIN; t += 1000) a.onBusy(true, t, DAY);
    a.onAnswer(DAY);
    a.onCommit(DAY);
    TodaySnap snap = a.today();
    TEST_ASSERT_EQUAL(DAY, snap.day);
    Info b;  // after the restart: millis() starts again from zero
    b.restore(snap);
    b.onBusy(true, 0, DAY);
    b.onBusy(true, 60000, DAY);
    Info::Card c = b.card(70000, DAY);
    TEST_ASSERT_EQUAL_STRING("Today 31m", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("1 ans. · 1 commit", c.line2.c_str());
    Info d;
    d.restore(snap);
    TEST_ASSERT_TRUE(d.card(0, DAY + 1).line1.empty());  // restored yesterday: not shown today
}

static const uint32_t E0 = 1795680000;  // an epoch (seconds) for the meeting tests

void test_meeting_in_rotation_then_pinned_blue_then_now() {
    Info in;
    in.onWeather(WeatherNow{803, 3, 5, 1, false}, 0);
    Meeting ms[] = {{E0 + 3 * 3600, E0 + 4 * 3600, "Planning", "14:00"}, {E0 + 5 * 3600, E0 + 6 * 3600, "", "16:00"}};
    in.onMeetings(ms, 2);
    // 3 h ahead: one more item in the rotation, "At 14:00"
    Info::Card c = in.card(5000, DAY, E0);
    TEST_ASSERT_EQUAL(2, c.items);
    TEST_ASSERT_EQUAL(int(InfoIcon::Calendar), int(c.icon));
    TEST_ASSERT_EQUAL_STRING("At 14:00", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("Planning", c.line2.c_str());
    TEST_ASSERT_EQUAL(0, c.tone);
    // 40 min ahead: "In 40m"
    TEST_ASSERT_EQUAL_STRING("In 40m", in.card(5000, DAY, E0 + 3 * 3600 - 40 * 60).line1.c_str());
    // 8 min ahead (7:50 rounds up): pinned, blue, no dots
    c = in.card(0, DAY, E0 + 3 * 3600 - 470);
    TEST_ASSERT_EQUAL_STRING("In 8m", c.line1.c_str());
    TEST_ASSERT_EQUAL(3, c.tone);
    TEST_ASSERT_EQUAL(0, c.items);
    TEST_ASSERT_EQUAL(8, in.meetingSoonMin(E0 + 3 * 3600 - 470));
    TEST_ASSERT_EQUAL(-1, in.meetingSoonMin(E0 + 3 * 3600 - 700));
    // during the meeting
    c = in.card(0, DAY, E0 + 3 * 3600 + 35 * 60);
    TEST_ASSERT_EQUAL_STRING("Now · 25m left", c.line1.c_str());
    TEST_ASSERT_EQUAL(3, c.tone);
    TEST_ASSERT_TRUE(in.inMeeting(E0 + 3 * 3600 + 35 * 60));
    // after it the next one, untitled
    setLang(Lang::Uk);
    c = in.card(5000, DAY, E0 + 4 * 3600 + 60);
    TEST_ASSERT_EQUAL_STRING("Через 59хв", c.line1.c_str());
    TEST_ASSERT_EQUAL_STRING("Зустріч", c.line2.c_str());
    // all over: back to weather only
    TEST_ASSERT_EQUAL(1, in.card(5000, DAY, E0 + 7 * 3600).items);
    TEST_ASSERT_FALSE(in.inMeeting(0));  // time not known yet
}

void test_alert_wins_over_a_meeting() {
    Info in;
    Meeting m{E0, E0 + 3600, "Sync", "10:00"};
    in.onMeetings(&m, 1);
    in.onAlert(true, 0);
    TEST_ASSERT_EQUAL(2, in.card(1000, DAY, E0 + 60).tone);
    in.onMeetings(nullptr, 0);  // calendar switched off
    TEST_ASSERT_FALSE(in.inMeeting(E0 + 60));
}

void test_alert_duration_from_the_source_survives_a_restart() {
    std::string changed;
    TEST_ASSERT_EQUAL(1, parseAlertTail("{\"alertnow\":true,\"changed\":\"2026-11-26 13:12:47\"}", &changed));
    TEST_ASSERT_EQUAL_STRING("2026-11-26 13:12:47", changed.c_str());
    int y, mo, d, h, mi, se;
    TEST_ASSERT_TRUE(parseStamp(changed, y, mo, d, h, mi, se));
    TEST_ASSERT_EQUAL(2026, y);
    TEST_ASSERT_EQUAL(13, h);
    TEST_ASSERT_EQUAL(47, se);
    TEST_ASSERT_FALSE(parseStamp("2022", y, mo, d, h, mi, se));
    Info in;  // just restarted: millis() is small, the alert began 95 min ago by the source
    in.onAlert(true, 1000, E0 - 95 * 60);
    TEST_ASSERT_EQUAL_STRING("Air raid alert 1:35", in.card(2000, DAY, E0).line1.c_str());
    TEST_ASSERT_EQUAL_STRING("Air raid alert 0m", in.card(2000, DAY, 0).line1.c_str());  // no clock yet: own count
    Info old;  // an alert that has been on for years (occupied regions): shown in days
    old.onAlert(true, 1000, E0 - 3 * 86400 - 3600);
    TEST_ASSERT_EQUAL_STRING("Air raid alert 3d", old.card(2000, DAY, E0).line1.c_str());
    in.onAlert(true, 2000, E0 + 600);  // a start time in the future (clock skew): ignored
    TEST_ASSERT_EQUAL_STRING("Air raid alert 1:35", in.card(2000, DAY, E0).line1.c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_owm_parses_id_temp_wind);
    RUN_TEST(test_owm_errors_are_rejected);
    RUN_TEST(test_weather_kind_for_scenes);
    RUN_TEST(test_weather_text_both_languages);
    RUN_TEST(test_json_key_escapes_like_the_api);
    RUN_TEST(test_alerts_by_region);
    RUN_TEST(test_weather_details_icon_and_night);
    RUN_TEST(test_rain_forecast_replaces_the_detail_line);
    RUN_TEST(test_today_counts_busy_time_answers_and_commits);
    RUN_TEST(test_busy_gap_after_a_pause_is_not_counted);
    RUN_TEST(test_card_rotates_with_dots);
    RUN_TEST(test_message_takes_the_card_for_a_while);
    RUN_TEST(test_week_log_keeps_8_days_and_sums_last_week);
    RUN_TEST(test_monday_card_shows_last_week);
    RUN_TEST(test_today_survives_a_restart_through_a_snapshot);
    RUN_TEST(test_meeting_in_rotation_then_pinned_blue_then_now);
    RUN_TEST(test_alert_wins_over_a_meeting);
    RUN_TEST(test_alert_duration_from_the_source_survives_a_restart);
    RUN_TEST(test_alert_takes_the_card_and_counts_minutes);
    RUN_TEST(test_alert_end_feeds_the_all_clear_scene_for_10_min);
    RUN_TEST(test_alert_level_from_the_mac_overrides_for_3_min);
    RUN_TEST(test_threat_and_partial_levels);
    RUN_TEST(test_stale_alert_source_is_not_an_alert);
    RUN_TEST(test_alerts_unknown_when_the_source_is_silent);
    return UNITY_END();
}
