// Scene animations (design/idle-anim Pack, Pack2, Work boards): each scene draws the mascot and its props for a tick.
// Coordinates are SCREEN px copied from the design boards; the device canvas shifts them into the 4-bit sprite
// (clock stage 28,80,184x70 or work hero 28,44,184x60) and clips. Colours are palette slots:
// 0..6 fixed (below), 7..15 = the scene's own nine colours (Scene::colours), loaded before drawing.
#pragma once
#include <stdint.h>
#include "pgm.h"
#include "sprites.h"

constexpr uint16_t C565(uint32_t rgb) {
    return uint16_t(((rgb >> 16 & 0xF8) << 8) | ((rgb >> 8 & 0xFC) << 3) | ((rgb & 0xFF) >> 3));
}

enum : uint8_t { S_BG, S_BODY, S_EYE, S_LID, S_HATR, S_HATW, S_INK, S_7, S_8, S_9, S_A, S_B, S_C, S_D, S_E, S_F };

struct Canvas {
    virtual void rect(int x, int y, int w, int h, uint8_t slot, int radius) = 0;
    virtual ~Canvas() {}
};

// Draw helpers (all coordinates screen px; floats are rounded like the design's Math.round).
void R(Canvas &c, float x, float y, int w, int h, uint8_t slot, int radius = 0);
void frame(Canvas &c, const SpriteFrame &f, int x, int y, int px, uint8_t body = S_BODY, int rows = 99);
// Pixel art from a flat PROGMEM string of `cols`-wide rows; `map` pairs a char with a slot hex digit, e.g. "w6k7".
void art(Canvas &c, const char *pgmRows, int cols, float x, float y, int s, const char *map);
int hop(uint32_t p, uint32_t n, uint32_t len, int h);  // n quick hops of len ticks, h px high, then 0

// Calendar / event context for scene selection (filled by the device each tick).
enum class Weather : uint8_t { Unknown, Clear, Rain, Snow, Hot, Wind, Storm, Fog };
struct SceneCtx {
    bool synced = false;
    int year = 0, month = 0, day = 0, wday = 0, hour = 0, min = 0;
    Weather weather = Weather::Unknown;
    bool meetingSoon = false, alert = false, limitReset = false, longSession = false;
    bool meetingNow = false, alertEnded = false;   // stage 2 inputs
    bool bored = false;      // day, 30-60 min without Mac events
    bool goodnight = false;  // the last session ended in the evening
    bool weekClosed = false; // ...and it is Friday
    int birthMonth = 0, birthDay = 0;  // 0 = not set
};

// Daily: random pool; Weekend: Sat/Sun pool; Rare: surprise, about once a day; Event / Holiday: by condition;
// Work: big mascot on the Claude screens.
enum class SceneKind : uint8_t { Daily, Event, Holiday, Work, Rare, Weekend };

// Where the mascot ended up (for the New Year hat, drawn by the caller on top).
struct MascotAt {
    int x, y;
    bool hatOk;  // false: something already on the head (band, helmet, glasses...) or the mascot is hidden
};

struct Scene {
    const char *name;       // for tests / logs
    SceneKind kind;
    bool calm;              // allowed when tired (5h >= 90 %)
    uint16_t ticks;         // length in idle ticks (140 ms)
    uint16_t colours[9];    // slots 7..15 (RGB565)
    MascotAt (*draw)(Canvas &c, uint32_t p, uint32_t t);  // p: tick since the scene started, t: global tick
    bool (*when)(const SceneCtx &ctx);                     // Event / Holiday: condition; Daily: nullptr
    const char *caption;    // holiday card text (nullptr: none)
    const char *date;       // holiday card right side ("24.08" or "")
    uint16_t cooldownMin;   // Event: at most once per N minutes (0 = 10)
};

int sceneCount();
Scene sceneAt(int i);  // copy out of flash (the tables are PROGMEM)

// Work screens: scene index for a session state and its current tool (variant: random pick for unknown tools).
enum class SState;
int workScene(SState state, const char *tool, uint32_t variant);
int sceneIndexByName(const char *name);  // -1 if missing

// Full choice for the big mascot (priority: greeting > fresh command result > done > long command > many edits >
// 3+ agents > late-night thinking > tool scene).
enum class Screen;
enum class Reaction;
struct WorkCtx {
    Screen screen;
    SState state;
    const char *tool = "";
    uint32_t variant = 0;
    Reaction reaction = Reaction(0);
    uint32_t reactionAgeMs = 0xFFFFFFFF, toolAgeMs = 0, doneDurationMs = 0;
    int edits = 0, agents = 0, hour = 12;
    bool synced = false, greeting = false, morning = false;
};
int pickWorkScene(const WorkCtx &w);

// Season background behind the small mascot (slot 15 = green in every palette). moving=false: static.
void seasonLayer(Canvas &c, int month, uint32_t t, bool moving);

// Calendar helpers (pure).
void orthodoxEaster(int year, int &month, int &day);    // Gregorian date of Orthodox Easter
int nthWeekdayOfMonth(int year, int month, int wday, int n);  // day of month (wday 0 = Sunday)
int dayOfYear(int year, int month, int day);
