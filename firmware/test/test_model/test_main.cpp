#include <unity.h>
#include "i18n.h"
#include <cstring>
#include "model.h"
#include "text.h"
#include "sprites.h"

static EventIn ev(const char *s, Ev e, const char *tool = "", const char *target = "") {
    return EventIn{s, "proj", "main", tool, target, e};
}

void setUp() {}
void tearDown() { setLang(Lang::En); }

// ---- Task 1: event mapping ----
void test_session_start_creates_idle() {
    Model m;
    m.onEvent(ev("s1", Ev::SessionStart), 1000);
    TEST_ASSERT_NOT_NULL(m.find("s1"));
    TEST_ASSERT_EQUAL(int(SState::Idle), int(m.find("s1")->state));
}

void test_pretooluse_sets_working_with_tool_target() {
    Model m;
    m.onEvent(ev("s1", Ev::SessionStart), 1000);
    m.onEvent(ev("s1", Ev::PreToolUse, "Edit", "main.cpp"), 2000);
    TEST_ASSERT_EQUAL(int(SState::Working), int(m.find("s1")->state));
    TEST_ASSERT_EQUAL_STRING("Edit", m.find("s1")->tool.c_str());
    TEST_ASSERT_EQUAL_STRING("main.cpp", m.find("s1")->target.c_str());
}

void test_parse_event_names() {
    TEST_ASSERT_EQUAL(int(Ev::NotifyPermission), int(parseEvent("Notification", "permission")));
    TEST_ASSERT_EQUAL(int(Ev::NotifyIdle), int(parseEvent("Notification", "idle")));
    TEST_ASSERT_EQUAL(int(Ev::PreToolUse), int(parseEvent("PreToolUse", "")));
    TEST_ASSERT_EQUAL(int(Ev::Unknown), int(parseEvent("Bogus", "")));
}

void test_unknown_event_is_noop() {
    Model m;
    m.onEvent(ev("s1", Ev::Unknown), 1000);
    TEST_ASSERT_EQUAL(0, int(m.count()));
}

// ---- Task 2: lifecycle & timeouts ----
static const uint32_t MIN = 60u * 1000u;

void test_event_state_mapping() {
    Model m;
    m.onEvent(ev("s1", Ev::UserPromptSubmit, "", "add limits"), 1000);
    TEST_ASSERT_EQUAL(int(SState::Thinking), int(m.find("s1")->state));
    TEST_ASSERT_EQUAL_STRING("add limits", m.find("s1")->target.c_str());
    m.onEvent(ev("s1", Ev::PreCompact), 2000);
    TEST_ASSERT_EQUAL(int(SState::Compacting), int(m.find("s1")->state));
    m.onEvent(ev("s1", Ev::NotifyPermission, "Bash", "git push"), 3000);
    TEST_ASSERT_EQUAL(int(SState::Permission), int(m.find("s1")->state));
    m.onEvent(ev("s1", Ev::NotifyIdle), 4000);
    TEST_ASSERT_EQUAL(int(SState::YourTurn), int(m.find("s1")->state));
    m.onEvent(ev("s1", Ev::StopFailure), 5000);
    TEST_ASSERT_EQUAL(int(SState::Error), int(m.find("s1")->state));
}

void test_posttooluse_keeps_working_after_permission() {
    Model m;
    m.onEvent(ev("s1", Ev::NotifyPermission, "Bash", "git push"), 1000);
    m.onEvent(ev("s1", Ev::PostToolUse, "Bash", ""), 2000);  // user granted, tool ran
    TEST_ASSERT_EQUAL(int(SState::Working), int(m.find("s1")->state));
}

void test_idle_session_expires_after_30min() {
    Model m;
    m.onEvent(ev("s1", Ev::SessionStart), 0);
    m.tick(30 * MIN);
    TEST_ASSERT_EQUAL(1, int(m.count()));
    m.tick(30 * MIN + 1);
    TEST_ASSERT_EQUAL(0, int(m.count()));
}

