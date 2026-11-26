#include <cstdio>
#include "model.h"
#include "i18n.h"
#include <cstring>

Ev parseEvent(const char *hookName, const char *kind) {
    struct Map { const char *name; Ev ev; };
    static const Map MAP[] = {
        {"SessionStart", Ev::SessionStart}, {"UserPromptSubmit", Ev::UserPromptSubmit},
        {"PreToolUse", Ev::PreToolUse}, {"PostToolUse", Ev::PostToolUse},
        {"PostToolUseFailure", Ev::PostToolUseFailure}, {"SubagentStart", Ev::SubagentStart},
        {"SubagentStop", Ev::SubagentStop}, {"PreCompact", Ev::PreCompact},
        {"Stop", Ev::Stop}, {"StopFailure", Ev::StopFailure}, {"SessionEnd", Ev::SessionEnd},
    };
    if (!hookName) return Ev::Unknown;
    if (std::strcmp(hookName, "Notification") == 0) {
        if (kind && std::strcmp(kind, "permission") == 0) return Ev::NotifyPermission;
        if (kind && std::strcmp(kind, "idle") == 0) return Ev::NotifyIdle;
        return Ev::Unknown;
    }
    for (const auto &m : MAP)
        if (std::strcmp(hookName, m.name) == 0) return m.ev;
    return Ev::Unknown;
}

bool isWorking(const ClaudeSession &s) {
    return s.state == SState::Thinking || s.state == SState::Working || s.state == SState::Compacting ||
           s.agents > 0;
}

// Sessions that must survive long silences: working, or blocked on the user's permission.
static bool longLived(const ClaudeSession &s) { return isWorking(s) || s.state == SState::Permission; }

ClaudeSession *Model::findMut(const std::string &id) {
    for (auto &s : sessions_)
        if (s.id == id) return &s;
    return nullptr;
}

const ClaudeSession *Model::find(const std::string &id) const {
    for (const auto &s : sessions_)
        if (s.id == id) return &s;
    return nullptr;
}

size_t Model::busyCount() const {
    size_t n = 0;
    for (const auto &s : sessions_)
        if (isWorking(s)) n++;
    return n;
}

ClaudeSession &Model::create(const std::string &id, uint32_t nowMs) {
    if (sessions_.size() >= MAX_SESSIONS) {
        // Drop the least recently active non-working session; if all work, the oldest overall.
        int victim = -1;
        for (size_t i = 0; i < sessions_.size(); i++) {
            if (longLived(sessions_[i])) continue;
            if (victim < 0 || sessions_[i].lastMs < sessions_[victim].lastMs) victim = int(i);
        }
        if (victim < 0) {
            victim = 0;
            for (size_t i = 1; i < sessions_.size(); i++)
                if (sessions_[i].lastMs < sessions_[victim].lastMs) victim = int(i);
        }
        sessions_.erase(sessions_.begin() + victim);
    }
    sessions_.push_back(ClaudeSession{});
    ClaudeSession &s = sessions_.back();
    s.id = id;
    s.sinceMs = nowMs;
    return s;
}

void Model::onEvent(const EventIn &e, uint32_t nowMs) {
    if (e.ev == Ev::Unknown || e.session.empty()) return;
    silent_ = false;
    if (everEvent_ && nowMs - lastEventMs_ >= BORED_MS) { greetSeen_ = true; greetAtMs_ = nowMs; }  // you're back
    everEvent_ = true;
    lastEventMs_ = nowMs;
    if (e.ev == Ev::SessionEnd) {
        for (size_t i = 0; i < sessions_.size(); i++)
            if (sessions_[i].id == e.session) { sessions_.erase(sessions_.begin() + i); break; }
        return;
    }
    ClaudeSession *s = findMut(e.session);
    if (!s) s = &create(e.session, nowMs);
    if (!e.project.empty()) s->project = e.project;
    if (!e.branch.empty()) s->branch = e.branch;
    s->lastMs = nowMs;

    auto setState = [&](SState st) {
        if (s->state != st) s->sinceMs = nowMs;
        s->state = st;
    };
    switch (e.ev) {
        case Ev::SessionStart:
            setState(SState::Idle);
            helloAtMs_ = nowMs;
            helloSeen_ = true;
            break;
        case Ev::UserPromptSubmit:
            s->tool.clear();
            s->edits = 0;
            s->target = e.target;
            s->promptMs = nowMs;
            s->hasPrompt = true;
            setState(SState::Thinking);
            break;
        case Ev::PreToolUse:
            s->tool = e.tool;
            s->target = e.target;
            s->toolMs = nowMs;
            if (e.tool == "Edit" || e.tool == "Write" || e.tool == "MultiEdit" || e.tool == "NotebookEdit") s->edits++;
            setState(SState::Working);
            break;
        case Ev::PostToolUse:
        case Ev::PostToolUseFailure:
            if (e.tool == "Bash") {
                bool ok = e.ev == Ev::PostToolUse;
                Reaction r = e.kinds.empty() ? classifyBash(e.target, ok) : classifyKinds(e.kinds, ok);
                if (r != Reaction::None) { reaction_ = r; reactionAtMs_ = nowMs; }
            }
            if (s->state != SState::Thinking) setState(SState::Working);
            break;
        case Ev::SubagentStart: s->agents++; break;
        case Ev::SubagentStop: if (s->agents > 0) s->agents--; break;
        case Ev::PreCompact: setState(SState::Compacting); break;
        case Ev::NotifyPermission:
            if (!e.tool.empty()) s->tool = e.tool;
            if (!e.target.empty()) s->target = e.target;
            setState(SState::Permission);
            break;
        case Ev::NotifyIdle: setState(SState::YourTurn); break;
        case Ev::Stop:
            s->agents = 0;
            setState(SState::Idle);
            doneDurationMs_ = s->hasPrompt ? nowMs - s->promptMs : 0;
            doneAtMs_ = nowMs;
            doneSeen_ = true;
            break;
        case Ev::StopFailure:
            s->agents = 0;
            setState(SState::Error);
            break;
        default: break;
    }
}

