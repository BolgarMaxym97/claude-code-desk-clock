// Idle animations on the clock screen: random scheduler + per-action poses.
#include <unity.h>
#include <set>
#include "idle.h"
#include "sprites.h"
#include "scene.h"
#include <cstring>
#include <vector>

void setUp() {}
void tearDown() {}

static const int MIN = 30;  // minute stays constant unless a test changes it

// One continuous play of an action (scene index, or 500 + IdleAct for classic actions).
struct Play { int key; uint32_t start, end; };
static int keyOf(const IdleDirector &d) {
    return d.act() == IdleAct::Scene ? d.sceneIndex() : (d.act() == IdleAct::Rest ? -1 : 500 + int(d.act()));
}
static std::vector<Play> plays(IdleDirector &d, uint32_t from, uint32_t to, bool tired = false) {
    std::vector<Play> v;
    for (uint32_t t = from; t < to; t++) {
        d.pose(t, MIN, tired);
        int k = keyOf(d);
        if (k < 0) continue;
        if (!v.empty() && v.back().key == k && v.back().end + 1 == t) v.back().end = t;
        else v.push_back(Play{k, t, t});
    }
    return v;
}
// Plays of the same action separated by short gaps form one group.
static std::vector<std::vector<Play>> groups(const std::vector<Play> &v) {
    std::vector<std::vector<Play>> g;
    for (const Play &p : v) {
        if (!g.empty() && g.back().back().key == p.key && p.start - g.back().back().end <= 40) g.back().push_back(p);
        else g.push_back({p});
    }
    return g;
}

void test_rest_blinks_irregularly_every_2_to_6_seconds() {
    IdleDirector d(7);
    std::set<uint32_t> gaps;
    uint32_t lastBlink = 0;
    int blinks = 0;
    bool prevClosed = false;
    for (uint32_t t = 0; t < 600; t++) {  // 84 s, before the first action (>= 90 s)
        IdlePose p = d.pose(t, MIN, false);
        TEST_ASSERT_EQUAL(int(IdleAct::Rest), int(d.act()));
        bool closed = p.frame == &SPR_BLINK;
        if (!closed) TEST_ASSERT_TRUE(p.frame == &SPR_IDLE);
        if (closed && !prevClosed) {
            if (blinks) {
                uint32_t gap = t - lastBlink;
                TEST_ASSERT_TRUE_MESSAGE(gap * IDLE_TICK_MS >= 280 && gap * IDLE_TICK_MS <= 6100, "blink gap out of range");
                gaps.insert(gap);
            }
            lastBlink = t;
            blinks++;
        }
        prevClosed = closed;
    }
    TEST_ASSERT_TRUE(blinks >= 12 && blinks <= 45);
    TEST_ASSERT_TRUE_MESSAGE(gaps.size() >= 5, "blinks are not on a fixed beat");
}

void test_first_action_starts_after_1_5_to_4_minutes() {
    for (uint32_t seed = 1; seed < 40; seed++) {
        IdleDirector d(seed);
        uint32_t t = 0;
        while (t < 4000) { d.pose(t, MIN, false); if (d.act() != IdleAct::Rest) break; t++; }
        TEST_ASSERT_TRUE_MESSAGE(t * IDLE_TICK_MS >= 90000 && t * IDLE_TICK_MS <= 240000, "first action out of 1.5..4 min");
    }
}

void test_all_chosen_actions_occur_and_never_repeat_back_to_back() {
    IdleDirector d(3);
    std::set<int> seen;
    auto g = groups(plays(d, 0, 600000));
    for (size_t k = 0; k < g.size(); k++) {
        seen.insert(g[k][0].key);
        if (k) TEST_ASSERT_TRUE_MESSAGE(g[k][0].key != g[k - 1][0].key, "same action twice in a row");
    }
    const IdleAct want[] = {IdleAct::Walk, IdleAct::Jump, IdleAct::Dance, IdleAct::Look, IdleAct::Wave,
                            IdleAct::Doze, IdleAct::Star, IdleAct::Eat, IdleAct::Sport};
    for (IdleAct a : want) TEST_ASSERT_TRUE_MESSAGE(seen.count(500 + int(a)), "action never chosen");
    TEST_ASSERT_FALSE(seen.count(500 + int(IdleAct::ClockLook)));  // only on minute change, never random
}

