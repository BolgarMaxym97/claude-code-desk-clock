#include "idle.h"
#include <math.h>
#include <string.h>

// Durations in idle ticks (140 ms).
static uint32_t duration(IdleAct a) {
    switch (a) {
        case IdleAct::Walk: return 80;
        case IdleAct::Doze: return 60;
        case IdleAct::Star: return 50;
        case IdleAct::Eat: return 56;
        case IdleAct::Sport: return 60;
        case IdleAct::ClockLook: return 14;
        case IdleAct::Party: return 44;
        default: return 40;  // Jump, Dance, Look, Wave
    }
}

static const IdleAct ALL[] = {IdleAct::Walk, IdleAct::Jump, IdleAct::Dance, IdleAct::Look, IdleAct::Wave,
                              IdleAct::Doze, IdleAct::Star, IdleAct::Eat, IdleAct::Sport};
static const IdleAct CALM[] = {IdleAct::Look, IdleAct::Wave, IdleAct::Doze, IdleAct::Eat};
static uint32_t cooldownTicks(const Scene &s) { return (s.cooldownMin ? s.cooldownMin : 10) * 60000u / IDLE_TICK_MS; }
static const uint32_t RARE_PER_MILLE = 4;   // chance per new action: ~0.7 a day -> a surprise every couple of days
static const uint32_t WEEKEND_PERCENT = 40; // Sat/Sun: share of weekend scenes
static const int PLAYS = 3;                                         // each action plays 3 times in a row...
static const uint32_t GAP_MIN = 2000 / IDLE_TICK_MS + 1, GAP_MAX = 4000 / IDLE_TICK_MS;  // ...2-4 s apart
// Pause between actions: 1.5-4 min. A 3x action moves ~25-40 s, so the mascot moves ~15-20 % of the time:
// alive when you glance at it, not a constant motion in the corner of your eye.
static const uint32_t REST_MIN = 90000 / IDLE_TICK_MS + 1, REST_MAX = 240000 / IDLE_TICK_MS;

// hop() comes from scene.h (shared with the scene pack).

IdleDirector::IdleDirector(uint32_t seed) : rng_(seed ? seed : 1) {}

