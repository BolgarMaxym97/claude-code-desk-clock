// Scene pack, board "Pack2" (design/idle-anim/src/pack2.js): events 27-39, daily 40-54, holidays 55-63.
#include <math.h>
#include "scene.h"
#include "scenes_internal.h"

namespace {
const int X0 = 85, Y0 = 96;

const char HEART[] PROGMEM = ".x.x." "xxxxx" "xxxxx" ".xxx." "..x..";
const char CUP[] PROGMEM = "ww." "wwk" "ww.";
const char UMBRELLA[] PROGMEM = "...rrrrr..." ".rrrrrrrrr." "rrrrrrrrrrr" ".....h....." ".....h....." ".....h....." "....hh.....";
const char ICECREAM[] PROGMEM = ".pp." "pppp" "cccc" ".cc." ".cc." "..c.";
const char ZED[] PROGMEM = "xxx" ".x." "xxx";
const char HELMET[] PROGMEM = "....gggggg...." "..gggggggggg.." ".gggggggggggg." "gggggggggggggg";
const char T_O[] PROGMEM = "xx" "xx";
const char T_T[] PROGMEM = "xxx" ".x.";
const char T_I[] PROGMEM = "xxxx";
const char T_L[] PROGMEM = "x.." "xxx";
const char FISH[] PROGMEM = ".ff." "ffff" ".ff.";
const char RABBIT[] PROGMEM = "k.k" "k.k" "kkk" "kek";
const char AQFISH[] PROGMEM = ".ff.f" "ffff." ".ff.f";
const char ROCKET[] PROGMEM = "..w.." ".www." ".wbw." ".www." "r.w.r";
const char PI_[] PROGMEM = "xxxxx" ".x.x." ".x.x." ".x.x.";
const char BELL[] PROGMEM = ".bb." "bbbb" "bbbb" "b..b";
const char GIFT[] PROGMEM = ".r." "rrr" "ryr" "rrr";
const char BOUQUET[] PROGMEM = "x.x.x" "xxxxx" ".xxx.";

MascotAt M(Canvas &c, const SpriteFrame &f, int x, int y, bool hatOk = true, uint8_t body = S_BODY) {
    frame(c, f, x, y, 5, body);
    return MascotAt{x, y, hatOk};
}
const SpriteFrame &alt2(uint32_t p, const SpriteFrame &a, const SpriteFrame &b) { return ((p >> 1) % 2) ? a : b; }

void rain(Canvas &c, uint32_t p, int n, int speed, uint8_t s) {
    for (int i = 0; i < n; i++) R(c, 32 + (i * 23) % 176, 80 + (p * speed + i * 29) % 70, 1, 5, s);
}
void snow(Canvas &c, uint32_t p, int n) {
    for (int i = 0; i < n; i++)
        R(c, 32 + (i * 31) % 176 + 2 * sinf((p + i * 4) / 5.0f), 80 + (p * (1 + i % 2) + i * 19) % 70, 3, 3, S_INK);
}
void stars(Canvas &c, uint32_t p, int n, uint8_t s) {
    for (int i = 0; i < n; i++)
        if (((p + i * 3) % 10) < 6) R(c, 34 + (i * 47) % 172, 82 + (i * 13) % 26, 2, 2, s);
}
void steam(Canvas &c, uint32_t p, int x, int y, uint8_t s) {
    for (int i = 0; i < 3; i++) {
        uint32_t q = (p + i * 5) % 15;
        if (q < 10) R(c, x + ((q >> 2) % 2) * 3, y - q * 1.2f, 2, 3, s);
    }
}
void cup(Canvas &c, int x, int y, const char *map, uint8_t coffee) {
    art(c, CUP, 3, x, y, 4, map);
    R(c, x, y, 8, 2, coffee);
}

// ---- events ----
MascotAt rainScene(Canvas &c, uint32_t p, uint32_t) {  // 7 rain 8 red 9 grey a puddle
    rain(c, p, 14, 3, S_7);
    R(c, 88, 138, 64, 3, S_A, 2);
    MascotAt m = M(c, SPR_ARM_A, X0, Y0, false);
    art(c, UMBRELLA, 11, 98, 82, 4, "r8h9");
    return m;
}

MascotAt snowScene(Canvas &c, uint32_t p, uint32_t) {  // 7 dark 8 carrot
    snow(c, p, 12);
    uint32_t q = p % 90;
    R(c, 170, 128, 18, 14, S_INK, 7);
    if (q > 25) R(c, 173, 116, 12, 12, S_INK, 6);
    if (q > 50) {
        R(c, 176, 106, 8, 9, S_INK, 4);
        R(c, 178, 109, 1, 1, S_7);
        R(c, 181, 109, 1, 1, S_7);
        R(c, 184, 111, 3, 1, S_8);
    }
    return M(c, alt2(p, SPR_ARM_A, SPR_ARM_B), X0, Y0);
}

MascotAt hotScene(Canvas &c, uint32_t p, uint32_t) {  // 7 sun 8 pink 9 cone
    R(c, 34, 82, 14, 14, S_7, 7);
    for (int k = 0; k < 6; k++) {
        float a = k * 1.0472f + p / 10.0f;
        R(c, 41 + 11 * cosf(a), 89 + 11 * sinf(a), 2, 2, S_7);
    }
    MascotAt m = M(c, SPR_EAT_A, X0, Y0);
    art(c, ICECREAM, 4, 152, 104, 3, "p8c9");
    uint32_t d = p % 20;
    if (d < 14) R(c, 155, 104 + 18 + d, 2, 3, S_8);
    return m;
}

MascotAt windScene(Canvas &c, uint32_t p, uint32_t) {  // 7 streak 8 orange 9 amber a green
    for (int i = 0; i < 4; i++) R(c, 212 - (p * 4 + i * 47) % 200, 86 + i * 14, 18, 2, S_7);
    for (int i = 0; i < 3; i++)
        R(c, 212 - (p * 3 + i * 61) % 200, 92 + i * 16 + 4 * sinf((p + i * 9) / 3.0f), 5, 4, uint8_t(S_8 + i), 2);
    return M(c, SPR_LOOK_L, X0 + int(lroundf(4 * sinf(p / 3.0f))), Y0);
}

MascotAt morning(Canvas &c, uint32_t p, uint32_t) {  // 7 sun 8 brown 9 steam
    uint32_t q = p % 50;
    R(c, 176, 150 - fminf(40, q * 1.5f), 18, 18, S_7, 9);
    MascotAt m = M(c, q < 12 ? SPR_SQUISH : q < 24 ? SPR_UP : SPR_EAT_A, X0, Y0 + (q >= 12 && q < 24 ? -4 : 0));
    if (q >= 24) {
        cup(c, 152, 112, "w6k6", S_8);
        steam(c, p, 153, 108, S_9);
    }
    return m;
}

MascotAt lunch(Canvas &c, uint32_t p, uint32_t) {  // 7 bowl 8 soup 9 spoon a steam
    MascotAt m = M(c, (p % 20) < 10 ? SPR_EAT_A : SPR_EAT_B, X0, Y0);
    R(c, 146, 118, 26, 12, S_7, 6);
    R(c, 148, 116, 22, 4, S_8, 2);
    R(c, 166, 102, 3, 16, S_9);
    steam(c, p, 156, 112, S_A);
    return m;
}

MascotAt evening(Canvas &c, uint32_t p, uint32_t) {  // 7 moon 8 tea 9 brown a steam
    R(c, 186, 84, 14, 14, S_7, 7);
    R(c, 191, 82, 12, 12, S_BG, 6);
    stars(c, p, 6, S_INK);
    MascotAt m = M(c, (p % 30) < 6 ? SPR_EAT_B : SPR_EAT_A, X0, Y0);
    cup(c, 152, 112, "w8k8", S_9);
    steam(c, p, 153, 108, S_A);
    return m;
}

MascotAt friday(Canvas &c, uint32_t p, uint32_t) {  // 7 ball 8 pink 9 amber a blue b green
    R(c, 114, 80, 12, 10, S_7, 5);
    for (int i = 0; i < 10; i++) R(c, 34 + (i * 37 + p * 6) % 176, 96 + (i * 23 + p * 3) % 50, 3, 3, uint8_t(S_8 + i % 4));
    bool a = (p >> 1) % 2;
    return M(c, a ? SPR_ARM_A : SPR_ARM_L, X0, Y0 + (a ? -3 : 0));
}

MascotAt monday(Canvas &c, uint32_t p, uint32_t) {  // 7 grey
    MascotAt m = M(c, SPR_DROWSY, X0 + int(lroundf(2 * sinf(p / 6.0f))), Y0);
    if ((p % 40) < 20) art(c, ZED, 3, 160, 90 - (p % 20) / 2.0f, 2, "x7");
    return m;
}

MascotAt limitReset(Canvas &c, uint32_t p, uint32_t) {  // 7 track 8 amber 9 green a blue
    uint32_t q = p % 50;
    int fill = q * 5 > 100 ? 0 : 100 - int(q) * 5;
    R(c, 182, 88, 8, 52, S_7, 4);
    if (fill > 0) R(c, 182, 88 + 52 * (1 - fill / 100.0f), 8, 52 * fill / 100, S_BODY, 4);
    if (fill == 0) {
        R(c, 176, 84, 3, 3, S_8);
        R(c, 194, 92, 3, 3, S_9);
        R(c, 178, 98, 3, 3, S_A);
    }
    return M(c, fill == 0 ? SPR_HAPPY : SPR_WALK_A, X0, Y0 + (fill == 0 && q >= 20 ? hop(q - 20, 1, 10, 10) : 0));
}

MascotAt longSession(Canvas &c, uint32_t p, uint32_t) {  // 7 blue
    MascotAt m = M(c, SPR_DROWSY, X0, Y0 + 2);
    R(c, 88, 108, 30, 6, S_INK, 2);
    R(c, 88, 108, 6, 18, S_INK, 2);
    for (int i = 0; i < 2; i++) R(c, 140 + i * 8, 100 + ((p + i * 7) % 14) * 2, 3, 5, S_7);
    return m;
}

MascotAt meeting(Canvas &c, uint32_t p, uint32_t) {  // 7 blue
    uint32_t q = p % 40;
    R(c, 176, 82, 30, 12, S_7, 6);
    int x = X0 + int(q < 20 ? q : 40 - q) - 10;
    MascotAt m = M(c, q < 20 ? SPR_WALK_A : SPR_LOOK_L, x, Y0);
    R(c, x + 65, 108, 10, 10, S_INK, 5);
    R(c, x + 69, 110, 1, 4, S_EYE);
    R(c, x + 69, 113, 3, 1, S_EYE);
    return m;
}

MascotAt alertScene(Canvas &c, uint32_t, uint32_t) {  // 7 wall 8 brick 9 khaki
    R(c, 168, 96, 40, 44, S_7, 2);
    for (int i = 0; i < 3; i++) R(c, 172, 100 + i * 10, 32, 4, S_8);
    MascotAt m = M(c, SPR_SQUISH, X0, Y0 + 4, false);
    art(c, HELMET, 14, 85, 87, 5, "g9");
    return m;
}

// ---- daily 40-54 ----
MascotAt tetris(Canvas &c, uint32_t p, uint32_t) {  // 7 floor 8 amber 9 pink a blue b green
    const char *P[4] = {T_O, T_T, T_I, T_L};
    const int W[4] = {2, 3, 4, 3};
    const char *MAP[4] = {"x8", "x9", "xa", "xb"};
    uint32_t q = p % 60;
    int piece = int(q / 15);
    R(c, 166, 146, 42, 3, S_7);
    for (int k = 0; k < piece && k < 3; k++) art(c, P[k], W[k], 168 + k * 10, 138, 4, MAP[k]);
    art(c, P[piece % 4], W[piece % 4], 176, 84 + (q % 15) * 3.6f, 4, MAP[piece % 4]);
    return M(c, SPR_WALK_A, X0, Y0);
}

MascotAt fishing(Canvas &c, uint32_t p, uint32_t) {  // 7 rod 8 line 9 red a water b amber
    int bob = int(lroundf(2 * sinf(p / 2.0f)));
    uint32_t q = p % 60;
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    R(c, 170, 140, 42, 10, S_A, 2);
    R(c, 152, 100, 40, 2, S_7);
    R(c, 191, 100, 1, 30 + bob, S_8);
    R(c, 189, 130 + bob, 5, 5, S_9, 3);
    if (q > 40 && q < 52) art(c, FISH, 4, 178 + (q - 40), 128 + hop(q - 40, 1, 12, 16), 3, "fb");
    return m;
}

MascotAt telescope(Canvas &c, uint32_t p, uint32_t) {  // 7 tube 8 eyepiece 9 legs
    stars(c, p, 7, S_INK);
    if ((p % 40) < 8) R(c, 200 - (p % 40) * 8, 84 + (p % 40), 6, 1, S_INK);
    MascotAt m = M(c, SPR_WALK_A, X0, Y0);
    R(c, 150, 100, 26, 7, S_7, 3);
    R(c, 174, 96, 8, 10, S_8, 2);
    R(c, 158, 106, 2, 30, S_9);
    R(c, 152, 132, 14, 2, S_9);
    return m;
}

MascotAt easel(Canvas &c, uint32_t p, uint32_t) {  // 7 wood 8 red 9 blue a green b amber
    MascotAt m = M(c, alt2(p, SPR_ARM_A, SPR_ARM_B), X0, Y0);
    R(c, 170, 88, 30, 34, S_INK, 1);
    R(c, 172, 122, 2, 22, S_7);
    R(c, 196, 122, 2, 22, S_7);
    R(c, 184, 82, 2, 8, S_7);
    for (int k = 0; k < 4; k++)
        if ((p % 64) > uint32_t(8 + k * 12)) R(c, 174 + (k % 2) * 10, 92 + (k / 2) * 12, 12, 8, uint8_t(S_8 + k), 3);
    return m;
}

MascotAt castle(Canvas &c, uint32_t p, uint32_t) {  // 7 amber 8 blue 9 green a pink b red
    const int P[6][2] = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {2, 1}, {1, 2}};
    int nb = int((p % 70) / 9);
    if (nb > 6) nb = 6;
    for (int i = 0; i < nb; i++) R(c, 168 + P[i][0] * 12, 136 - P[i][1] * 12, 11, 11, i == 5 ? S_INK : uint8_t(S_7 + i), 1);
    return M(c, ((p >> 2) % 2) ? SPR_ARM_A : SPR_WALK_A, X0, Y0);
}