void test_busy_session_survives_90min_expires_after_2h() {
    Model m;
    m.onEvent(ev("s1", Ev::PreToolUse, "Bash", "make"), 0);
    m.tick(90 * MIN);
    TEST_ASSERT_EQUAL(1, int(m.count()));
    m.tick(120 * MIN + 1);
    TEST_ASSERT_EQUAL(0, int(m.count()));
}

void test_session_end_removes() {
    Model m;
    m.onEvent(ev("s1", Ev::SessionStart), 0);
    m.onEvent(ev("s1", Ev::SessionEnd), 10);
    TEST_ASSERT_EQUAL(0, int(m.count()));
    TEST_ASSERT_FALSE(m.macSilent(20));
}

void test_max_6_sessions_drops_oldest_idle() {
    Model m;
    m.onEvent(ev("busy", Ev::PreToolUse, "Bash", "x"), 0);
    char id[4] = "s0";
    for (int i = 1; i <= 6; i++) { id[1] = char('0' + i); m.onEvent(ev(id, Ev::SessionStart), uint32_t(i * 10)); }
    TEST_ASSERT_EQUAL(6, int(m.count()));
    TEST_ASSERT_NOT_NULL(m.find("busy"));   // busy kept even though oldest
    TEST_ASSERT_NULL(m.find("s1"));         // oldest idle dropped
    TEST_ASSERT_NOT_NULL(m.find("s6"));
}

void test_stop_returns_idle_and_records_duration() {
    Model m;
    m.onEvent(ev("s1", Ev::UserPromptSubmit), 0);
    m.onEvent(ev("s1", Ev::Stop), 252000);
    TEST_ASSERT_EQUAL(int(SState::Idle), int(m.find("s1")->state));
    TEST_ASSERT_EQUAL_UINT32(252000, m.lastDoneDurationMs());
}

void test_subagent_counter_never_negative() {
    Model m;
    m.onEvent(ev("s1", Ev::SubagentStop), 0);
    TEST_ASSERT_EQUAL(0, int(m.find("s1")->agents));
    m.onEvent(ev("s1", Ev::SubagentStart), 1);
    m.onEvent(ev("s1", Ev::SubagentStart), 2);
    TEST_ASSERT_EQUAL(2, int(m.find("s1")->agents));
    m.onEvent(ev("s1", Ev::Stop), 3);
    TEST_ASSERT_EQUAL(0, int(m.find("s1")->agents));
}

void test_mac_silent_after_sessions_expired_without_end() {
    Model m;
    m.onEvent(ev("s1", Ev::SessionStart), 0);
    m.tick(31 * MIN);
    TEST_ASSERT_TRUE(m.macSilent(31 * MIN));
    m.onEvent(ev("s2", Ev::SessionStart), 32 * MIN);
    TEST_ASSERT_FALSE(m.macSilent(32 * MIN));
}

void test_busy_count() {
    Model m;
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "x"), 0);
    m.onEvent(ev("b", Ev::UserPromptSubmit), 0);
    m.onEvent(ev("c", Ev::SessionStart), 0);
    TEST_ASSERT_EQUAL(2, int(m.busyCount()));
}


// ---- Task 3: screen selection ----
static LocalTime day() { return LocalTime{true, 14, 0, 11, 25, 3}; }
static LocalTime night() { return LocalTime{true, 23, 30, 11, 25, 3}; }

void test_permission_wins_over_other_working_session() {
    Model m;
    m.onEvent(ev("a", Ev::NotifyPermission, "Bash", "git push"), 1000);
    m.onEvent(ev("b", Ev::PreToolUse, "Edit", "x.cpp"), 1000);
    View v = m.view(2000, day());
    TEST_ASSERT_EQUAL(int(Screen::Permission), int(v.screen));
    TEST_ASSERT_EQUAL_STRING("a", v.focus->id.c_str());
}

