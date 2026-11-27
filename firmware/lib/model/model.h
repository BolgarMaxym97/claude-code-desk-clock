// Pure C++ state model of the Claude Clock (no Arduino deps -> unit-tested natively).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class Ev {
    SessionStart, UserPromptSubmit, PreToolUse, PostToolUse, PostToolUseFailure,
    SubagentStart, SubagentStop, PreCompact, NotifyPermission, NotifyIdle, LimitHit,
    Stop, StopFailure, SessionEnd, Unknown
};

enum class SState { Idle, Thinking, Working, Compacting, Permission, YourTurn, Error };

// What a finished Bash command meant (for a short reaction on the work screen).
enum class Reaction { None, TestsOk, TestsFail, Commit, Push, Install, Docker };
// target: the command (truncated by the hook); ok: PostToolUse (true) or PostToolUseFailure (false).
Reaction classifyBash(const std::string &target, bool ok);
// Reaction from the kinds the computer found in the whole command. ok: push > commit > tests > install > docker;
// failed: tests -> TestsFail, else none.
Reaction classifyKinds(const std::string &kinds, bool ok);

struct EventIn {
    std::string session, project, branch, tool, target;
    Ev ev;
    std::string kinds;  // finished Bash command, from the computer: "tests,commit,push,install,docker" subset
};

struct ClaudeSession {
    std::string id, project, branch, tool, target;
    SState state = SState::Idle;
    uint8_t agents = 0;
    uint32_t sinceMs = 0;   // when the current state began
    uint32_t lastMs = 0;    // last event of this session
    uint32_t promptMs = 0;  // last UserPromptSubmit (for "done in N")
    uint32_t toolMs = 0;    // when the current tool started (long Bash -> hourglass)
    int edits = 0;          // Edit/Write calls since the last prompt (many -> paper pile)
    bool hasPrompt = false;
    int8_t ctx = -1;        // context window used, % (statusline), -1 unknown
};

// hookName: Claude Code hook event name; kind: "permission" | "idle" | "" (Notification only)
Ev parseEvent(const char *hookName, const char *kind);

// Timeouts / limits (spec 6.6)
constexpr uint32_t IDLE_TTL_MS = 30u * 60u * 1000u;      // idle / your-turn / error session without events
constexpr uint32_t BUSY_TTL_MS = 2u * 3600u * 1000u;     // working session without events (long builds)
constexpr size_t MAX_SESSIONS = 6;

struct LocalTime {
    bool synced;   // false until NTP: then hour/day are bogus (1970) and must not drive night logic
    int hour, min, month, day, wday;
    int year = 0;  // e.g. 2027; 0 = unknown
};

enum class Screen {
    Boot, FirstSetup, Clock, Sleep, Working, Multi, Permission, YourTurn,
    Error, Done, Hello, LimitReached, MacSilent
};

// Screen to show while Wi-Fi is lost (after time is known): Claude's state would be stale.
Screen offlineScreen(Screen s);

// Time to the limit reset, shown under a gauge from 70 %: two short grey lines ("2г"/"14х", "3д"/"4г", "45х"/"").
// Time to the limit reset in one short line: 5h window "h:mm" ("45хв" under an hour), 7d window "3д" / "5г" / "30хв".
std::string resetShort(long secondsLeft, bool sevenDay);
// One animation step of a gauge bar toward its new % (called every ~40 ms): eases out, never overshoots.
int gaugeStep(int shown, int target);
// Top row: one dot per session, filled = busy; at most 6 dots, the rest as "+N".
struct SessionDots { int shown, filled, more; };
SessionDots sessionDots(int busy, int total);
inline bool showResetTimer(int pct) { return pct >= 70; }
// Time from the computer (sent with events) when NTP can't be reached (networks that block it): only while the
// clock has no time at all, and only a plausible value. NTP still corrects it later.
inline bool takeHostTime(uint32_t clockEpoch, uint32_t hostEpoch) { return clockEpoch < 1600000000u && hostEpoch > 1700000000u; }
// Context bar colour: 0 normal, 1 amber (>= 80 %), 2 red + "compact soon" (>= 90 %).
inline int contextLevel(int pct) { return pct >= 90 ? 2 : pct >= 80 ? 1 : 0; }
// How much of the limit window has passed, 0..100 (-1: reset time unknown) -> white pace mark on the gauge.
int pacePercent(uint32_t resetsAt, uint32_t nowEpoch, uint32_t windowSec);

struct View {
    Screen screen;
    const ClaudeSession *focus;   // session the screen is about (may be null)
    bool night;
    uint8_t brightness;     // percent
};

struct Limit {
    int pct = 0;
    uint32_t resetsAt = 0;  // epoch seconds
};