MascotAt rubik(Canvas &c, uint32_t p, uint32_t) {  // 7..c six colours
    MascotAt m = M(c, SPR_EAT_A, X0, Y0);
    for (int i = 0; i < 9; i++) R(c, 148 + (i % 3) * 6, 104 + (i / 3) * 6, 5, 5, uint8_t(S_7 + (i + (p >> 2)) % 6));
    return m;
}

MascotAt dumbbells(Canvas &c, uint32_t p, uint32_t) {  // 7 plate 8 bar
    bool up = (p >> 2) % 2;
    MascotAt m = M(c, up ? SPR_SHOUT : SPR_IDLE, X0, Y0);
    int y = up ? 96 : 110;
    R(c, 80, y, 4, 10, S_7);
    R(c, 84, y + 3, 6, 4, S_8);
    R(c, 150, y, 4, 10, S_7);
    R(c, 146, y + 3, 6, 4, S_8);
    return m;
}

MascotAt headphones(Canvas &c, uint32_t p, uint32_t) {  // 7 band 8 red
    int dy = ((p >> 1) % 2) ? 2 : 0;
    MascotAt m = M(c, SPR_IDLE, X0, Y0 + dy, false);
    R(c, 88, 88 + dy, 64, 3, S_7, 2);
    R(c, 84, 96 + dy, 8, 14, S_8, 3);
    R(c, 148, 96 + dy, 8, 14, S_8, 3);
    return m;
}

