#include "sprites.h"

// Frames live in flash (PROGMEM): one string of 14-char rows.
#define FRAME(name, rows)                              \
    static const char name##_DATA[] PROGMEM = rows;   \
    const SpriteFrame name = {int((sizeof(name##_DATA) - 1) / 14), name##_DATA};

FRAME(SPR_IDLE, "..oooooooooo.." "..ooeooooeoo.." "..ooeooooeoo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_BLINK, "..oooooooooo.." "..oooooooooo.." "..ooeooooeoo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_WALK_A, "..oooooooooo.." "..oooeooooeo.." "..oooeooooeo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_WALK_B, "..oooooooooo.." "..oooeooooeo.." "..oooeooooeo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "...o.o..o.o..." "...o.o..o.o...")
FRAME(SPR_ARM_A, "..oooooooooo.o" "..ooeooooeoo.o" "..ooeooooeoo.o" "ooooooooooooo." "oooooooooooo.." "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_ARM_B, "..oooooooooo.." "..ooeooooeoo.o" "..ooeooooeoo.o" "oooooooooooooo" "oooooooooooo.." "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_UP, "..ooeooooeoo.." "..ooeooooeoo.." "..oooooooooo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_HAPPY, "o.oooooooooo.o" "o.ooeooooeoo.o" "o.oeoeooeoeo.o" "oooooooooooooo" "..oooooooooo.." "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_ERR, "..oooooooooo.." "..ooroooorooo." "..ooroooorooo." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_SLEEP, ".............." ".............." ".............." "..oooooooooo.." "..oobboobboo.." "oooooooooooooo" "..oooooooooo.." "..oo......oo..")
FRAME(SPR_HAT_IDLE, "..........hw.." ".....hhhhhh..." "..wwwwwwwwww.." "..ooeooooeoo.." "..ooeooooeoo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_HAT_WALK, ".........hw..." ".....hhhhhh..." "..wwwwwwwwww.." "..oooeooooeo.." "..oooeooooeo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "...o.o..o.o..." "...o.o..o.o...")

FRAME(SPR_LOOK_L, "..oooooooooo.." "..oeooooeooo.." "..oeooooeooo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_WALK_LB, "..oooooooooo.." "..oeooooeooo.." "..oeooooeooo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "...o.o..o.o..." "...o.o..o.o...")
FRAME(SPR_ARM_L, "o.oooooooooo.." "o.ooeooooeoo.." "o.ooeooooeoo.." ".ooooooooooooo" "..oooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_DROWSY, "..oooooooooo.." "..oooooooooo.." "..oobboobboo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_EAT_A, "..oooooooooo.." "..ooeooooeoo.." "..ooeooooeoo.." "ooooooooooooo." "oooooooooooooo" "..oooooooooooo" "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_EAT_B, "..oooooooooo.." "..oooooooooo.." "..oobboobboo.." "ooooooooooooo." "oooooooooooooo" "..oooooooooooo" "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_BAND_A, "o.kkkkkkkkkk.o" "o.ooeooooeoo.o" "o.ooeooooeoo.o" "oooooooooooooo" "..oooooooooo.." "..oooooooooo.." ".o.o......o.o." "o..o......o..o")
FRAME(SPR_BAND_B, "..kkkkkkkkkkkk" "..ooeooooeook." "..ooeooooeoo.." "oooooooooooooo" "oooooooooooooo" "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_BAND_SQ, ".............." ".kkkkkkkkkkkkk" ".oooeooooeook." "oooooooooooooo" "oooooooooooooo" ".oooooooooooo." ".oo.o....o.oo." "..............")
FRAME(SPR_BAND_UP, "..kkkkkkkkkkkk" "..ooeooooeoo.k" "..ooeooooeoo.." "..oooooooooo.." "oooooooooooooo" "oooooooooooooo" "..o.o....o.o.." "..o.o....o.o..")

FRAME(SPR_SQUISH, ".............." ".oooooooooooo." ".oooeooooeooo." "oooooooooooooo" "oooooooooooooo" ".oooooooooooo." "..o.o....o.o.." "..............")
FRAME(SPR_SHOUT, "o.oooooooooo.o" "o.ooeooooeoo.o" "o.ooeooooeoo.o" "oooooooooooooo" "..oooooooooo.." "..oooooooooo.." "..o.o....o.o.." "..o.o....o.o..")
FRAME(SPR_FLIP, "..o.o....o.o.." "..o.o....o.o.." "..oooooooooo.." "oooooooooooooo" "oooooooooooooo" "..ooeooooeoo.." "..ooeooooeoo.." "..oooooooooo..")

bool hatSeason(int month, int day) { return (month == 12 && day >= 25) || (month == 1 && day <= 7); }
