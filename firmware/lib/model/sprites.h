// Mascot pixel sprites (14 px wide). Source of truth: design/Mascot.dc.html.
// Palette keys: '.' transparent, 'o' body, 'e' eye, 'b' closed lid, 'r' red eye, 'h' hat red, 'w' hat white,
// 'k' sport headband.
#pragma once
#include "pgm.h"

// Rows are one flat PROGMEM string (14 chars per row): read pixels with spx(), never index data directly.
struct SpriteFrame {
    int rows;
    const char *data;
};

inline char spx(const SpriteFrame &f, int r, int c) { return char(pgm_read_byte(f.data + r * 14 + c)); }

extern const SpriteFrame SPR_IDLE, SPR_BLINK, SPR_WALK_A, SPR_WALK_B, SPR_ARM_A, SPR_ARM_B, SPR_UP, SPR_HAPPY,
    SPR_ERR, SPR_SLEEP, SPR_HAT_IDLE, SPR_HAT_WALK;
// Idle actions on the clock screen (design/idle-anim/Main.dc.html).
extern const SpriteFrame SPR_LOOK_L, SPR_WALK_LB, SPR_ARM_L, SPR_DROWSY, SPR_EAT_A, SPR_EAT_B, SPR_BAND_A,
    SPR_BAND_B, SPR_BAND_SQ, SPR_BAND_UP;
// Scene pack (design/idle-anim Pack boards).
extern const SpriteFrame SPR_SQUISH, SPR_SHOUT, SPR_FLIP;

// New Year hat season: Dec 25 – Jan 7 inclusive.
bool hatSeason(int month, int day);