MascotAt tophat(Canvas &c, uint32_t p, uint32_t) {  // 7 black 8 red
    uint32_t q = p % 60;
    MascotAt m = M(c, q > 20 && q < 30 ? SPR_ARM_B : (q >= 30 ? SPR_HAPPY : SPR_WALK_A), X0, Y0);
    if (q >= 30) art(c, RABBIT, 3, 174, 112 - fminf(16, (q - 30) * 3.0f), 4, "k6e2");
    R(c, 166, 128, 26, 4, S_7, 1);
    R(c, 170, 112, 18, 16, S_7, 1);
    R(c, 170, 124, 18, 3, S_8);
    if (q > 20 && q < 30) R(c, 160 - (q % 3), 104, 2, 14, S_INK);
    return m;
}

MascotAt newspaper(Canvas &c, uint32_t p, uint32_t) {  // 7 paper 8 header 9 text
    MascotAt m = M(c, ((p >> 3) % 2) ? SPR_LOOK_L : SPR_WALK_A, X0, Y0);
    R(c, 92, 106, 56, 32, S_7, 1);
    R(c, 94, 108, 52, 6, S_8);
    for (int i = 0; i < 4; i++) R(c, 94 + (i % 2) * 27, 118 + (i / 2) * 9, 24, 3, S_9);
    return m;
}

