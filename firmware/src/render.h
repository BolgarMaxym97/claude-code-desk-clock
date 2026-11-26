// Drawing primitives shared by all screens.
#pragma once
#include <TFT_eSPI.h>
#include "sprites.h"

extern TFT_eSPI tft;

// Smooth-font text. fill=false: anti-alias against bg without painting the glyph box
// (use inside shapes lower than the line height, and for the big clock digits).
void text(const uint8_t *font, const String &s, int x, int y, uint16_t fg, uint16_t bg, uint8_t datum,
          bool fill = true);
int textWidth(const uint8_t *font, const String &s);

// Top row between the gauges: left text, right text with a dot; all on lay::TOP_BASE.
void drawTopBar(const String &left, int busy, int total, uint16_t dotCol);  // dots: one per session, filled = busy
// Vertical limit gauge in a 26 px side column.
// line1/line2: reset timer under the % (nullptr/"" = none: full-height bar); pace: white mark 0..100, -1 none.
void drawGauge(int cx, const char *label, int pct, int shownPct, bool warn, bool stale, const char *timer, int pace);
void drawGaugeBar(int cx, int pct, bool warn, bool stale, int pace, bool full);  // full=false: animation step
// Mascot from a sprite frame. hat: draw the New Year hat rows (frame must be a hat frame).
void drawSprite(TFT_eSPI &g, const SpriteFrame &f, int x, int y, int px, uint16_t body);
void drawSpinner(TFT_eSPI &g, int cx, int cy, uint32_t tick, uint16_t c, uint16_t bg);
// Pixel-exact "!" (2 px wide), top-left of the bar at (cx-1, top).
void drawBang(TFT_eSPI &g, int cx, int top, uint16_t c);