void test_each_action_plays_three_times_with_short_gaps() {
    IdleDirector d(31);
    auto g = groups(plays(d, 0, 300000));
    TEST_ASSERT_TRUE(g.size() > 20);
    for (size_t k = 0; k + 1 < g.size(); k++) {  // the last group may be cut by the end of the run
        TEST_ASSERT_EQUAL_MESSAGE(3, int(g[k].size()), "each action plays 3 times");
        for (size_t i = 1; i < g[k].size(); i++) {
            uint32_t gap = g[k][i].start - g[k][i - 1].end;
            TEST_ASSERT_TRUE_MESSAGE(gap * IDLE_TICK_MS >= 2000 && gap * IDLE_TICK_MS <= 4300, "2-4 s between replays");
        }
        uint32_t pause = g[k + 1][0].start - g[k].back().end;
        TEST_ASSERT_TRUE_MESSAGE(pause * IDLE_TICK_MS >= 90000 && pause * IDLE_TICK_MS <= 240300, "1.5-4 min before the next action");
    }
}

void test_minute_change_while_resting_looks_at_clock() {
    IdleDirector d(5);
    d.pose(0, 41, false);
    IdlePose p = d.pose(1, 42, false);
    TEST_ASSERT_EQUAL(int(IdleAct::ClockLook), int(d.act()));
    TEST_ASSERT_TRUE(p.frame == &SPR_UP);
}

void test_minute_glances_do_not_postpone_the_next_action() {
    for (uint32_t seed = 1; seed < 20; seed++) {
        IdleDirector d(seed);
        const uint32_t perMin = 60000 / IDLE_TICK_MS;
        uint32_t t = 0;
        bool acted = false;
        for (; t < 10 * perMin && !acted; t++) {  // wall clock runs: the minute changes every 60 s (not at :00)
            d.pose(t, 1 + int(t / perMin), false);
            acted = d.act() != IdleAct::Rest && d.act() != IdleAct::ClockLook;
        }
        TEST_ASSERT_TRUE_MESSAGE(acted, "only clock glances for 10 minutes");
        TEST_ASSERT_TRUE_MESSAGE(t * IDLE_TICK_MS <= 245000, "first action later than 4 min");
    }
}

void test_tired_rests_drowsy_with_sweat_and_only_calm_actions() {
    IdleDirector d(11);
    bool sweat = false;
    for (uint32_t t = 0; t < 40000; t++) {
        IdlePose p = d.pose(t, MIN, true);
        IdleAct a = d.act();
        bool calmScene = a == IdleAct::Scene && sceneAt(d.sceneIndex()).calm;
        TEST_ASSERT_TRUE_MESSAGE(a == IdleAct::Rest || a == IdleAct::Look || a == IdleAct::Wave || a == IdleAct::Doze ||
                                     a == IdleAct::Eat || calmScene,
                                 "energetic action while tired");
        if (a == IdleAct::Rest) {
            TEST_ASSERT_TRUE(p.frame == &SPR_DROWSY || p.frame == &SPR_BLINK);
            if (p.fx & FX_SWEAT) sweat = true;
        }
    }
    TEST_ASSERT_TRUE(sweat);
}

void test_poses_stay_inside_the_stage() {
    IdleDirector d(9);
    for (uint32_t t = 0; t < 40000; t++) {
        IdlePose p = d.pose(t, MIN, false);
        TEST_ASSERT_NOT_NULL(p.frame);
        TEST_ASSERT_TRUE(p.dx >= -IDLE_MAX_DX && p.dx <= IDLE_MAX_DX);
        TEST_ASSERT_TRUE(p.dy >= -16 && p.dy <= 6);
    }
}

static IdleDirector runUntil(IdleAct want, uint32_t &t) {
    IdleDirector d(13);
    for (t = 0; t < 2000000; t++) { d.pose(t, MIN, false); if (d.act() == want) break; }
    return d;
}

void test_eating_apple_shrinks_to_nothing_then_happy() {
    uint32_t t;
    IdleDirector d = runUntil(IdleAct::Eat, t);
    int lastW = 99;
    bool crumbs = false, happy = false;
    for (; d.act() == IdleAct::Eat; t++) {
        IdlePose p = d.pose(t, MIN, false);
        if (d.act() != IdleAct::Eat) break;
        if (p.fx & FX_FOOD) { TEST_ASSERT_TRUE(p.foodW <= lastW && p.foodW > 0); lastW = p.foodW; }
        if (p.fx & FX_CRUMB) crumbs = true;
        if (p.frame == &SPR_HAPPY) happy = true;
    }
    TEST_ASSERT_EQUAL(3, lastW);
    TEST_ASSERT_TRUE(crumbs);
    TEST_ASSERT_TRUE(happy);
}