MascotAt aquarium(Canvas &c, uint32_t p, uint32_t) {  // 7 water 8 blue 9 orange a amber
    R(c, 166, 104, 42, 36, S_7, 2);
    R(c, 166, 104, 42, 2, S_8);
    for (int i = 0; i < 2; i++) art(c, AQFISH, 5, 172 + (p * (1 + i) + i * 20) % 30, 116 + i * 10, 2, i ? "fa" : "f9");
    for (int i = 0; i < 2; i++) R(c, 176 + i * 16, 136 - ((p + i * 7) % 14) * 2, 3, 3, S_INK, 2);
    return M(c, SPR_WALK_A, X0, Y0);
}

MascotAt handheld(Canvas &c, uint32_t p, uint32_t) {  // 7 body 8 red 9 blue a..c screen
    MascotAt m = M(c, (p % 4) < 2 ? SPR_EAT_A : SPR_EAT_B, X0, Y0);
    R(c, 140, 110, 26, 16, S_7, 4);
    R(c, 146, 112, 14, 10, uint8_t(S_A + (p >> 2) % 3));
    R(c, 142, 116, 3, 3, S_8, 2);
    R(c, 161, 115, 3, 3, S_9, 2);
    return m;
}

MascotAt rocket(Canvas &c, uint32_t p, uint32_t) {  // 7 blue 8 red 9 amber a orange
    uint32_t q = p % 60;
    float ry = q < 20 ? 116 : 116 - (q - 20) * 3.0f;
    art(c, ROCKET, 5, 172, ry, 3, "w6b7r8");
    if (q >= 16) {
        R(c, 178, ry + 15, 3, 4 + int(p % 3) * 2, S_9);
        R(c, 177, ry + 15, 5, 3, S_A);
    }
    return M(c, q < 20 ? SPR_WALK_A : SPR_UP, X0, Y0);
}