constexpr uint32_t DONE_SHOW_MS = 6000;
constexpr uint32_t HELLO_SHOW_MS = 3000;
constexpr uint32_t AWAY_SLEEP_MS = 60u * 60u * 1000u;  // day: no Mac events for an hour -> mascot sleeps too
constexpr uint32_t EVENING_SLEEP_MS = 30u * 60u * 1000u;  // after 18:00: work is over sooner
constexpr int EVENING_FROM_H = 18;
constexpr uint32_t BORED_MS = 30u * 60u * 1000u;       // day: 30 min without events -> "bored" scene
constexpr uint32_t GREET_SHOW_MS = 8000;               // first event after BORED_MS -> greeting
constexpr uint32_t LIMITS_STALE_MS = 10u * 60u * 1000u;
constexpr uint8_t BRIGHT_DAY = 80, BRIGHT_NIGHT = 20;
constexpr int NIGHT_FROM_H = 23, NIGHT_TO_H = 7;

// Sleep hours [from, to) wrapping midnight; from == to: never.
bool isNight(const LocalTime &t, int from = NIGHT_FROM_H, int to = NIGHT_TO_H);

// Claude is actively doing something in this session (drives the "working" screens).
bool isWorking(const ClaudeSession &s);

class Model {
public:
    void onEvent(const EventIn &e, uint32_t nowMs);
    void tick(uint32_t nowMs);  // expire stale sessions

    const ClaudeSession *find(const std::string &id) const;
    const std::vector<ClaudeSession> &sessions() const { return sessions_; }
    size_t count() const { return sessions_.size(); }
    size_t busyCount() const;
    // Sessions existed but all expired without SessionEnd, and nothing arrived since.
    bool macSilent(uint32_t nowMs) const { (void)nowMs; return silent_; }
    uint32_t lastDoneDurationMs() const { return doneDurationMs_; }
    uint32_t lastDoneAtMs() const { return doneAtMs_; }
    uint32_t lastHelloAtMs() const { return helloAtMs_; }
    uint32_t lastEventMs() const { return lastEventMs_; }
    uint32_t quietMs(uint32_t nowMs) const { return nowMs - lastEventMs_; }  // since the last Mac event (or boot)
    bool greeting(uint32_t nowMs) const { return greetSeen_ && nowMs - greetAtMs_ < GREET_SHOW_MS; }
    uint32_t commitCount() const { return commits_; }  // successful git commits since boot
    Reaction reaction() const { return reaction_; }
    uint32_t reactionAtMs() const { return reactionAtMs_; }
    bool everHadEvent() const { return everEvent_; }

    View view(uint32_t nowMs, const LocalTime &t) const;

    void onLimits(int h5pct, uint32_t h5reset, int d7pct, uint32_t d7reset, uint32_t nowMs);
    void onContext(const std::string &session, int pct);
    // User display prefs (settings record): sleep hours and brightness %.
    void setNight(int from, int to) { nightFrom_ = from; nightTo_ = to; }
    void setBrightness(int day, int night) { brightDay_ = uint8_t(day); brightNight_ = uint8_t(night); }  // statusline: context window used by that session
    const Limit &limit5h() const { return h5_; }
    const Limit &limit7d() const { return d7_; }
    bool hasLimits() const { return hasLimits_; }
    // The window shown on the limit screen: the fuller one (the one that ran out).
    const Limit &limitShown() const { return h5_.pct >= d7_.pct ? h5_ : d7_; }
    void setAlert(bool on) { alert_ = on; }  // air-raid alert: wakes the clock (dimmed at night)
    bool limitsStale(uint32_t nowMs) const { return !hasLimits_ || nowMs - limitsRxMs_ > LIMITS_STALE_MS; }

private:
    int nightFrom_ = NIGHT_FROM_H, nightTo_ = NIGHT_TO_H;
    uint8_t brightDay_ = BRIGHT_DAY, brightNight_ = BRIGHT_NIGHT;
    ClaudeSession *findMut(const std::string &id);
    ClaudeSession &create(const std::string &id, uint32_t nowMs);
    std::vector<ClaudeSession> sessions_;
    bool silent_ = false, everEvent_ = false, greetSeen_ = false;
    uint32_t greetAtMs_ = 0, reactionAtMs_ = 0;
    Reaction reaction_ = Reaction::None;
    uint32_t commits_ = 0;
    uint32_t doneDurationMs_ = 0, doneAtMs_ = 0, helloAtMs_ = 0, lastEventMs_ = 0;
    bool doneSeen_ = false, helloSeen_ = false;
    Limit h5_, d7_;
    bool hasLimits_ = false;
    bool alert_ = false;
    bool limitHit_ = false;  // a turn ended on the usage limit: limit screen until Claude works again or it resets
    uint32_t limitsRxMs_ = 0;
};
