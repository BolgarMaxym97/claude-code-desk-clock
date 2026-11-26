// Scene pack: calendar maths, every scene draws sane rectangles inside its area, holiday / event conditions.
#include <unity.h>
#include <cstring>
#include <set>
#include <string>
#include "scene.h"
#include "model.h"

void setUp() {}
void tearDown() {}

struct Rec : Canvas {
    int n = 0, bad = 0, inside = 0;
    int x0, y0, x1, y1;
    Rec(int ax, int ay, int w, int h) : x0(ax), y0(ay), x1(ax + w), y1(ay + h) {}
    void rect(int x, int y, int w, int h, uint8_t slot, int r) override {
        n++;
        if (w < 0 || h < 0 || slot > 15 || r < 0) bad++;
        if (x < x1 && x + w > x0 && y < y1 && y + h > y0) inside++;
    }
};

void test_orthodox_easter() {
    int m, d;
    orthodoxEaster(2024, m, d); TEST_ASSERT_EQUAL(5, m); TEST_ASSERT_EQUAL(5, d);
    orthodoxEaster(2025, m, d); TEST_ASSERT_EQUAL(4, m); TEST_ASSERT_EQUAL(20, d);
    orthodoxEaster(2026, m, d); TEST_ASSERT_EQUAL(4, m); TEST_ASSERT_EQUAL(12, d);
    orthodoxEaster(2027, m, d); TEST_ASSERT_EQUAL(5, m); TEST_ASSERT_EQUAL(2, d);
}

void test_floating_days() {
    TEST_ASSERT_EQUAL(21, nthWeekdayOfMonth(2026, 5, 4, 3));  // vyshyvanka: 3rd Thursday of May 2026
    TEST_ASSERT_EQUAL(10, nthWeekdayOfMonth(2026, 5, 0, 2));  // mother's day: 2nd Sunday of May 2026
    TEST_ASSERT_EQUAL(256, dayOfYear(2026, 9, 13));           // programmers' day
    TEST_ASSERT_EQUAL(256, dayOfYear(2024, 9, 12));           // leap year
}

static bool isWork(const Scene &s) { return s.kind == SceneKind::Work; }

void test_every_scene_draws_sane_rects_in_its_area() {
    std::set<std::string> names;
    TEST_ASSERT_TRUE(sceneCount() >= 63 + 13);
    for (int i = 0; i < sceneCount(); i++) {
        const Scene &s = sceneAt(i);
        TEST_ASSERT_TRUE_MESSAGE(names.insert(s.name).second, s.name);
        TEST_ASSERT_TRUE_MESSAGE(s.ticks > 0, s.name);
        TEST_ASSERT_NOT_NULL_MESSAGE(s.draw, s.name);
        if (s.kind == SceneKind::Event || s.kind == SceneKind::Holiday) TEST_ASSERT_NOT_NULL_MESSAGE(s.when, s.name);
        if (s.kind == SceneKind::Holiday) TEST_ASSERT_NOT_NULL_MESSAGE(s.caption, s.name);
        bool work = isWork(s);
        for (uint32_t p = 0; p < s.ticks; p++) {
            Rec r = work ? Rec(28, 44, 184, 60) : Rec(28, 80, 184, 70);
            MascotAt m = s.draw(r, p, 1000 + p);
            TEST_ASSERT_EQUAL_MESSAGE(0, r.bad, s.name);
            TEST_ASSERT_TRUE_MESSAGE(r.n > 0 && r.inside > 0, s.name);
            TEST_ASSERT_TRUE_MESSAGE(m.x > -80 && m.x < 240 && m.y > 0 && m.y < 240, s.name);
        }
    }
}

static Scene found;
static const Scene *byName(const char *n) {
    int i = sceneIndexByName(n);
    if (i < 0) TEST_FAIL_MESSAGE(n);
    found = sceneAt(i);
    return &found;
}

static SceneCtx day(int y, int m, int d, int wday, int h = 12) {
    SceneCtx c;
    c.synced = true; c.year = y; c.month = m; c.day = d; c.wday = wday; c.hour = h;
    return c;
}