void Model::tick(uint32_t nowMs) {
    bool removed = false;
    for (size_t i = 0; i < sessions_.size();) {
        const ClaudeSession &s = sessions_[i];
        uint32_t ttl = longLived(s) ? BUSY_TTL_MS : IDLE_TTL_MS;
        if (nowMs - s.lastMs > ttl) {
            sessions_.erase(sessions_.begin() + i);
            removed = true;
        } else {
            i++;
        }
    }
    if (removed && sessions_.empty()) silent_ = true;
}

bool isNight(const LocalTime &t) {
    return t.synced && (t.hour >= NIGHT_FROM_H || t.hour < NIGHT_TO_H);
}

static int clampPct(int p) { return p < 0 ? 0 : (p > 100 ? 100 : p); }

void Model::onLimits(int h5pct, uint32_t h5reset, int d7pct, uint32_t d7reset, uint32_t nowMs) {
    h5_.pct = clampPct(h5pct);
    h5_.resetsAt = h5reset;
    d7_.pct = clampPct(d7pct);
    d7_.resetsAt = d7reset;
    hasLimits_ = true;
    limitsRxMs_ = nowMs;
}

View Model::view(uint32_t nowMs, const LocalTime &t) const {
    View v{Screen::Clock, nullptr, isNight(t), BRIGHT_DAY};
    const ClaudeSession *perm = nullptr, *work = nullptr, *turn = nullptr, *err = nullptr;
    size_t working = 0;
    for (const auto &s : sessions_) {
        if (s.state == SState::Permission && (!perm || s.sinceMs > perm->sinceMs)) perm = &s;
        if (isWorking(s)) { working++; if (!work || s.lastMs > work->lastMs) work = &s; }
        if (s.state == SState::YourTurn && !turn) turn = &s;
        if (s.state == SState::Error && !err) err = &s;
    }
    auto pick = [&](Screen sc, const ClaudeSession *f) { v.screen = sc; v.focus = f; };
    if (perm) pick(Screen::Permission, perm);
    else if (working >= 2) pick(Screen::Multi, work);
    else if (working == 1) pick(Screen::Working, work);
    else if (turn) pick(Screen::YourTurn, turn);
    else if (err) pick(Screen::Error, err);
    else if (doneSeen_ && nowMs - doneAtMs_ < DONE_SHOW_MS) pick(Screen::Done, nullptr);
    else if (helloSeen_ && nowMs - helloAtMs_ < HELLO_SHOW_MS) pick(Screen::Hello, nullptr);
    else {
        // awake (idle scenes) until an hour of quiet by day, half an hour in the evening
        bool away = nowMs - lastEventMs_ > (t.hour >= EVENING_FROM_H ? EVENING_SLEEP_MS : AWAY_SLEEP_MS);
        bool limitHit = hasLimits_ && !limitsStale(nowMs) && (h5_.pct >= 100 || d7_.pct >= 100);
        if (!t.synced) pick(Screen::Boot, nullptr);  // booting / connecting: no time yet
        else if (v.night) pick(Screen::Sleep, nullptr);
        else if (limitHit) pick(Screen::LimitReached, nullptr);
        else if (away) pick(Screen::Sleep, nullptr);
        else pick(Screen::Clock, nullptr);
    }
    v.brightness = (v.night || v.screen == Screen::Sleep) ? BRIGHT_NIGHT : BRIGHT_DAY;
    return v;
}