void test_two_working_is_multi_one_is_working() {
    Model m;
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "x"), 1000);
    TEST_ASSERT_EQUAL(int(Screen::Working), int(m.view(2000, day()).screen));
    m.onEvent(ev("b", Ev::UserPromptSubmit), 1000);
    TEST_ASSERT_EQUAL(int(Screen::Multi), int(m.view(2000, day()).screen));
}

void test_thinking_and_compacting_are_working() {
    Model m;
    m.onEvent(ev("a", Ev::PreCompact), 1000);
    TEST_ASSERT_EQUAL(int(Screen::Working), int(m.view(2000, day()).screen));
}

void test_your_turn_and_error() {
    Model m;
    m.onEvent(ev("a", Ev::NotifyIdle), 1000);
    TEST_ASSERT_EQUAL(int(Screen::YourTurn), int(m.view(2000, day()).screen));
    m.onEvent(ev("a", Ev::StopFailure), 3000);
    TEST_ASSERT_EQUAL(int(Screen::Error), int(m.view(4000, day()).screen));
}

void test_done_for_6s_then_clock() {
    Model m;
    m.onEvent(ev("a", Ev::UserPromptSubmit), 0);
    m.onEvent(ev("a", Ev::Stop), 10000);
    TEST_ASSERT_EQUAL(int(Screen::Done), int(m.view(15999, day()).screen));
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(16001, day()).screen));
}

void test_hello_for_3s() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    TEST_ASSERT_EQUAL(int(Screen::Hello), int(m.view(2999, day()).screen));
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(3001, day()).screen));
}

void test_day_stays_awake_until_an_hour_without_mac() {
    Model m;
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(1000, day()).screen));       // fresh boot by day: awake
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(m.view(61 * MIN, day()).screen));   // nobody around for an hour
    m.onEvent(ev("a", Ev::SessionStart), 61 * MIN);
    m.onEvent(ev("a", Ev::Stop), 62 * MIN);
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(62 * MIN + 30 * MIN, day()).screen));  // bored, still awake
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(m.view(62 * MIN + 61 * MIN, day()).screen));
    TEST_ASSERT_EQUAL(30u * MIN, m.quietMs(62 * MIN + 30 * MIN));
}

void test_evening_sleeps_after_half_an_hour() {
    Model m;
    LocalTime eve{true, 19, 30, 11, 25, 3};
    m.onEvent(ev("a", Ev::Stop), 0);
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(29 * MIN, eve).screen));
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(m.view(31 * MIN, eve).screen));
}

void test_working_shows_at_night_dimmed() {
    Model m;
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "make"), 0);
    View v = m.view(15 * MIN, night());   // also beyond the 10-min quiet rule
    TEST_ASSERT_EQUAL(int(Screen::Working), int(v.screen));
    TEST_ASSERT_TRUE(v.night);
    TEST_ASSERT_EQUAL(20, int(v.brightness));
}

void test_night_idle_sleeps_but_your_turn_wakes() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(m.view(5000, night()).screen));
    m.onEvent(ev("a", Ev::NotifyIdle), 6000);
    TEST_ASSERT_EQUAL(int(Screen::YourTurn), int(m.view(7000, night()).screen));
}

void test_day_brightness_80() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    TEST_ASSERT_EQUAL(80, int(m.view(5000, day()).brightness));
}

void test_unsynced_time_is_not_night() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    LocalTime unsynced{false, 0, 0, 1, 1, 4};   // 1970-01-01 00:00 must not count as night
    View v = m.view(5000, unsynced);
    TEST_ASSERT_FALSE(v.night);
    TEST_ASSERT_EQUAL(int(Screen::Boot), int(v.screen));  // no time yet -> boot / connecting screen
    TEST_ASSERT_EQUAL(80, int(v.brightness));
}

void test_unsynced_boot_replaces_idle_screens_only() {
    Model m;
    LocalTime unsynced{false, 0, 0, 1, 1, 4};
    TEST_ASSERT_EQUAL(int(Screen::Boot), int(m.view(1000, unsynced).screen));  // fresh boot, no sessions
    m.onEvent(ev("a", Ev::SessionStart), 0);
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "x"), 1000);
    TEST_ASSERT_EQUAL(int(Screen::Working), int(m.view(2000, unsynced).screen));  // Claude work still wins
}