void test_holidays_only_on_their_day() {
    TEST_ASSERT_TRUE(byName("independence")->when(day(2026, 8, 24, 1)));
    TEST_ASSERT_FALSE(byName("independence")->when(day(2026, 8, 25, 2)));
    TEST_ASSERT_TRUE(byName("easter")->when(day(2026, 4, 12, 0)));
    TEST_ASSERT_FALSE(byName("easter")->when(day(2026, 4, 19, 0)));
    TEST_ASSERT_TRUE(byName("vyshyvanka")->when(day(2026, 5, 21, 4)));
    TEST_ASSERT_TRUE(byName("programmers")->when(day(2026, 9, 13, 0)));
    TEST_ASSERT_TRUE(byName("programmers")->when(day(2024, 9, 12, 4)));
    SceneCtx b = day(2026, 3, 3, 2);
    TEST_ASSERT_FALSE(byName("birthday")->when(b));  // not configured
    b.birthMonth = 3; b.birthDay = 3;
    TEST_ASSERT_TRUE(byName("birthday")->when(b));
    TEST_ASSERT_TRUE(byName("kyiv")->when(day(2026, 5, 31, 0)));      // last Sunday of May 2026
    TEST_ASSERT_FALSE(byName("kyiv")->when(day(2026, 5, 24, 0)));
    TEST_ASSERT_TRUE(byName("kyiv")->when(day(2027, 5, 30, 0)));
    TEST_ASSERT_FALSE(byName("clock-birthday")->when(day(2026, 11, 25, 3)));  // the day it was first flashed
    TEST_ASSERT_TRUE(byName("clock-birthday")->when(day(2027, 11, 25, 4)));
    SceneCtx u = day(2026, 8, 24, 1);
    u.synced = false;
    TEST_ASSERT_FALSE(byName("independence")->when(u));  // unknown date: no holidays
}

void test_events_follow_context() {
    SceneCtx c = day(2026, 11, 27, 5, 18);  // Friday evening
    TEST_ASSERT_TRUE(byName("friday")->when(c));
    TEST_ASSERT_FALSE(byName("rain")->when(c));
    c.weather = Weather::Rain;
    TEST_ASSERT_TRUE(byName("rain")->when(c));
    c.alert = true;
    TEST_ASSERT_TRUE(byName("alert")->when(c));
    TEST_ASSERT_FALSE(byName("monday")->when(c));
    TEST_ASSERT_TRUE(byName("morning")->when(day(2026, 11, 26, 4, 8)));
    TEST_ASSERT_FALSE(byName("morning")->when(day(2026, 11, 26, 4, 13)));
}

static const char *work(SState st, const char *tool, uint32_t variant = 0) { return sceneAt(workScene(st, tool, variant)).name; }

void test_work_scene_by_state_and_tool() {
    TEST_ASSERT_EQUAL_STRING("work:bash", work(SState::Working, "Bash"));
    TEST_ASSERT_EQUAL_STRING("work:edit", work(SState::Working, "Edit"));
    TEST_ASSERT_EQUAL_STRING("work:edit", work(SState::Working, "MultiEdit"));
    TEST_ASSERT_EQUAL_STRING("work:read", work(SState::Working, "Grep"));
    TEST_ASSERT_EQUAL_STRING("work:agents", work(SState::Working, "Task"));
    TEST_ASSERT_EQUAL_STRING("work:web", work(SState::Working, "WebFetch"));
    TEST_ASSERT_EQUAL_STRING("work:think", work(SState::Thinking, ""));
    TEST_ASSERT_EQUAL_STRING("work:compact", work(SState::Compacting, ""));
    TEST_ASSERT_EQUAL_STRING("work:permission", work(SState::Permission, "Bash"));
    TEST_ASSERT_EQUAL_STRING("work:your-turn", work(SState::YourTurn, ""));
    TEST_ASSERT_EQUAL_STRING("work:error", work(SState::Error, ""));
    // unknown tool (mcp__*, Artifact...): walk or laptop, chosen by the variant
    std::set<std::string> v;
    for (uint32_t i = 0; i < 8; i++) v.insert(work(SState::Working, "mcp__figma", i));
    TEST_ASSERT_TRUE(v.count("work:walk") && v.count("work:laptop") && v.size() == 2);
}