MascotAt selfie(Canvas &c, uint32_t p, uint32_t) {  // 7 phone 8 screen
    uint32_t q = p % 40;
    MascotAt m = M(c, q > 20 ? SPR_HAPPY : SPR_ARM_A, X0, Y0);
    R(c, 152, 86, 12, 20, S_7, 2);
    R(c, 154, 88, 8, 14, S_8, 1);
    if (q > 20 && q < 24) R(c, 28, 80, 184, 70, S_INK);
    return m;
}

MascotAt catchDrop(Canvas &c, uint32_t p, uint32_t) {  // 7 blue
    float dx = 60 + ((p / 16 * 47) % 120);
    uint32_t dq = p % 16;
    R(c, dx, 80 + dq * 3, 2, 5, S_7);
    float k = dq / 10.0f > 1 ? 1 : dq / 10.0f;
    return M(c, dx > X0 + 35 ? SPR_WALK_A : SPR_LOOK_L, int(lroundf(X0 + (dx - X0 - 35) * k)), Y0);
}

// ---- holidays 55-63 ----
MascotAt aprilFools(Canvas &c, uint32_t p, uint32_t) { return M(c, (p % 40) < 30 ? SPR_FLIP : SPR_IDLE, X0, Y0, false); }

MascotAt cosmos(Canvas &c, uint32_t p, uint32_t) {  // 7 glass 8 shine
    stars(c, p, 10, S_INK);
    int y = Y0 - 4 + int(lroundf(3 * sinf(p / 4.0f)));
    R(c, 80, y - 8, 80, 52, S_7, 26);  // glass dome behind, mascot inside
    MascotAt m = M(c, SPR_IDLE, X0, y, false);
    R(c, 96, y - 4, 14, 4, S_8, 2);
    return m;
}