Screen offlineScreen(Screen s) {
    if (s == Screen::Sleep || s == Screen::Boot || s == Screen::FirstSetup) return s;
    return Screen::MacSilent;
}

std::string resetShort(long s, bool sevenDay) {
    if (s < 0) s = 0;
    long d = s / 86400, hours = s / 3600, m = s / 60 % 60;
    char o[12];
    if (sevenDay && d > 0) snprintf(o, sizeof o, "%ld%s", d, tr(T_U_DAY));
    else if (sevenDay && hours > 0) snprintf(o, sizeof o, "%ld%s", hours, tr(T_U_HOUR));
    else if (hours > 0) snprintf(o, sizeof o, "%ld:%02ld", hours, m);
    else snprintf(o, sizeof o, "%ld%s", m, tr(T_U_MIN));
    return o;
}

int gaugeStep(int shown, int target) {
    int d = target - shown;
    if (d == 0) return target;
    int step = (d > 0 ? d + 2 : d - 2) / 3;  // a third of the way, rounded away from zero -> at least 1
    return shown + step;
}

SessionDots sessionDots(int busy, int total) {
    const int MAX = 6;
    int shown = total < MAX ? total : MAX;
    int filled = busy < shown ? busy : shown;
    return SessionDots{shown, filled, total - shown};
}

int pacePercent(uint32_t resetsAt, uint32_t now, uint32_t window) {
    if (!resetsAt || !window) return -1;
    long left = long(resetsAt) - long(now);
    if (left <= 0) return 100;
    if (left >= long(window)) return 0;
    return int(100 - left * 100 / long(window));
}

static bool hasToken(const std::string &t, const char *const *words) {
    size_t i = 0;
    while (i < t.size()) {
        while (i < t.size() && (t[i] == ' ' || t[i] == '&' || t[i] == ';' || t[i] == '|')) i++;
        size_t j = i;
        while (j < t.size() && t[j] != ' ' && t[j] != '&' && t[j] != ';' && t[j] != '|') j++;
        std::string tok = t.substr(i, j - i);
        size_t slash = tok.rfind('/');
        std::string base = slash == std::string::npos ? tok : tok.substr(slash + 1);
        for (const char *const *w = words; *w; w++) {
            size_t n = strlen(*w);
            if (base == *w || (base.size() > n && base.compare(0, n, *w) == 0 && (base[n] == '.' || base[n] == '_'))) return true;
        }
        i = j;
    }
    return false;
}

Reaction classifyKinds(const std::string &kinds, bool ok) {
    auto has = [&kinds](const char *k) {
        size_t n = std::strlen(k), p = 0;
        while ((p = kinds.find(k, p)) != std::string::npos) {
            bool start = p == 0 || kinds[p - 1] == ',', end = p + n == kinds.size() || kinds[p + n] == ',';
            if (start && end) return true;
            p += n;
        }
        return false;
    };
    if (!ok) return has("tests") ? Reaction::TestsFail : Reaction::None;
    if (has("push")) return Reaction::Push;
    if (has("commit")) return Reaction::Commit;
    if (has("tests")) return Reaction::TestsOk;
    if (has("install")) return Reaction::Install;
    if (has("docker")) return Reaction::Docker;
    return Reaction::None;
}

Reaction classifyBash(const std::string &t, bool ok) {
    static const char *const TEST[] = {"test", "tests", "pytest", "phpunit", "pest", "jest", "vitest", "unittest", nullptr};
    static const char *const INSTALL[] = {"install", nullptr};
    static const char *const PM[] = {"npm", "pip", "pip3", "brew", "yarn", "pnpm", "bun", "composer", "apt", "apt-get", "cargo", nullptr};
    static const char *const ADD[] = {"add", "i", "require", nullptr};
    static const char *const DOCKER[] = {"docker", "docker-compose", nullptr};
    if (hasToken(t, TEST)) return ok ? Reaction::TestsOk : Reaction::TestsFail;
    if (!ok) return Reaction::None;
    if (t.find("git push") != std::string::npos) return Reaction::Push;  // "commit && push": the push is the news
    if (t.find("git commit") != std::string::npos) return Reaction::Commit;
    if (hasToken(t, PM) && (hasToken(t, INSTALL) || hasToken(t, ADD))) return Reaction::Install;
    if (hasToken(t, DOCKER)) return Reaction::Docker;
    return Reaction::None;
}
