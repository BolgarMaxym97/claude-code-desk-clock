// Idle animations of the small mascot on the clock screen (design/idle-anim/Main.dc.html).
// Pure logic: a random scheduler picks an action every 8–20 s; pose() says what to draw for a tick.
#pragma once
#include <stdint.h>
#include "sprites.h"
#include "scene.h"

constexpr uint32_t IDLE_TICK_MS = 140;  // animation tick on the clock screen
constexpr int IDLE_MAX_DX = 55;
constexpr int IDLE_STAGE_W = 184, IDLE_STAGE_H = 70;  // stage between the gauges, mascot rests at (57, 16)         // walk range left/right of centre (stays between the gauges)

enum class IdleAct : uint8_t { Rest, Walk, Jump, Dance, Look, Wave, Doze, Star, Eat, Sport, ClockLook, Party, NewYear, Scene };  // Scene: sceneAt(sceneIndex()) draws itself

// Extra effects drawn next to the mascot; fxX/fxY are relative to the sprite's rest position (top-left).
enum : uint8_t { FX_Z = 1, FX_STAR = 2, FX_SWEAT = 4, FX_FOOD = 8, FX_CRUMB = 16, FX_SPARK = 32, FX_FLAKE = 64, FX_FIREWORK = 128 };  // FX_SPARK: fxX = blink phase 0/1

struct IdlePose {
    const SpriteFrame *frame;
    int dx, dy;     // sprite offset from its rest position, px
    uint8_t fx;     // FX_* flags
    int fxX, fxY;   // z / star / sweat position
    int foodW;      // apple width, px (FX_FOOD)
    int crumbY;     // crumbs y (FX_CRUMB), x is next to the apple
    bool band;      // wears the sport headband (hide the New Year hat)
};

class IdleDirector {
  public:
    explicit IdleDirector(uint32_t seed = 1);
    // t: idle tick (millis / IDLE_TICK_MS); minute: wall-clock minute (look at the clock when it changes);
    // tired: 5h limit >= 90 % -> drowsy rest, sweat, only calm actions.
    IdlePose pose(uint32_t t, int minute, bool tired);
    IdleAct act() const { return act_; }
    // New Year season (Dec 25 - Jan 7): snowflake instead of the star; midnight of Jan 1: dance + fireworks.
    void setCalendar(bool newYearSeason, bool newYearMidnight) { season_ = newYearSeason; midnight_ = newYearMidnight; }
    // Date / weather / events for scene selection (event scenes play once per 10 min, holidays ~every other action).
    void setContext(const SceneCtx &c) { ctx_ = c; }
    int sceneIndex() const { return scene_; }
    void playScene(uint32_t t, int i) { startScene(t, i); replays_ = 0; pendingReplay_ = false; }  // preview: once
    uint32_t sceneTick(uint32_t t) const { return t - start_; }

  private:
    uint32_t rnd();
    uint32_t range(uint32_t lo, uint32_t hi) { return lo + rnd() % (hi - lo + 1); }
    void rest(uint32_t t);
    void pick(uint32_t t, bool tired);      // new action, played PLAYS times
    void pickOne(uint32_t t, bool tired);

    uint32_t rng_;
    IdleAct act_ = IdleAct::Rest, prev_ = IdleAct::Rest;
    uint32_t start_ = 0, end_ = 0;
    bool started_ = false;
    int lastMinute_ = -1;
    bool season_ = false, midnight_ = false;
    SceneCtx ctx_;
    int scene_ = -1, prevScene_ = -1;
    bool prevEvent_ = false;
    static constexpr int MAX_SCENES = 100, MAX_EVENTS = 16;
    uint8_t eventIdx_[MAX_EVENTS] = {};     // scene index of each remembered event
    uint32_t eventAt_[MAX_EVENTS] = {};     // when it last played (0 = never)
    uint32_t &lastPlayed(int sceneIndex);
    bool startScene(uint32_t t, int i);
    int holidayScene();            // a holiday scene for today (not the previous one if there is a choice), -1 none
    void replay(uint32_t t);
    int replays_ = 0;              // replays left of the current action (each action plays PLAYS times)
    bool pendingReplay_ = false;   // resting in the short gap before a replay
    uint32_t nextBlink_ = 0, blinkUntil_ = 0;  // irregular blinking (2-6 s, sometimes double)
    int rareDay_ = -1;             // date key of today's surprise (at most one a day)
};

// Night / quiet sleep screen: slow breathing and "z"s floating up from the mascot's head.
struct SleepZ { int x, y; bool big; };  // relative to the sprite's top-right corner
struct SleepPose {
    int dy;         // 0/1: breathing
    int zCount;
    SleepZ z[3];
};
SleepPose sleepPose(uint32_t t);

// New Year decorations (stage coordinates).
struct Dot { int x, y; };
int snowFlakes(uint32_t t, Dot *out, int max);                       // falling snow behind the mascot
int fireworkDots(uint32_t t, Dot *out, uint8_t *colour, int max);    // colour 0 amber, 1 red, 2 blue
bool treeLightOn(uint32_t t, int i);                                 // garland light i