MascotAt piDay(Canvas &c, uint32_t, uint32_t) {  // 7 crust 8 dark 9 amber
    R(c, 160, 120, 34, 14, S_7, 6);
    R(c, 162, 116, 30, 6, S_8, 3);
    art(c, PI_, 5, 170, 98, 3, "x9");
    return M(c, SPR_HAPPY, X0, Y0);
}

MascotAt mayFourth(Canvas &c, uint32_t p, uint32_t) {  // 7 green 8 light green
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    float a = -1.2f + sinf(p / 3.0f) * 0.8f;
    for (int k = 0; k < 12; k++) R(c, 153 + k * 3 * cosf(a), 108 + k * 3 * sinf(a), 3, 3, (p % 6) < 3 ? S_7 : S_8);
    return m;
}

MascotAt vyshyvanka(Canvas &c, uint32_t, uint32_t t) {  // 7 linen 8 red 9 black
    MascotAt m = M(c, ((t + 7) % 21) < 2 ? SPR_BLINK : SPR_IDLE, X0, Y0, true, S_7);
    for (int k = 0; k < 12; k++) R(c, 90 + k * 5, 112 + (k % 2) * 4, 4, 3, k % 3 ? S_8 : S_9);
    R(c, 117, 106, 6, 6, S_8, 1);
    return m;
}

MascotAt constitution(Canvas &c, uint32_t, uint32_t) {  // 7 blue 8 yellow
    MascotAt m = M(c, SPR_EAT_A, X0, Y0);
    R(c, 152, 104, 28, 32, S_7, 2);
    R(c, 164, 112, 6, 12, S_8);
    R(c, 161, 112, 12, 3, S_8);
    return m;
}

MascotAt firstSept(Canvas &c, uint32_t p, uint32_t) {  // 7 bag 8 amber 9 clapper
    int sw = int(lroundf(4 * sinf(p / 2.0f)));
    R(c, 80, 108, 12, 22, S_7, 3);
    R(c, 82, 112, 8, 4, S_8, 1);
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    art(c, BELL, 4, 156 + sw, 92, 4, "b8");
    R(c, 162 + sw, 108, 4, 4, S_9, 2);
    return m;
}

MascotAt nicholas(Canvas &c, uint32_t p, uint32_t) {  // 7 pillow 8 red 9 amber
    uint32_t q = p % 60;
    R(c, 70, 134, 100, 10, S_7, 5);
    if (q > 25) art(c, GIFT, 3, 166, 118, 4, "r8y9");
    return M(c, q < 25 ? SPR_SLEEP : SPR_HAPPY, X0, q < 25 ? Y0 + 8 : Y0, q >= 25);
}

MascotAt mothersDay(Canvas &c, uint32_t, uint32_t) {  // 7 pink 8 green 9 red
    MascotAt m = M(c, SPR_EAT_A, X0, Y0);
    art(c, BOUQUET, 5, 150, 92, 3, "x7");
    R(c, 154, 101, 2, 18, S_8);
    R(c, 160, 101, 2, 18, S_8);
    art(c, HEART, 5, 176, 100, 3, "x9");
    return m;
}