void test_sport_wears_headband_and_sweats() {
    uint32_t t;
    IdleDirector d = runUntil(IdleAct::Sport, t);
    int frames = 0;
    for (; ; t++) {
        IdlePose p = d.pose(t, MIN, false);
        if (d.act() != IdleAct::Sport) break;
        TEST_ASSERT_TRUE(p.frame == &SPR_BAND_A || p.frame == &SPR_BAND_B || p.frame == &SPR_BAND_SQ ||
                         p.frame == &SPR_BAND_UP);
        TEST_ASSERT_TRUE(p.fx & FX_SWEAT);
        frames++;
    }
    TEST_ASSERT_TRUE(frames >= 40);
}

void test_star_falls_then_is_caught() {
    uint32_t t;
    IdleDirector d = runUntil(IdleAct::Star, t);
    int lastY = -100;
    bool caughtHappy = false;
    for (; ; t++) {
        IdlePose p = d.pose(t, MIN, false);
        if (d.act() != IdleAct::Star) break;
        if (p.fx & FX_STAR) { TEST_ASSERT_TRUE(p.fxY >= lastY); lastY = p.fxY; }
        else if (p.frame == &SPR_HAPPY) caughtHappy = true;
    }
    TEST_ASSERT_TRUE(caughtHappy);
}

void test_hourly_party_at_full_hour_even_mid_action() {
    IdleDirector d(13);
    uint32_t t = 0;
    for (; t < 100000; t++) { d.pose(t, 59, false); if (d.act() == IdleAct::Walk) break; }  // busy with an action
    IdlePose p = d.pose(++t, 0, false);
    TEST_ASSERT_EQUAL(int(IdleAct::Party), int(d.act()));
    int ticks = 0;
    for (; d.act() == IdleAct::Party; t++, ticks++) {
        TEST_ASSERT_TRUE(p.frame == &SPR_ARM_A || p.frame == &SPR_ARM_L);
        TEST_ASSERT_TRUE(p.fx & FX_SPARK);
        p = d.pose(t + 1, 0, false);
    }
    TEST_ASSERT_TRUE(ticks * IDLE_TICK_MS >= 5000);
}

void test_party_never_random_and_not_on_other_minutes() {
    IdleDirector d(3);
    for (uint32_t t = 0; t < 40000; t++) {
        d.pose(t, int((t / 430) % 59) + 1, false);  // minutes 1..59 only
        TEST_ASSERT_TRUE(d.act() != IdleAct::Party);
    }
}

void test_sleep_breathes_slowly_and_zs_float_up() {
    int ups = 0, lastDy = 0, changes = 0;
    for (uint32_t t = 0; t < 200; t++) {
        SleepPose p = sleepPose(t);
        TEST_ASSERT_TRUE(p.dy == 0 || p.dy == 1);
        if (t && p.dy != lastDy) changes++;
        lastDy = p.dy;
        TEST_ASSERT_TRUE(p.zCount >= 1 && p.zCount <= 3);
        for (int i = 0; i < p.zCount; i++) {
            TEST_ASSERT_TRUE(p.z[i].x >= 0 && p.z[i].y <= 0);  // right of / above the sprite's top-right
            TEST_ASSERT_TRUE(p.z[i].y >= -40);
        }
        SleepPose n = sleepPose(t + 1);
        if (n.z[0].y < p.z[0].y) ups++;
    }
    TEST_ASSERT_TRUE(changes >= 8 && changes <= 20);  // ~2 s per breath at 140 ms
    TEST_ASSERT_TRUE(ups > 100);                       // the first z mostly rises
}

void test_new_year_season_catches_a_snowflake_instead_of_star() {
    for (int season = 0; season < 2; season++) {
        IdleDirector d(13);
        d.setCalendar(season, false);
        bool star = false, flake = false;
        for (uint32_t t = 0; t < 1000000; t++) {
            IdlePose p = d.pose(t, MIN, false);
            star |= (p.fx & FX_STAR) != 0;
            flake |= (p.fx & FX_FLAKE) != 0;
        }
        TEST_ASSERT_EQUAL(!season, star);
        TEST_ASSERT_EQUAL(season, flake);
    }
}