void test_limit_reached_only_replaces_clock() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    m.onLimits(100, 0, 30, 0, 5000);
    TEST_ASSERT_EQUAL(int(Screen::LimitReached), int(m.view(6000, day()).screen));
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "x"), 7000);
    TEST_ASSERT_EQUAL(int(Screen::Working), int(m.view(8000, day()).screen));
}

void test_expired_sessions_do_not_show_mac_silent() {
    // "Mac мовчить" is only for lost Wi-Fi now; a quiet Mac means a bored, then sleeping mascot
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    m.tick(31 * MIN);
    TEST_ASSERT_EQUAL(int(Screen::Clock), int(m.view(31 * MIN, day()).screen));
}

void test_greeting_after_a_long_silence() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    m.onEvent(ev("a", Ev::Stop), 1000);
    TEST_ASSERT_FALSE(m.greeting(2000));
    m.onEvent(ev("a", Ev::UserPromptSubmit), 1000 + 31 * MIN);  // back after 31 min
    TEST_ASSERT_TRUE(m.greeting(1000 + 31 * MIN + 2000));
    TEST_ASSERT_FALSE(m.greeting(1000 + 31 * MIN + 9000));      // ~8 s
    m.onEvent(ev("a", Ev::Stop), 1000 + 32 * MIN);
    m.onEvent(ev("a", Ev::UserPromptSubmit), 1000 + 33 * MIN);  // short pause: no greeting
    TEST_ASSERT_FALSE(m.greeting(1000 + 33 * MIN + 1000));
}

void test_bash_reactions() {
    TEST_ASSERT_EQUAL(int(Reaction::TestsOk), int(classifyBash("npm test", true)));
    TEST_ASSERT_EQUAL(int(Reaction::TestsFail), int(classifyBash("pytest -q", false)));
    TEST_ASSERT_EQUAL(int(Reaction::TestsOk), int(classifyBash("sh tools/test.sh", true)));
    TEST_ASSERT_EQUAL(int(Reaction::Commit), int(classifyBash("git commit -m \"x\"", true)));
    TEST_ASSERT_EQUAL(int(Reaction::None), int(classifyBash("git commit -m x", false)));
    TEST_ASSERT_EQUAL(int(Reaction::Push), int(classifyBash("git push origin main", true)));
    TEST_ASSERT_EQUAL(int(Reaction::Install), int(classifyBash("npm install", true)));
    TEST_ASSERT_EQUAL(int(Reaction::Install), int(classifyBash("pip install -r req…", true)));
    TEST_ASSERT_EQUAL(int(Reaction::Install), int(classifyBash("brew install jq", true)));
    TEST_ASSERT_EQUAL(int(Reaction::Docker), int(classifyBash("docker run --rm -v…", true)));
    TEST_ASSERT_EQUAL(int(Reaction::None), int(classifyBash("ls -la", true)));
    TEST_ASSERT_EQUAL(int(Reaction::None), int(classifyBash("cat latest.txt", true)));  // "test" inside a word
}

void test_model_remembers_reaction_tool_time_and_edits() {
    Model m;
    m.onEvent(ev("a", Ev::UserPromptSubmit), 0);
    m.onEvent(ev("a", Ev::PreToolUse, "Bash", "npm test"), 1000);
    TEST_ASSERT_EQUAL(1000u, m.find("a")->toolMs);
    m.onEvent(ev("a", Ev::PostToolUseFailure, "Bash", "npm test"), 5000);
    TEST_ASSERT_EQUAL(int(Reaction::TestsFail), int(m.reaction()));
    TEST_ASSERT_EQUAL(5000u, m.reactionAtMs());
    for (int i = 0; i < 10; i++) m.onEvent(ev("a", Ev::PreToolUse, "Edit", "x.cpp"), 6000 + i);
    TEST_ASSERT_EQUAL(10, m.find("a")->edits);
    m.onEvent(ev("a", Ev::UserPromptSubmit), 9000);
    TEST_ASSERT_EQUAL(0, m.find("a")->edits);  // counted per prompt
}