bool on(const SceneCtx &x, int m, int d) { return x.synced && x.month == m && x.day == d; }
bool wRain(const SceneCtx &x) { return x.weather == Weather::Rain; }
bool wSnow(const SceneCtx &x) { return x.weather == Weather::Snow; }
bool wHot(const SceneCtx &x) { return x.weather == Weather::Hot; }
bool wWind(const SceneCtx &x) { return x.weather == Weather::Wind; }
bool tMorning(const SceneCtx &x) { return x.synced && x.hour >= 6 && x.hour < 10; }
bool tLunch(const SceneCtx &x) { return x.synced && x.hour >= 12 && x.hour < 14; }
bool tEvening(const SceneCtx &x) { return x.synced && x.hour >= 19 && x.hour < 23; }
bool tFriday(const SceneCtx &x) { return x.synced && x.wday == 5 && x.hour >= 17; }
bool tMonday(const SceneCtx &x) { return x.synced && x.wday == 1 && x.hour < 11; }
bool eLimitReset(const SceneCtx &x) { return x.limitReset; }
bool eLongSession(const SceneCtx &x) { return x.longSession; }
bool eMeeting(const SceneCtx &x) { return x.meetingSoon; }
bool eAlert(const SceneCtx &x) { return x.alert; }
bool hApril1(const SceneCtx &x) { return on(x, 4, 1); }
bool hCosmos(const SceneCtx &x) { return on(x, 4, 12); }
bool hPi(const SceneCtx &x) { return on(x, 3, 14); }
bool hMay4(const SceneCtx &x) { return on(x, 5, 4); }
bool hVyshyvanka(const SceneCtx &x) { return x.synced && x.month == 5 && x.day == nthWeekdayOfMonth(x.year, 5, 4, 3); }
bool hConstitution(const SceneCtx &x) { return on(x, 6, 28); }
bool hSept1(const SceneCtx &x) { return on(x, 9, 1); }
bool hNicholas(const SceneCtx &x) { return on(x, 12, 6); }
bool hMothers(const SceneCtx &x) { return x.synced && x.month == 5 && x.day == nthWeekdayOfMonth(x.year, 5, 0, 2); }

const uint16_t K_GREY = C565(0xA8A399), K_AMB = C565(0xF0B429), K_GRN = C565(0x5FB97A), K_BLU = C565(0x6AA6F8),
               K_RED = C565(0xE5484D), K_PINK = C565(0xF08CB0), K_DARK = C565(0x3A3732), K_BROWN = C565(0x7A4A2A),
               K_WOOD = C565(0xB0603A), K_ORANGE = C565(0xE8772E), K_UB = C565(0x2E6BD8), K_UY = C565(0xF5C518);
}  // namespace