void test_new_year_midnight_dances_with_fireworks_all_minute() {
    IdleDirector d(5);
    d.setCalendar(true, false);
    d.pose(0, 59, false);
    d.setCalendar(true, true);
    int armA = 0;
    for (uint32_t t = 1; t < 430; t++) {  // the whole minute 00:00
        IdlePose p = d.pose(t, 0, false);
        armA += p.frame == &SPR_ARM_A;
        TEST_ASSERT_EQUAL(int(IdleAct::NewYear), int(d.act()));
        TEST_ASSERT_TRUE(p.fx & FX_FIREWORK);
        TEST_ASSERT_TRUE(p.frame == &SPR_ARM_A || p.frame == &SPR_ARM_L);
    }
    TEST_ASSERT_TRUE(armA > 100 && armA < 330);  // really dancing, not frozen on one frame
    d.setCalendar(true, false);
    d.pose(430, 1, false);
    TEST_ASSERT_TRUE(d.act() != IdleAct::NewYear);
}

void test_snow_falls_inside_the_stage() {
    Dot a[16], b[16];
    int n = snowFlakes(10, a, 16);
    TEST_ASSERT_TRUE(n >= 10 && n <= 16);
    snowFlakes(11, b, 16);
    int down = 0;
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(a[i].x >= 0 && a[i].x < IDLE_STAGE_W && a[i].y >= 0 && a[i].y < IDLE_STAGE_H);
        if (b[i].y > a[i].y) down++;
    }
    TEST_ASSERT_TRUE(down > n / 2);
}

void test_fireworks_burst_inside_the_stage_and_grow() {
    Dot d[40];
    uint8_t c[40];
    int maxN = 0;
    for (uint32_t t = 0; t < 42; t++) {
        int n = fireworkDots(t, d, c, 40);
        if (n > maxN) maxN = n;
        for (int i = 0; i < n; i++) {
            TEST_ASSERT_TRUE(d[i].x >= 0 && d[i].x < IDLE_STAGE_W && d[i].y >= 0 && d[i].y < IDLE_STAGE_H);
            TEST_ASSERT_TRUE(c[i] < 3);
        }
    }
    TEST_ASSERT_TRUE(maxN >= 15);
}

void test_tree_lights_blink_in_turn() {
    int changes = 0;
    for (uint32_t t = 0; t < 40; t++) changes += treeLightOn(t, 0) != treeLightOn(t + 1, 0);
    TEST_ASSERT_TRUE(changes >= 5);
    TEST_ASSERT_TRUE(treeLightOn(8, 0) != treeLightOn(8, 1));  // neighbours alternate
}

static int sceneIdx(const char *n) {
    for (int i = 0; i < sceneCount(); i++) if (!strcmp(sceneAt(i).name, n)) return i;
    return -1;
}

void test_daily_scenes_join_the_random_pool() {
    IdleDirector d(17);
    std::set<int> scenes;
    for (uint32_t t = 0; t < 600000; t++) {
        d.pose(t, MIN, false);
        if (d.act() == IdleAct::Scene) {
            const Scene &s = sceneAt(d.sceneIndex());
            TEST_ASSERT_TRUE_MESSAGE(s.kind == SceneKind::Daily, "only daily scenes without context");
            scenes.insert(d.sceneIndex());
        }
    }
    TEST_ASSERT_TRUE(scenes.size() >= 25);  // most of the 33 daily scenes show up
}

void test_event_scene_plays_next_and_then_cools_down() {
    IdleDirector d(19);
    SceneCtx c;
    c.synced = true; c.year = 2026; c.month = 11; c.day = 25; c.wday = 3; c.hour = 15;
    c.weather = Weather::Rain;
    d.setContext(c);
    auto g = groups(plays(d, 0, 43000));  // ~100 min
    int rain = 0;
    for (auto &x : g) if (x[0].key == sceneIdx("rain")) rain++;
    TEST_ASSERT_EQUAL_MESSAGE(sceneIdx("rain"), g[0][0].key, "rain is the first action");
    TEST_ASSERT_TRUE_MESSAGE(rain >= 2 && rain <= 3, "rain at most once per 40 min");
}

void test_holiday_day_plays_only_the_holiday_scene() {
    for (int holiday = 0; holiday < 2; holiday++) {
        IdleDirector d(23);
        SceneCtx c;
        c.synced = true; c.year = 2026; c.month = 8; c.day = holiday ? 24 : 23; c.wday = holiday ? 1 : 0; c.hour = 15;
        d.setContext(c);
        auto v = plays(d, 0, 100000);
        int flag = 0;
        for (auto &p : v) flag += p.key == sceneIdx("independence");
        if (holiday) TEST_ASSERT_EQUAL_MESSAGE(int(v.size()), flag, "only the holiday scene all day");
        else TEST_ASSERT_EQUAL(0, flag);
    }
}