uint32_t IdleDirector::rnd() {  // xorshift32
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

void IdleDirector::rest(uint32_t t) {
    act_ = IdleAct::Rest;
    start_ = t;
    end_ = t + range(REST_MIN, REST_MAX);
}

uint32_t &IdleDirector::lastPlayed(int sceneIndex) {
    for (int k = 0; k < MAX_EVENTS; k++)
        if (eventAt_[k] && eventIdx_[k] == sceneIndex) return eventAt_[k];
    for (int k = 0; k < MAX_EVENTS; k++)
        if (!eventAt_[k]) { eventIdx_[k] = uint8_t(sceneIndex); return eventAt_[k]; }
    return eventAt_[0];
}

bool IdleDirector::startScene(uint32_t t, int i) {
    act_ = IdleAct::Scene;
    prev_ = IdleAct::Scene;
    scene_ = prevScene_ = i;
    start_ = t;
    end_ = t + sceneAt(i).ticks;
    if (sceneAt(i).kind == SceneKind::Event) lastPlayed(i) = t | 1;
    return true;
}

static bool urgent(const Scene &s) { return !strcmp(s.name, "alert") || !strcmp(s.name, "meeting"); }

int IdleDirector::holidayScene() {
    int hol[8], nh = 0;
    for (int i = 0; i < sceneCount() && nh < 8; i++) {
        Scene s = sceneAt(i);
        if (s.kind == SceneKind::Holiday && s.when(ctx_)) hol[nh++] = i;
    }
    if (!nh) return -1;
    int k = int(rnd() % uint32_t(nh));
    if (nh > 1 && hol[k] == prevScene_) k = (k + 1 + int(rnd() % uint32_t(nh - 1))) % nh;  // a different one
    return hol[k];
}

void IdleDirector::replay(uint32_t t) {
    pendingReplay_ = false;
    if (replays_ > 0) replays_--;
    if (prev_ == IdleAct::Scene && prevScene_ >= 0) { startScene(t, prevScene_); return; }
    act_ = prev_;
    start_ = t;
    end_ = t + duration(act_);
}

void IdleDirector::pick(uint32_t t, bool tired) {
    pickOne(t, tired);
    replays_ = PLAYS - 1;
    pendingReplay_ = false;
}

void IdleDirector::pickOne(uint32_t t, bool tired) {
    const int n = sceneCount();
    bool wasEvent = prevEvent_;
    prevEvent_ = false;
    const int today = holidayScene();
    // 1) event scenes (weather, time of day, limits, meeting, alert): when their condition holds, not back to back
    if (!wasEvent)
        for (int i = 0; i < n && i < MAX_SCENES; i++) {
            const Scene &s = sceneAt(i);
            if (s.kind != SceneKind::Event || !s.when(ctx_) || (tired && !s.calm)) continue;
            if (today >= 0 && !urgent(s)) continue;  // holiday: only the alert / meeting may interrupt
            uint32_t last = lastPlayed(i);
            if (last && t - last < cooldownTicks(s)) continue;
            prevEvent_ = true;
            startScene(t, i);
            return;
        }
    // 2) holiday today: only the holiday scene(s) all day
    if (today >= 0) { startScene(t, today); return; }
    // 3) a rare surprise: at most one a day
    if (ctx_.synced) {
        int key = ctx_.year * 400 + ctx_.month * 32 + ctx_.day;
        if (key != rareDay_ && rnd() % 1000 < RARE_PER_MILLE) {
            int rare[16], nr = 0;
            for (int i = 0; i < n && nr < 16; i++) {
                Scene s = sceneAt(i);
                if (s.kind == SceneKind::Rare && (!tired || s.calm)) rare[nr++] = i;
            }
            if (nr) { rareDay_ = key; startScene(t, rare[rnd() % uint32_t(nr)]); return; }
        }
    }
    // 4) weekend: lazy weekend scenes part of the time
    if (ctx_.synced && (ctx_.wday == 0 || ctx_.wday == 6) && rnd() % 100 < WEEKEND_PERCENT) {
        int wk[8], nw = 0;
        for (int i = 0; i < n && nw < 8; i++)
            if (sceneAt(i).kind == SceneKind::Weekend && i != prevScene_) wk[nw++] = i;
        if (nw) { startScene(t, wk[rnd() % uint32_t(nw)]); return; }
    }
    // 5) random: classic actions + daily scenes, never the same twice in a row
    const IdleAct *list = tired ? CALM : ALL;
    int legacy = int(tired ? sizeof CALM / sizeof CALM[0] : sizeof ALL / sizeof ALL[0]);
    int daily[MAX_SCENES], nd = 0;
    for (int i = 0; i < n && i < MAX_SCENES; i++) {
        const Scene &s = sceneAt(i);
        if (s.kind == SceneKind::Daily && (!tired || s.calm)) daily[nd++] = i;
    }
    for (int tries = 0; tries < 20; tries++) {
        int k = int(rnd() % uint32_t(legacy + nd));
        if (k < legacy) {
            IdleAct a = list[k];
            if (a == prev_) continue;
            prev_ = act_ = a;
            prevScene_ = -1;
            start_ = t;
            end_ = t + duration(a);
            return;
        }
        int i = daily[k - legacy];
        if (i == prevScene_) continue;
        startScene(t, i);
        return;
    }
    prev_ = act_ = list[0] == prev_ ? list[1] : list[0];  // unreachable in practice
    prevScene_ = -1;
    start_ = t;
    end_ = t + duration(act_);
}

IdlePose IdleDirector::pose(uint32_t t, int minute, bool tired) {
    if (!started_) { started_ = true; rest(t); }
    if (act_ != IdleAct::Rest && t >= end_) {
        bool special = act_ == IdleAct::ClockLook || act_ == IdleAct::Party || act_ == IdleAct::NewYear;
        if ((!special && replays_ > 0) || (act_ == IdleAct::ClockLook && pendingReplay_)) {
            pendingReplay_ = true;  // short gap, then the same action again
            act_ = IdleAct::Rest;
            start_ = t;
            end_ = t + range(GAP_MIN, GAP_MAX);
        } else if (act_ == IdleAct::ClockLook) {  // a glance at the clock: back to the rest it interrupted
            act_ = IdleAct::Rest;
            start_ = t;
            end_ = resumeEnd_ > t ? resumeEnd_ : t;
        } else {
            pendingReplay_ = false;
            rest(t);
        }
    }
    bool newMinute = lastMinute_ >= 0 && minute != lastMinute_;
    if (midnight_) {  // New Year minute: keep dancing
        if (act_ != IdleAct::NewYear) { act_ = IdleAct::NewYear; start_ = t; }
        end_ = t + 100000;  // ends when the flag drops
    } else if (act_ == IdleAct::NewYear) {
        rest(t);
    } else if (newMinute && minute == 0) {  // full hour: party (holiday: the holiday scene), interrupts any action
        int h = holidayScene();
        if (h >= 0) {
            startScene(t, h);
            replays_ = PLAYS - 1;
        } else {
            act_ = IdleAct::Party;
            start_ = t;
            end_ = t + duration(act_);
            replays_ = 0;
        }
        pendingReplay_ = false;
    } else if (act_ == IdleAct::Rest) {
        if (newMinute) {
            resumeEnd_ = end_;  // the glance must not restart the 1.5-4 min pause
            act_ = IdleAct::ClockLook;
            start_ = t;
            end_ = t + duration(act_);
        } else if (t >= end_) {
            if (pendingReplay_) replay(t);
            else pick(t, tired);
        }
    }
    lastMinute_ = minute;

    const uint32_t p = t - start_;
    const bool alt = (p >> 1) & 1;
    IdlePose o{&SPR_IDLE, 0, 0, 0, 0, 0, 0, 0, false};
    switch (act_) {
        case IdleAct::Rest:
            if (tired) {
                o.frame = (t % 40) < 3 ? &SPR_BLINK : &SPR_DROWSY;
                o.dy = 2;
                if ((t % 30) < 22) { o.fx = FX_SWEAT; o.fxX = 57; o.fxY = 4 + int(t % 30); }
            } else {
                if (t >= nextBlink_) {  // irregular like a real blink: 2-6 s, now and then a double blink
                    blinkUntil_ = t + 2;
                    nextBlink_ = rnd() % 100 < 15 ? t + 3 : t + range(2000 / IDLE_TICK_MS, 6000 / IDLE_TICK_MS);
                }
                o.frame = t < blinkUntil_ ? &SPR_BLINK : &SPR_IDLE;
            }
            break;
        case IdleAct::Walk: {
            // centre -> right -> left -> centre
            bool right = p < 20 || p >= 60;
            if (p < 20) o.dx = IDLE_MAX_DX * int(p) / 20;
            else if (p < 60) o.dx = IDLE_MAX_DX - 2 * IDLE_MAX_DX * int(p - 20) / 40;
            else o.dx = -IDLE_MAX_DX + IDLE_MAX_DX * int(p - 60) / 20;
            o.frame = alt ? (right ? &SPR_WALK_A : &SPR_LOOK_L) : (right ? &SPR_WALK_B : &SPR_WALK_LB);
            o.dy = alt ? 0 : -2;
            break;
        }
        case IdleAct::Jump: o.dy = hop(p, 3, 6, 14); break;
        case IdleAct::Dance:
            o.frame = alt ? &SPR_ARM_A : &SPR_ARM_L;
            o.dx = int(lroundf(6 * sinf(p / 3.0f)));
            o.dy = alt ? -3 : 0;
            break;
        case IdleAct::Look:
            o.frame = p < 10 ? &SPR_WALK_A : p < 20 ? &SPR_LOOK_L : p < 26 ? &SPR_WALK_A : &SPR_IDLE;
            break;
        case IdleAct::Wave: o.frame = p < 20 ? (alt ? &SPR_ARM_A : &SPR_ARM_B) : &SPR_IDLE; break;
        case IdleAct::Doze:
            if (p < 35) {
                o.frame = &SPR_DROWSY;
                o.dy = 2;
                if (p > 6) { o.fx = FX_Z; o.fxX = 72; o.fxY = 4 - int(p - 6) / 2; }
            } else if (p < 40) {  // startles awake
                o.dy = -5;
                o.dx = (p & 1) ? 2 : -2;
            }
            break;
        case IdleAct::Star:
            if (p < 22) { o.fx = season_ ? FX_FLAKE : FX_STAR; o.fxX = 30; o.fxY = -16 + int(p * 4 / 5); }
            if (p >= 20 && p < 30) o.dy = hop(p - 20, 1, 10, 12);
            o.frame = (p >= 22 && p < 34) ? &SPR_HAPPY : p < 20 ? &SPR_UP : &SPR_IDLE;
            break;
        case IdleAct::Eat: {
            if (p < 40) {
                static const int W[] = {12, 9, 6, 3};
                int bite = p < 8 ? 0 : int((p - 8) / 10) + 1;
                if (bite > 3) bite = 3;
                bool chew = p >= 8 && ((p - 8) % 10) < 4;
                o.frame = chew ? &SPR_EAT_B : &SPR_EAT_A;
                o.dy = chew ? 1 : 0;
                o.fx = FX_FOOD | (chew ? FX_CRUMB : 0);
                o.foodW = W[bite];
                o.crumbY = chew ? 26 + int((p - 8) % 10) * 3 : 0;
            } else {
                o.frame = &SPR_HAPPY;
                o.dy = hop(p - 40, 1, 8, 8);
            }
            break;
        }
        case IdleAct::Sport:
            o.band = true;
            o.fx = FX_SWEAT;
            o.fxX = 55;
            o.fxY = -4 + int(p % 14) * 2;
            if (p < 28) {  // jumping jacks
                o.frame = alt ? &SPR_BAND_A : &SPR_BAND_B;
                o.dy = alt ? -5 : 0;
            } else {  // squats
                bool down = ((p - 28) >> 2) & 1;
                o.frame = down ? &SPR_BAND_SQ : &SPR_BAND_UP;
                o.dy = down ? 6 : -2;
            }
            break;
        case IdleAct::ClockLook: o.frame = &SPR_UP; break;
        case IdleAct::Scene: break;  // drawn by sceneAt(scene_).draw
        case IdleAct::NewYear:
        case IdleAct::Party:
            o.frame = alt ? &SPR_ARM_A : &SPR_ARM_L;
            o.dy = alt ? -4 : 0;
            o.fx = act_ == IdleAct::Party ? FX_SPARK : FX_FIREWORK;
            o.fxX = alt;
            break;
    }
    return o;
}

SleepPose sleepPose(uint32_t t) {
    SleepPose p{int((t / 14) % 2), 3, {}};  // breath: 1 px every ~2 s
    for (int i = 0; i < 3; i++) {
        uint32_t q = (t + 14 * i) % 42;      // each z lives ~6 s, staggered by 2 s
        p.z[i] = SleepZ{2 + int(q / 6) + int((q / 7) & 1), -int(q * 40 / 42), q > 20};
    }
    return p;
}

int snowFlakes(uint32_t t, Dot *out, int max) {
    int n = max < 12 ? max : 12;
    for (int i = 0; i < n; i++) {
        uint32_t sp = 1 + i % 3;  // three depths: slow / mid / fast
        int x = 6 + (i * 53) % (IDLE_STAGE_W - 14) + int(lroundf(2 * sinf((t + i * 7) / 6.0f)));
        out[i] = Dot{x, int((i * 37 + t * sp) % (IDLE_STAGE_H - 4))};
    }
    return n;
}

int fireworkDots(uint32_t t, Dot *out, uint8_t *colour, int max) {
    static const int CX[] = {28, 156, 92}, CY[] = {26, 22, 14}, OFF[] = {0, 7, 14};
    int n = 0;
    for (int b = 0; b < 3; b++) {
        uint32_t q = (t + OFF[b]) % 21;
        if (q > 14) continue;  // pause between bursts
        float r = q * 1.6f;
        for (int k = 0; k < 10 && n < max; k++) {
            int x = CX[b] + int(lroundf(r * cosf(k * 0.6283f))), y = CY[b] + int(lroundf(r * sinf(k * 0.6283f)));
            if (x < 0 || y < 0 || x >= IDLE_STAGE_W - 2 || y >= IDLE_STAGE_H - 2) continue;
            out[n] = Dot{x, y};
            colour[n++] = uint8_t(b);
        }
    }
    return n;
}

bool treeLightOn(uint32_t t, int i) { return ((t >> 2) + uint32_t(i)) % 2 == 0; }