const Scene SCENES_B[] PROGMEM = {
    {"rain", SceneKind::Event, true, 60, {K_BLU, K_RED, C565(0x8A8478), C565(0x2A3A5C)}, rainScene, wRain, nullptr, nullptr, 40},
    {"snow", SceneKind::Event, true, 90, {C565(0x1C1A17), K_ORANGE}, snowScene, wSnow, nullptr, nullptr, 40},
    {"hot", SceneKind::Event, true, 60, {K_AMB, K_PINK, C565(0xD8A460)}, hotScene, wHot, nullptr, nullptr, 60},
    {"wind", SceneKind::Event, true, 60, {K_GREY, K_ORANGE, K_AMB, K_GRN}, windScene, wWind, nullptr, nullptr, 40},
    {"morning", SceneKind::Event, true, 50, {K_AMB, K_BROWN, K_GREY}, morning, tMorning, nullptr, nullptr, 120},
    {"lunch", SceneKind::Event, true, 40, {K_WOOD, C565(0xE8A060), C565(0xC8C0B0), K_GREY}, lunch, tLunch, nullptr, nullptr, 120},
    {"evening", SceneKind::Event, true, 60, {C565(0xE8E2D4), C565(0xC8E0C0), K_BROWN, K_GREY}, evening, tEvening, nullptr, nullptr, 120},
    {"friday", SceneKind::Event, false, 60, {C565(0xC8C0B0), K_PINK, K_AMB, K_BLU, K_GRN}, friday, tFriday, nullptr, nullptr, 120},
    {"monday", SceneKind::Event, true, 60, {K_GREY}, monday, tMonday, nullptr, nullptr, 120},
    {"limit-reset", SceneKind::Event, false, 50, {C565(0x2A2825), K_AMB, K_GRN, K_BLU}, limitReset, eLimitReset, nullptr, nullptr, 60},
    {"long-session", SceneKind::Event, true, 56, {K_BLU}, longSession, eLongSession, nullptr, nullptr, 120},
    {"meeting", SceneKind::Event, true, 80, {K_BLU}, meeting, eMeeting, nullptr, nullptr, 3},
    {"alert", SceneKind::Event, true, 40, {K_DARK, C565(0x4A4742), C565(0x5B6B3A)}, alertScene, eAlert, nullptr, nullptr, 5},
    {"tetris", SceneKind::Daily, false, 60, {K_DARK, K_AMB, K_PINK, K_BLU, K_GRN}, tetris, nullptr, nullptr, nullptr},
    {"fishing", SceneKind::Daily, true, 60, {K_WOOD, K_GREY, K_RED, C565(0x1F3A5C), K_AMB}, fishing, nullptr, nullptr, nullptr},
    {"telescope", SceneKind::Daily, true, 60, {C565(0xC8C0B0), C565(0x8A8478), C565(0x8A8478)}, telescope, nullptr, nullptr, nullptr},
    {"easel", SceneKind::Daily, true, 64, {K_WOOD, K_RED, K_BLU, K_GRN, K_AMB}, easel, nullptr, nullptr, nullptr},
    {"castle", SceneKind::Daily, false, 70, {K_AMB, K_BLU, K_GRN, K_PINK, K_RED}, castle, nullptr, nullptr, nullptr},
    {"rubik", SceneKind::Daily, true, 48, {K_RED, K_AMB, K_BLU, K_GRN, C565(0xF4F1EA), K_ORANGE}, rubik, nullptr, nullptr, nullptr},
    {"dumbbells", SceneKind::Daily, false, 48, {C565(0x5E5A53), C565(0x8A8478)}, dumbbells, nullptr, nullptr, nullptr},
    {"headphones", SceneKind::Daily, true, 48, {K_DARK, K_RED}, headphones, nullptr, nullptr, nullptr},
    {"tophat", SceneKind::Daily, false, 60, {C565(0x1C1A17), K_RED}, tophat, nullptr, nullptr, nullptr},
    {"newspaper", SceneKind::Daily, true, 64, {C565(0xE8E2D4), K_DARK, C565(0x9A958C)}, newspaper, nullptr, nullptr, nullptr},
    {"aquarium", SceneKind::Daily, true, 60, {C565(0x24384E), K_BLU, K_ORANGE, K_AMB}, aquarium, nullptr, nullptr, nullptr},
    {"handheld", SceneKind::Daily, true, 48, {K_DARK, K_RED, K_BLU, K_GRN, K_BLU, K_AMB}, handheld, nullptr, nullptr, nullptr},
    {"rocket", SceneKind::Daily, false, 60, {K_BLU, K_RED, K_AMB, K_ORANGE}, rocket, nullptr, nullptr, nullptr},
    {"selfie", SceneKind::Daily, false, 40, {C565(0x1C1A17), C565(0x2A3A5C)}, selfie, nullptr, nullptr, nullptr},
    {"catch-drop", SceneKind::Daily, false, 64, {K_BLU}, catchDrop, nullptr, nullptr, nullptr},
    {"april1", SceneKind::Holiday, true, 40, {}, aprilFools, hApril1, "День сміху", "01.04"},
    {"cosmos", SceneKind::Holiday, true, 60, {C565(0x2A3E52), C565(0xB8D8F0)}, cosmos, hCosmos, "День космонавтики", ""},
    {"pi", SceneKind::Holiday, true, 40, {C565(0xD8A460), K_WOOD, K_AMB}, piDay, hPi, "День числа Пі", "14.03"},
    {"may4", SceneKind::Holiday, false, 48, {C565(0x7CFF8A), C565(0xB8FFC0)}, mayFourth, hMay4, "May the 4th", "04.05"},
    {"vyshyvanka", SceneKind::Holiday, true, 40, {C565(0xEDE6D8), C565(0xC8102E), C565(0x1C1A17)}, vyshyvanka, hVyshyvanka, "День вишиванки", ""},
    {"constitution", SceneKind::Holiday, true, 40, {K_UB, K_UY}, constitution, hConstitution, "День Конституції", ""},
    {"sept1", SceneKind::Holiday, true, 48, {C565(0x3B7DD8), K_AMB, K_WOOD}, firstSept, hSept1, "День знань", "01.09"},
    {"nicholas", SceneKind::Holiday, true, 60, {C565(0xE8E2D4), K_RED, K_AMB}, nicholas, hNicholas, "День Миколая", "06.12"},
    {"mothers", SceneKind::Holiday, true, 40, {K_PINK, K_GRN, K_RED}, mothersDay, hMothers, "День матері", ""},
};
const int SCENES_B_N = sizeof(SCENES_B) / sizeof(SCENES_B[0]);