void test_holiday_hour_mark_plays_the_holiday_scene_and_alert_still_wins() {
    IdleDirector d(29);
    SceneCtx c;
    c.synced = true; c.year = 2026; c.month = 8; c.day = 24; c.wday = 1; c.hour = 15;
    d.setContext(c);
    d.pose(0, 59, false);
    d.pose(1, 0, false);  // :00 on a holiday: no party, the holiday scene instead
    TEST_ASSERT_EQUAL(int(IdleAct::Scene), int(d.act()));
    TEST_ASSERT_EQUAL(sceneIdx("independence"), d.sceneIndex());
    IdleDirector e(29);
    c.alert = true;
    e.setContext(c);
    auto g = groups(plays(e, 0, 3000));
    TEST_ASSERT_EQUAL_MESSAGE(sceneIdx("alert"), g[0][0].key, "air-raid alert is shown even on a holiday");
}

void test_weekend_pool_only_on_weekends() {
    for (int weekend = 0; weekend < 2; weekend++) {
        IdleDirector d(41);
        SceneCtx c;
        c.synced = true; c.year = 2026; c.month = 11; c.day = weekend ? 28 : 26; c.wday = weekend ? 6 : 4; c.hour = 15;
        d.setContext(c);
        auto g = groups(plays(d, 0, 400000));
        int w = 0;
        for (auto &x : g) if (x[0].key < 500 && sceneAt(x[0].key).kind == SceneKind::Weekend) w++;
        if (weekend) TEST_ASSERT_TRUE_MESSAGE(w * 100 >= int(g.size()) * 25 && w * 100 <= int(g.size()) * 55, "~40 % weekend scenes on Sat/Sun");
        else TEST_ASSERT_EQUAL(0, w);
    }
}

void test_rare_surprises_at_most_once_a_day() {
    IdleDirector d(43);
    SceneCtx c;
    c.synced = true; c.year = 2026; c.month = 11; c.wday = 3; c.hour = 14;
    int total = 0;
    for (int day = 1; day <= 10; day++) {
        c.day = day;
        d.setContext(c);
        int today = 0;
        uint32_t from = uint32_t(day) * 300000;
        auto g = groups(plays(d, from, from + 260000));  // ~10 h of a day
        for (auto &x : g) if (x[0].key < 500 && sceneAt(x[0].key).kind == SceneKind::Rare) today++;
        TEST_ASSERT_TRUE_MESSAGE(today <= 1, "more than one surprise a day");
        total += today;
    }
    TEST_ASSERT_TRUE_MESSAGE(total >= 2 && total <= 9, "surprises roughly every couple of days");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rest_blinks_irregularly_every_2_to_6_seconds);
    RUN_TEST(test_first_action_starts_after_1_5_to_4_minutes);
    RUN_TEST(test_all_chosen_actions_occur_and_never_repeat_back_to_back);
    RUN_TEST(test_minute_change_while_resting_looks_at_clock);
    RUN_TEST(test_minute_glances_do_not_postpone_the_next_action);
    RUN_TEST(test_tired_rests_drowsy_with_sweat_and_only_calm_actions);
    RUN_TEST(test_poses_stay_inside_the_stage);
    RUN_TEST(test_eating_apple_shrinks_to_nothing_then_happy);
    RUN_TEST(test_sport_wears_headband_and_sweats);
    RUN_TEST(test_star_falls_then_is_caught);
    RUN_TEST(test_hourly_party_at_full_hour_even_mid_action);
    RUN_TEST(test_party_never_random_and_not_on_other_minutes);
    RUN_TEST(test_sleep_breathes_slowly_and_zs_float_up);
    RUN_TEST(test_daily_scenes_join_the_random_pool);
    RUN_TEST(test_event_scene_plays_next_and_then_cools_down);
    RUN_TEST(test_holiday_day_plays_only_the_holiday_scene);
    RUN_TEST(test_holiday_hour_mark_plays_the_holiday_scene_and_alert_still_wins);
    RUN_TEST(test_each_action_plays_three_times_with_short_gaps);
    RUN_TEST(test_weekend_pool_only_on_weekends);
    RUN_TEST(test_rare_surprises_at_most_once_a_day);
    RUN_TEST(test_new_year_season_catches_a_snowflake_instead_of_star);
    RUN_TEST(test_new_year_midnight_dances_with_fireworks_all_minute);
    RUN_TEST(test_snow_falls_inside_the_stage);
    RUN_TEST(test_fireworks_burst_inside_the_stage_and_grow);
    RUN_TEST(test_tree_lights_blink_in_turn);
    return UNITY_END();
}