static const char *pick(WorkCtx w) { return sceneAt(pickWorkScene(w)).name; }

void test_work_scene_priorities() {
    WorkCtx w;
    w.screen = Screen::Working; w.state = SState::Working; w.tool = "Bash"; w.synced = true; w.hour = 15;
    TEST_ASSERT_EQUAL_STRING("work:bash", pick(w));
    w.toolAgeMs = 61000;
    TEST_ASSERT_EQUAL_STRING("work:long", pick(w));                  // a command running for over a minute
    w.reaction = Reaction::TestsOk; w.reactionAgeMs = 3000;
    TEST_ASSERT_EQUAL_STRING("work:tests-ok", pick(w));              // fresh result beats the tool
    w.reactionAgeMs = 11000;
    TEST_ASSERT_EQUAL_STRING("work:long", pick(w));                  // reactions last ~10 s
    w.greeting = true;
    TEST_ASSERT_EQUAL_STRING("work:back", pick(w));                  // greeting first
    w.morning = true;
    TEST_ASSERT_EQUAL_STRING("work:morning", pick(w));
    WorkCtx e;
    e.screen = Screen::Working; e.state = SState::Working; e.tool = "Edit"; e.edits = 9;
    TEST_ASSERT_EQUAL_STRING("work:edit", pick(e));
    e.edits = 10;
    TEST_ASSERT_EQUAL_STRING("work:edits", pick(e));
    WorkCtx a;
    a.screen = Screen::Working; a.state = SState::Working; a.tool = "Task"; a.agents = 3;
    TEST_ASSERT_EQUAL_STRING("work:orchestra", pick(a));
    WorkCtx th;
    th.screen = Screen::Working; th.state = SState::Thinking; th.tool = ""; th.synced = true; th.hour = 23;
    TEST_ASSERT_EQUAL_STRING("work:late", pick(th));
    th.hour = 15;
    TEST_ASSERT_EQUAL_STRING("work:think", pick(th));
    WorkCtx d;
    d.screen = Screen::Done; d.state = SState::Idle; d.tool = ""; d.doneDurationMs = 30000;
    TEST_ASSERT_EQUAL_STRING("work:done-small", pick(d));          // quick answers: just a nod
    d.doneDurationMs = 3 * 60000;
    TEST_ASSERT_EQUAL_STRING("work:done", pick(d));                // real tasks: confetti
    d.reaction = Reaction::Commit; d.reactionAgeMs = 2000;
    TEST_ASSERT_EQUAL_STRING("work:commit", pick(d));
}

void test_season_layer_is_still_at_rest_and_inside_the_stage() {
    for (int month = 1; month <= 12; month++) {
        Rec a(28, 80, 184, 70), b(28, 80, 184, 70);
        seasonLayer(a, month, 100, false);
        seasonLayer(b, month, 777, false);
        TEST_ASSERT_EQUAL_MESSAGE(a.n, b.n, "season background moves while resting");
        TEST_ASSERT_EQUAL(0, a.bad);
        TEST_ASSERT_EQUAL(a.n, a.inside);
        Rec m(28, 80, 184, 70);
        seasonLayer(m, month, 100, true);
        TEST_ASSERT_EQUAL(0, m.bad);
        TEST_ASSERT_TRUE(m.n >= a.n);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_orthodox_easter);
    RUN_TEST(test_floating_days);
    RUN_TEST(test_every_scene_draws_sane_rects_in_its_area);
    RUN_TEST(test_holidays_only_on_their_day);
    RUN_TEST(test_events_follow_context);
    RUN_TEST(test_work_scene_by_state_and_tool);
    RUN_TEST(test_work_scene_priorities);
    RUN_TEST(test_season_layer_is_still_at_rest_and_inside_the_stage);
    return UNITY_END();
}