void test_quiet_night_is_sleep() {
    Model m;
    m.onEvent(ev("a", Ev::SessionStart), 0);
    m.tick(31 * MIN);
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(m.view(31 * MIN, night()).screen));
}

// ---- Task 4 (MVP part): limits ----
void test_limits_clamped_and_stale() {
    Model m;
    m.onLimits(150, 100, -5, 200, 0);
    TEST_ASSERT_EQUAL(100, m.limit5h().pct);
    TEST_ASSERT_EQUAL(0, m.limit7d().pct);
    TEST_ASSERT_TRUE(m.hasLimits());
    TEST_ASSERT_FALSE(m.limitsStale(10 * MIN));
    TEST_ASSERT_TRUE(m.limitsStale(10 * MIN + 1));
}

// ---- Task 4 (MVP part): verbs, hat, sprites ----
void test_verb_for_tool() {
    setLang(Lang::Uk);  // Ukrainian texts (English is covered in test_i18n)
    TEST_ASSERT_EQUAL_STRING("Виконує", verbFor("Bash"));
    TEST_ASSERT_EQUAL_STRING("Редагує", verbFor("Edit"));
    TEST_ASSERT_EQUAL_STRING("Редагує", verbFor("Write"));
    TEST_ASSERT_EQUAL_STRING("Редагує", verbFor("MultiEdit"));
    TEST_ASSERT_EQUAL_STRING("Редагує", verbFor("NotebookEdit"));
    TEST_ASSERT_EQUAL_STRING("Читає", verbFor("Read"));
    TEST_ASSERT_EQUAL_STRING("Читає", verbFor("Grep"));
    TEST_ASSERT_EQUAL_STRING("Читає", verbFor("Glob"));
    TEST_ASSERT_EQUAL_STRING("Делегує", verbFor("Task"));
    TEST_ASSERT_EQUAL_STRING("Делегує", verbFor("Agent"));
    TEST_ASSERT_EQUAL_STRING("Шукає", verbFor("WebSearch"));
    TEST_ASSERT_EQUAL_STRING("Шукає", verbFor("WebFetch"));
    TEST_ASSERT_EQUAL_STRING("Шукає", verbFor("mcp__clickup__search"));
    TEST_ASSERT_EQUAL_STRING("Працює", verbFor("SomethingNew"));
    TEST_ASSERT_EQUAL_STRING("Працює", verbFor(""));
}

void test_hat_season() {
    TEST_ASSERT_FALSE(hatSeason(12, 24));
    TEST_ASSERT_TRUE(hatSeason(12, 25));
    TEST_ASSERT_TRUE(hatSeason(12, 31));
    TEST_ASSERT_TRUE(hatSeason(1, 1));
    TEST_ASSERT_TRUE(hatSeason(1, 7));
    TEST_ASSERT_FALSE(hatSeason(1, 8));
}

void test_sprites_are_14_wide() {
    const SpriteFrame *all[] = {&SPR_IDLE, &SPR_BLINK, &SPR_WALK_A, &SPR_WALK_B, &SPR_ARM_A, &SPR_ARM_B,
                                &SPR_UP, &SPR_HAPPY, &SPR_ERR, &SPR_SLEEP, &SPR_HAT_IDLE, &SPR_HAT_WALK};
    for (auto f : all) {  // flat PROGMEM string, 14 chars per row, only known palette keys
        TEST_ASSERT_EQUAL(f->rows * 14, int(strlen(f->data)));
        for (int r = 0; r < f->rows; r++)
            for (int c = 0; c < 14; c++) TEST_ASSERT_NOT_NULL(strchr(".oebrhwk", spx(*f, r, c)));
    }
    TEST_ASSERT_EQUAL(10, SPR_HAT_IDLE.rows);
    TEST_ASSERT_EQUAL('e', spx(SPR_IDLE, 1, 4));
}

void test_utf8_truncate() {
    // 22 visible chars max, cut on a UTF-8 boundary, ellipsis appended
    TEST_ASSERT_EQUAL_STRING("main.cpp", utf8Truncate("main.cpp", 22).c_str());
    std::string cyr = utf8Truncate("додай екран лімітів і тривоги на годинник", 10);
    TEST_ASSERT_EQUAL_STRING("додай екра…", cyr.c_str());
    std::string emo = utf8Truncate("ab\xF0\x9F\x98\x80" "cd", 3);  // a b 😀 c d
    TEST_ASSERT_EQUAL_STRING("ab\xF0\x9F\x98\x80…", emo.c_str());
}

void test_offline_hides_stale_claude_state() {
    // Wi-Fi lost: the clock cannot know what Claude does -> grey "silent" clock, except sleep / boot
    TEST_ASSERT_EQUAL(int(Screen::MacSilent), int(offlineScreen(Screen::Working)));
    TEST_ASSERT_EQUAL(int(Screen::MacSilent), int(offlineScreen(Screen::Permission)));
    TEST_ASSERT_EQUAL(int(Screen::MacSilent), int(offlineScreen(Screen::Multi)));
    TEST_ASSERT_EQUAL(int(Screen::MacSilent), int(offlineScreen(Screen::Clock)));
    TEST_ASSERT_EQUAL(int(Screen::MacSilent), int(offlineScreen(Screen::LimitReached)));
    TEST_ASSERT_EQUAL(int(Screen::Sleep), int(offlineScreen(Screen::Sleep)));
    TEST_ASSERT_EQUAL(int(Screen::Boot), int(offlineScreen(Screen::Boot)));
}

void test_gauge_animation_steps_toward_target() {  // bar eases to the new % in ~0.5 s, never overshoots
    TEST_ASSERT_EQUAL(46, gaugeStep(46, 46));
    int v = 20, steps = 0;
    while (v != 80 && steps < 50) { int n = gaugeStep(v, 80); TEST_ASSERT_TRUE(n > v && n <= 80); v = n; steps++; }
    TEST_ASSERT_EQUAL(80, v);
    TEST_ASSERT_TRUE(steps >= 6 && steps <= 14);  // 40 ms per step -> 0.25..0.55 s
    v = 90;
    while (v != 89) v = gaugeStep(v, 89);       // small changes still arrive
    TEST_ASSERT_EQUAL(89, gaugeStep(95, 89) >= 89 ? 89 : -1);
}

void test_session_dots() {  // top row: one dot per session (filled = busy), at most 6, then "+N"
    SessionDots d = sessionDots(2, 3);
    TEST_ASSERT_EQUAL(3, d.shown);
    TEST_ASSERT_EQUAL(2, d.filled);
    TEST_ASSERT_EQUAL(0, d.more);
    d = sessionDots(4, 9);
    TEST_ASSERT_EQUAL(6, d.shown);
    TEST_ASSERT_EQUAL(4, d.filled);
    TEST_ASSERT_EQUAL(3, d.more);
    d = sessionDots(8, 9);  // more busy than shown dots: all shown are filled
    TEST_ASSERT_EQUAL(6, d.filled);
    d = sessionDots(0, 0);
    TEST_ASSERT_EQUAL(0, d.shown);
}

void test_reset_timer_one_line() {  // fits the 26 px gauge column under the percent
    setLang(Lang::Uk);  // Ukrainian texts (English is covered in test_i18n)
    TEST_ASSERT_EQUAL_STRING("2:14", resetShort(2 * 3600 + 14 * 60 + 30, false).c_str());  // 5h window: h:mm
    TEST_ASSERT_EQUAL_STRING("1:05", resetShort(65 * 60, false).c_str());
    TEST_ASSERT_EQUAL_STRING("4:59", resetShort(5 * 3600 - 1, false).c_str());
    TEST_ASSERT_EQUAL_STRING("45хв", resetShort(45 * 60 + 10, false).c_str());            // under an hour: minutes
    TEST_ASSERT_EQUAL_STRING("3д", resetShort(3 * 86400 + 4 * 3600 + 59, true).c_str()); // 7d: days
    TEST_ASSERT_EQUAL_STRING("5г", resetShort(5 * 3600 + 7 * 60, true).c_str());         // 7d under a day: hours
    TEST_ASSERT_EQUAL_STRING("30хв", resetShort(30 * 60, true).c_str());
    TEST_ASSERT_EQUAL_STRING("0хв", resetShort(-5, false).c_str());
}

void test_reset_timer_only_from_70_and_pace() {
    TEST_ASSERT_FALSE(showResetTimer(69));
    TEST_ASSERT_TRUE(showResetTimer(70));
    // 5h window (18000 s) resets in 1 h -> 80 % of the window has passed
    TEST_ASSERT_EQUAL(80, pacePercent(10000 + 3600, 10000, 18000));
    TEST_ASSERT_EQUAL(100, pacePercent(9000, 10000, 18000));  // already past reset
    TEST_ASSERT_EQUAL(0, pacePercent(10000 + 99999, 10000, 18000));
    TEST_ASSERT_EQUAL(-1, pacePercent(0, 10000, 18000));      // unknown reset time
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_session_start_creates_idle);
    RUN_TEST(test_pretooluse_sets_working_with_tool_target);
    RUN_TEST(test_parse_event_names);
    RUN_TEST(test_unknown_event_is_noop);
    RUN_TEST(test_event_state_mapping);
    RUN_TEST(test_posttooluse_keeps_working_after_permission);
    RUN_TEST(test_idle_session_expires_after_30min);
    RUN_TEST(test_busy_session_survives_90min_expires_after_2h);
    RUN_TEST(test_session_end_removes);
    RUN_TEST(test_max_6_sessions_drops_oldest_idle);
    RUN_TEST(test_stop_returns_idle_and_records_duration);
    RUN_TEST(test_subagent_counter_never_negative);
    RUN_TEST(test_mac_silent_after_sessions_expired_without_end);
    RUN_TEST(test_busy_count);
    RUN_TEST(test_permission_wins_over_other_working_session);
    RUN_TEST(test_two_working_is_multi_one_is_working);
    RUN_TEST(test_thinking_and_compacting_are_working);
    RUN_TEST(test_your_turn_and_error);
    RUN_TEST(test_done_for_6s_then_clock);
    RUN_TEST(test_hello_for_3s);
    RUN_TEST(test_day_stays_awake_until_an_hour_without_mac);
    RUN_TEST(test_evening_sleeps_after_half_an_hour);
    RUN_TEST(test_greeting_after_a_long_silence);
    RUN_TEST(test_bash_reactions);
    RUN_TEST(test_model_remembers_reaction_tool_time_and_edits);
    RUN_TEST(test_working_shows_at_night_dimmed);
    RUN_TEST(test_night_idle_sleeps_but_your_turn_wakes);
    RUN_TEST(test_day_brightness_80);
    RUN_TEST(test_unsynced_time_is_not_night);
    RUN_TEST(test_unsynced_boot_replaces_idle_screens_only);
    RUN_TEST(test_offline_hides_stale_claude_state);
    RUN_TEST(test_reset_timer_one_line);
    RUN_TEST(test_gauge_animation_steps_toward_target);
    RUN_TEST(test_session_dots);
    RUN_TEST(test_reset_timer_only_from_70_and_pace);
    RUN_TEST(test_limit_reached_only_replaces_clock);
    RUN_TEST(test_expired_sessions_do_not_show_mac_silent);
    RUN_TEST(test_quiet_night_is_sleep);
    RUN_TEST(test_limits_clamped_and_stale);
    RUN_TEST(test_verb_for_tool);
    RUN_TEST(test_hat_season);
    RUN_TEST(test_sprites_are_14_wide);
    RUN_TEST(test_utf8_truncate);
    return UNITY_END();
}
