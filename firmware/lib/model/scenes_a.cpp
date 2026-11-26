// Scene pack, board "Pack" (design/idle-anim/src/pack.js): daily 1-18, holidays 19-26.
// Coordinates are screen px from the design; mascot rests at (85, 96), 5 px cells.
#include <math.h>
#include <initializer_list>
#include "scene.h"
#include "scenes_internal.h"

namespace {
const int X0 = 85, Y0 = 96;
const float PI_F = 3.14159265f;

const char HEART[] PROGMEM = ".x.x." "xxxxx" "xxxxx" ".xxx." "..x..";
const char CUP[] PROGMEM = "ww." "wwk" "ww.";
const char LOGO[] PROGMEM = "x.x" ".x." "x.x";
const char PLANE[] PROGMEM = "w...." "www.." "wwwww" ".ww..";
const char FLOWER[] PROGMEM = "p.p" "ppp" ".p.";
const char CAT[] PROGMEM = "k...k.." "kk.kk.." "kekkek." "kkkkkkk" "kkkkkkt" "k.k.k.t";
const char NOTE[] PROGMEM = "..x" "..x" "..x" "xxx" "xx.";
const char BULB[] PROGMEM = ".yyy." "yyyyy" "yyyyy" ".yyy." "..g.." ".ggg.";
const char GUITAR[] PROGMEM =
    "..bb.bb............."
    ".bbbbbbbb..........."
    "bbbbbbbbbb.......hhh"
    "bbbbbooobbnfnfnfnhhh"
    "bbbbbooobbnfnfnfnhhh"
    "bbbbbbbbbb.......hhh"
    ".bbbbbbbb..........."
    "..bb.bb.............";
const char TULIP[] PROGMEM = "x.x" "xxx" "xxx";
const char EGG[] PROGMEM =
    "...www..." "..wwwww.." ".wwwwwww." ".wwwwwww." "wwwwwwwww" "wwwwwwwww" "wwwwwwwww" "wwwwwwwww" ".wwwwwww." ".wwwwwww." "..wwwww..";
const char EGG_PAT[] PROGMEM =
    "...rrr..." "..ryryr.." ".rrrrrrr." ".kykykyk." "kykkkkyky" "yrrrrrrry" "kykkkkyky" ".kykykyk." ".rrrrrrr." ".ryryryr." "..rrrrr..";
const char UA_HEART[] PROGMEM = ".bb.bb." "bbbbbbb" "bbbbbbb" "yyyyyyy" ".yyyyy." "..yyy.." "...y...";
const char PUMPKIN[] PROGMEM = "...g..." ".ooooo." "ooeoeoo" "ooooooo" "oeeeeeo" ".ooooo.";
const char CAKE[] PROGMEM = "..f..f.." "..c..c.." "wwwwwwww" "pppppppp" "wwwwwwww" "pppppppp";

MascotAt M(Canvas &c, const SpriteFrame &f, int x, int y, bool hatOk = true, uint8_t body = S_BODY) {
    frame(c, f, x, y, 5, body);
    return MascotAt{x, y, hatOk};
}
const SpriteFrame &alt2(uint32_t p, const SpriteFrame &a, const SpriteFrame &b) { return ((p >> 1) % 2) ? a : b; }

void laptop(Canvas &c, uint8_t grey, uint8_t dark) {
    R(c, 94, 114, 52, 22, grey, 2);
    R(c, 88, 136, 64, 3, dark, 1);
    art(c, LOGO, 3, 115, 120, 3, "x1");
}

// ---- daily ----
MascotAt coffee(Canvas &c, uint32_t p, uint32_t) {  // 7 brown 8 steam
    uint32_t p1 = p % 40;
    bool sip = p1 >= 20 && p1 < 28;
    int cupY = sip ? 104 : 112;
    MascotAt m = M(c, sip ? SPR_EAT_B : SPR_EAT_A, X0, Y0);
    art(c, CUP, 3, 152, cupY, 4, "w6k6");
    R(c, 152, cupY, 8, 2, S_7);
    for (int i = 0; i < 3; i++) {
        uint32_t q = (p + i * 5) % 15;
        if (q < 10) R(c, 153 + ((q >> 2) % 2) * 3, cupY - 4 - q * 2, 2, 3, S_8);
    }
    return m;
}

MascotAt book(Canvas &c, uint32_t p, uint32_t) {  // 7 cover 8 spine 9 lines a page
    uint32_t pb = p % 36;
    MascotAt m = M(c, ((p >> 3) % 2) ? SPR_LOOK_L : SPR_WALK_A, X0, Y0);
    R(c, 92, 111, 56, 26, S_7, 3);
    R(c, 95, 113, 24, 22, S_INK, 1);
    R(c, 121, 113, 24, 22, S_INK, 1);
    R(c, 119, 112, 2, 24, S_8);
    for (int x0 : {96, 122})
        for (int i = 0; i < 4; i++) R(c, x0 + 2, 118 + i * 4, i == 3 ? 10 : 17, 2, S_9);
    if (pb < 8) {
        int fw = int(lroundf(22 * fabsf(cosf(PI_F * pb / 8))));
        if (pb < 4) R(c, 121, 113, fw, 22, S_A, 1);
        else R(c, 119 - fw, 113, fw, 22, S_A, 1);
    }
    R(c, 88, 118, 6, 8, S_BODY, 2);
    R(c, 146, 118, 6, 8, S_BODY, 2);
    return m;
}

MascotAt laptopScene(Canvas &c, uint32_t p, uint32_t) {  // 7 green 8 amber 9 blue a grey b dark
    for (int i = 0; i < 3; i++) {
        uint32_t q = (p + i * 4) % 12;
        if (q < 8) R(c, 100 + i * 16, 108 - q * 2, 6, 2, uint8_t(S_7 + i));
    }
    MascotAt m = M(c, (p % 2) ? SPR_ARM_A : SPR_ARM_L, X0, Y0);
    laptop(c, S_A, S_B);
    return m;
}

MascotAt meditation(Canvas &c, uint32_t p, uint32_t) {  // 7 shadow
    float s = sinf(p / 5.0f);
    R(c, 100 + 2 * s, 141, int(40 - 4 * s), 3, S_7, 2);
    return M(c, SPR_DROWSY, X0, Y0 - 6 + int(lroundf(3 * s)));
}

MascotAt bubbles(Canvas &c, uint32_t p, uint32_t) {  // 7 amber 8 bubble
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    R(c, 156, 92, 2, 8, S_7);
    for (int i = 0; i < 5; i++) {
        uint32_t q = (p + i * 9) % 45;
        int s = 6 + (i % 3) * 2;
        if (q < 38) R(c, 166 + i * 7 + 5 * sinf((p + i * 7) / 5.0f), 132 - q * 1.2f, s, s, S_8, s);
    }
    return m;
}

MascotAt plane(Canvas &c, uint32_t p, uint32_t) {
    uint32_t q = p % 60;
    float x = q < 10 ? 152 : q < 34 ? 152 + (q - 10) * 3.0f : 20 + (q - 34) * 4.5f;
    float y = q < 10 ? 92 : 92 - (q < 34 ? float(q - 10) : 6.0f) + 4 * sinf(q / 3.0f);
    MascotAt m = M(c, q < 10 ? SPR_ARM_A : (q < 34 ? SPR_WALK_A : SPR_LOOK_L), X0, Y0);
    if (!(x > 205 && x < 230)) art(c, PLANE, 5, x, y, 2, "w6");
    return m;
}

MascotAt rope(Canvas &c, uint32_t p, uint32_t) {  // 7 amber
    float ph = p * 0.5f, mm = 26 * cosf(ph);
    MascotAt m = M(c, SPR_HAPPY, X0, Y0 + (cosf(ph) > 0.6f ? -8 : 0));
    for (int k = 0; k <= 12; k++) {
        float u = k / 12.0f;
        R(c, 86 + 66 * u, 112 + 4 * mm * u * (1 - u), 3, 3, S_7);
    }
    return m;
}

MascotAt juggle(Canvas &c, uint32_t p, uint32_t) {  // 7 amber 8 green 9 blue
    MascotAt m = M(c, alt2(p, SPR_ARM_A, SPR_ARM_L), X0, Y0);
    for (int i = 0; i < 3; i++) {
        float a = p * 0.35f + i * 2.094f;
        R(c, 116 + 34 * cosf(a), 92 - 10 * fabsf(sinf(a)), 8, 8, uint8_t(S_7 + i), 4);
    }
    return m;
}

MascotAt watering(Canvas &c, uint32_t p, uint32_t) {  // 7 pot 8 green 9 pink a blue
    uint32_t q = p % 60;
    float stem = fminf(20, 4 + q / 2.5f);
    R(c, 168, 128, 14, 9, S_7, 2);
    R(c, 174, 128 - stem, 2, int(stem), S_8);
    if (stem >= 18) art(c, FLOWER, 3, 170, 128 - stem - 8, 3, "p9");
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    R(c, 150, 100, 11, 9, S_A, 2);
    R(c, 160, 101, 7, 2, S_A);
    for (int i = 0; i < 3; i++) R(c, 166 + i * 2, 106 + ((p + i * 3) % 9) * 2.5f, 2, 3, S_A);
    return m;
}

MascotAt cat(Canvas &c, uint32_t p, uint32_t) {  // 7 grey 8 pink
    uint32_t q = p % 60;
    float x = q < 20 ? 212 - q * 2.2f : 168;
    art(c, CAT, 7, x, 118, 3, (p % 6) < 3 ? "k7e2t7" : "k7e2");
    if (q >= 25 && q < 40) art(c, HEART, 5, 150, 84, 2, "x8");
    return M(c, q < 20 ? SPR_WALK_A : (q < 40 ? SPR_HAPPY : SPR_ARM_B), X0, Y0);
}

MascotAt balloon(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 string
    uint32_t q = p % 60;
    float bx = 164 + 3 * sinf(p / 4.0f), by = q < 40 ? 82 : 82 - (q - 40) * 1.2f;
    MascotAt m = M(c, q < 40 ? SPR_ARM_A : SPR_UP, X0, Y0);
    R(c, bx, by, 14, 16, S_7, 8);
    float ex = q < 40 ? 153 : bx + 7, ey = q < 40 ? 100 : by + 30;
    for (int k = 0; k < 6; k++) {
        float u = k / 5.0f;
        R(c, bx + 7 + (ex - bx - 7) * u, by + 16 + (ey - by - 16) * u, 1, 3, S_8);
    }
    return m;
}

MascotAt shades(Canvas &c, uint32_t p, uint32_t t) {
    int dy = ((p >> 2) % 2) ? 2 : 0;
    MascotAt m = M(c, SPR_IDLE, X0, Y0 + dy, true);
    (void)t;
    R(c, 98, 99 + dy, 20, 11, S_EYE, 2);
    R(c, 123, 99 + dy, 20, 11, S_EYE, 2);
    R(c, 117, 101 + dy, 7, 2, S_EYE);
    if ((p % 20) < 9) R(c, 100 + (p % 20) * 4, 100 + dy, 3, 2, S_INK);
    return m;
}

MascotAt guitar(Canvas &c, uint32_t p, uint32_t) {  // 7 body 8 hole 9 neck a fret b head
    for (int i = 0; i < 2; i++) {
        uint32_t q = (p + i * 8) % 16;
        if (q < 12) art(c, NOTE, 3, 160 + i * 14 + q, 118 - q * 2.2f, 2, "x6");
    }
    MascotAt m = M(c, alt2(p, SPR_ARM_A, SPR_ARM_B), X0, Y0 + (((p >> 2) % 2) ? 1 : 0));
    art(c, GUITAR, 20, 80, 108, 3, "b7o8n9fahb");
    bool strum = (p % 4) < 2;
    R(c, 98, strum ? 117 : 118, 46, 1, S_INK);
    R(c, 98, strum ? 120 : 119, 46, 1, S_INK);
    return m;
}

MascotAt butterfly(Canvas &c, uint32_t p, uint32_t) {  // 7 amber
    float x = 120 + 70 * sinf(p / 9.0f), y = 88 + 6 * sinf(p / 4.0f);
    int w = (p % 2) ? 6 : 2;
    MascotAt m = M(c, x < 108 ? SPR_LOOK_L : x > 132 ? SPR_WALK_A : SPR_UP, X0, Y0);
    R(c, x - w, y, w, 6, S_7, 2);
    R(c, x + 2, y, w, 6, S_7, 2);
    R(c, x, y - 1, 2, 8, S_EYE);
    return m;
}

MascotAt idea(Canvas &c, uint32_t p, uint32_t) {  // 7 amber 8 grey 9 light
    uint32_t q = p % 45;
    MascotAt m = M(c, q < 15 ? SPR_UP : (q < 30 ? SPR_HAPPY : SPR_IDLE), X0, Y0 + (q >= 15 && q < 23 ? hop(q - 15, 1, 8, 8) : 0));
    if (q < 15) {
        for (int i = 0; i < 3; i++)
            if (q > uint32_t(i * 4)) R(c, 150 + i * 7, 92 - i * 5, 4, 4, S_8, 2);
    } else {
        bool on = (q % 4) < 2;
        art(c, BULB, 5, 160, 82, 3, on ? "y7g8" : "y9g8");
        if (on) {
            const int D[4][2] = {{-6, 4}, {18, 4}, {-4, 14}, {16, 14}};
            for (auto &d : D) R(c, 160 + d[0], 82 + d[1], 3, 3, S_7);
        }
    }
    return m;
}

MascotAt ball(Canvas &c, uint32_t p, uint32_t) {
    uint32_t q = p % 50;
    float x = q < 15 ? 30 + q * 4.5f : q < 18 ? 98 : 98 + (q - 18) * 4.0f;
    float y = q < 18 ? 126 : 126 + hop(q - 18, 1, 26, 40);
    bool kick = q >= 14 && q < 18;
    if (x < 208) {
        R(c, x, y, 10, 10, S_INK, 5);
        R(c, x + 3, y + 3, 4, 4, S_EYE, 2);
    }
    return M(c, kick ? SPR_WALK_B : SPR_WALK_A, X0 + (kick ? -4 : 0), Y0);
}

MascotAt hide(Canvas &c, uint32_t p, uint32_t) {  // 7 floor
    uint32_t q = p % 50;
    float y = q < 10 ? Y0 + q * 5.5f : q < 25 ? Y0 + 55 : q < 30 ? Y0 + 55 - (q - 25) * 4.0f : q < 40 ? Y0 + 35 : Y0 + 35 - (q - 40) * 3.5f;
    R(c, 30, 137, 180, 2, S_7);
    bool peek = q >= 25 && q < 40;
    return M(c, peek ? (((p >> 2) % 2) ? SPR_LOOK_L : SPR_WALK_A) : SPR_IDLE, X0, int(lroundf(y)), y < Y0 + 10);
}

MascotAt yoyo(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 string
    float l = 34 * fabsf(sinf(p / 4.0f));
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    R(c, 153, 100, 1, int(l), S_8);
    R(c, 149, 100 + l, 9, 9, S_7, 5);
    return m;
}

// ---- holidays (board "Pack" 19-26) ----
MascotAt valentine(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 pink
    for (int i = 0; i < 4; i++) {
        uint32_t q = (p + i * 11) % 44;
        if (q < 36) art(c, HEART, 5, 36 + i * 46 + 4 * sinf((p + i * 5) / 4.0f), 140 - q * 1.3f, 2, i % 2 ? "x7" : "x8");
    }
    MascotAt m = M(c, SPR_EAT_A, X0, Y0 + (((p >> 2) % 2) ? -2 : 0));
    art(c, HEART, 5, 148, 108, 3, "x7");
    return m;
}

MascotAt march8(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 pink 9 amber a green
    MascotAt m = M(c, SPR_EAT_A, X0 + int(lroundf(2 * sinf(p / 5.0f))), Y0);
    const int X[3] = {146, 154, 162};
    const char *MAP[3] = {"x8", "x9", "x7"};
    for (int i = 0; i < 3; i++) {
        R(c, X[i] + 3, 96 + i * 2, 2, 22 - i * 2, S_A);
        art(c, TULIP, 3, X[i], 88 + i * 2, 3, MAP[i]);
    }
    return m;
}

MascotAt easter(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 yellow 9 black a wood b dark wood c willow d buds
    uint32_t q = p % 64;
    int stage = q / 12 > 4 ? 4 : int(q / 12);
    R(c, 36, 82, 2, 50, S_C);
    const int B[6][2] = {{30, 86}, {40, 92}, {31, 100}, {40, 108}, {32, 116}, {41, 122}};
    for (auto &b : B) R(c, b[0], b[1], 5, 6, S_D, 3);
    R(c, 161, 125, 25, 4, S_A, 2);
    R(c, 166, 129, 15, 3, S_B, 1);
    for (int i = 0; i < 99; i++) {
        char e = char(pgm_read_byte(EGG + i));
        if (e == '.') continue;
        int r = i / 9, band = r <= 2 ? 1 : r <= 7 ? 2 : 3;
        char pc = char(pgm_read_byte(EGG_PAT + i));
        uint8_t s = stage < band && stage != 4 ? S_INK : pc == 'y' ? S_8 : pc == 'k' ? S_9 : S_7;
        R(c, 160 + (i % 9) * 3, 92 + r * 3, 3, 3, s);
    }
    MascotAt m = M(c, stage < 4 ? alt2(p, SPR_ARM_A, SPR_ARM_B) : SPR_HAPPY, X0, Y0 + (stage == 4 ? hop(q - 48, 1, 8, 8) : 0));
    if (stage < 4) {
        int band = stage + 1 > 3 ? 3 : stage + 1, by = band == 1 ? 96 : band == 2 ? 108 : 120;
        R(c, 150 + (q % 4) * 2, by - 10, 2, 12, S_A);
        R(c, 150 + (q % 4) * 2, by, 3, 3, band == 2 ? S_8 : S_7);
    }
    return m;
}

MascotAt programmers(Canvas &c, uint32_t p, uint32_t) {  // 7 green a grey b dark
    for (int i = 0; i < 10; i++) {
        uint32_t q = (p * (1 + i % 3) + i * 23) % 120;
        R(c, 34 + i * 18, 80 + q % 64, 3, ((i + p / 4) % 2) ? 8 : 4, S_7);
    }
    MascotAt m = M(c, (p % 2) ? SPR_ARM_A : SPR_ARM_L, X0, Y0);
    laptop(c, S_A, S_B);
    return m;
}

MascotAt independence(Canvas &c, uint32_t p, uint32_t) {  // 7 blue 8 grey 9 yellow
    MascotAt m = M(c, SPR_ARM_A, X0, Y0);
    R(c, 156, 84, 2, 52, S_8);
    for (int k = 0; k < 9; k++) {
        float dy = 2 * sinf(p / 2.0f + k * 0.7f);
        R(c, 158 + k * 4, 86 + dy, 4, 9, S_7);
        R(c, 158 + k * 4, 95 + dy, 4, 9, S_9);
    }
    return m;
}

MascotAt defenders(Canvas &c, uint32_t p, uint32_t t) {  // 7 blue 8 yellow
    int beat = (p % 8) < 2 ? 5 : 4;
    art(c, UA_HEART, 7, 166 - (beat - 4) * 7, 100 - (beat - 4) * 7, beat, "b7y8");
    return M(c, ((t + 3) % 21) < 2 ? SPR_BLINK : SPR_IDLE, X0, Y0);
}

MascotAt halloween(Canvas &c, uint32_t p, uint32_t t) {  // 7 orange 8 green 9 glow a ghost b dim glow
    art(c, PUMPKIN, 7, 164, 112, 4, (p % 7) < 5 ? "o7g8e9" : "o7g8eb");
    return M(c, ((t + 5) % 21) < 2 ? SPR_BLINK : SPR_IDLE, X0, Y0 - 4 + int(lroundf(3 * sinf(p / 4.0f))), true, S_A);
}

MascotAt birthday(Canvas &c, uint32_t p, uint32_t) {  // 7..b confetti, 8 pink, c flame d light flame
    for (int i = 0; i < 12; i++) {
        uint32_t q = (p * 2 + i * 17) % 150;
        R(c, 32 + (i * 37) % 176 + 3 * sinf((p + i) / 3.0f), 80 + q % 70, 3, 3, uint8_t(S_7 + i % 5));
    }
    MascotAt m = M(c, SPR_HAPPY, X0, Y0 + hop(p % 16, 1, 8, 8));
    art(c, CAKE, 8, 160, 104, 4, (p % 3) ? "fcc6w6p8" : "fdc6w6p8");
    return m;
}

bool on(const SceneCtx &x, int m, int d) { return x.synced && x.month == m && x.day == d; }
bool isValentine(const SceneCtx &x) { return on(x, 2, 14); }
bool isMarch8(const SceneCtx &x) { return on(x, 3, 8); }
bool isEaster(const SceneCtx &x) {
    if (!x.synced) return false;
    int m, d;
    orthodoxEaster(x.year, m, d);
    return x.month == m && x.day == d;
}
bool isProgrammers(const SceneCtx &x) { return x.synced && dayOfYear(x.year, x.month, x.day) == 256; }
bool isIndependence(const SceneCtx &x) { return on(x, 8, 24); }
bool isDefenders(const SceneCtx &x) { return on(x, 10, 1); }
bool isHalloween(const SceneCtx &x) { return on(x, 10, 31); }
bool isBirthday(const SceneCtx &x) { return x.birthMonth > 0 && on(x, x.birthMonth, x.birthDay); }

const uint16_t K_BROWN = C565(0x7A4A2A), K_GREY = C565(0xA8A399), K_AMB = C565(0xF0B429), K_GRN = C565(0x5FB97A),
               K_BLU = C565(0x6AA6F8), K_RED = C565(0xE5484D), K_PINK = C565(0xF08CB0), K_DARK = C565(0x3A3732);
}  // namespace

const Scene SCENES_A[] PROGMEM = {
    {"coffee", SceneKind::Daily, true, 80, {K_BROWN, K_GREY}, coffee, nullptr, nullptr, nullptr},
    {"book", SceneKind::Daily, true, 72, {C565(0x2E5AA8), C565(0x1E3E78), C565(0x9A958C), C565(0xE2DCCD)}, book, nullptr, nullptr, nullptr},
    {"laptop", SceneKind::Daily, false, 60, {K_GRN, K_AMB, K_BLU, C565(0x9A958C), C565(0x6E6A62)}, laptopScene, nullptr, nullptr, nullptr},
    {"meditation", SceneKind::Daily, true, 60, {C565(0x2A2825)}, meditation, nullptr, nullptr, nullptr},
    {"bubbles", SceneKind::Daily, true, 70, {K_AMB, C565(0x7FA8D8)}, bubbles, nullptr, nullptr, nullptr},
    {"plane", SceneKind::Daily, false, 60, {}, plane, nullptr, nullptr, nullptr},
    {"rope", SceneKind::Daily, false, 60, {K_AMB}, rope, nullptr, nullptr, nullptr},
    {"juggle", SceneKind::Daily, false, 60, {K_AMB, K_GRN, K_BLU}, juggle, nullptr, nullptr, nullptr},
    {"watering", SceneKind::Daily, true, 60, {C565(0xB0603A), K_GRN, K_PINK, K_BLU}, watering, nullptr, nullptr, nullptr},
    {"cat", SceneKind::Daily, true, 60, {C565(0x8A8478), K_PINK}, cat, nullptr, nullptr, nullptr},
    {"balloon", SceneKind::Daily, true, 60, {K_RED, K_GREY}, balloon, nullptr, nullptr, nullptr},
    {"shades", SceneKind::Daily, true, 40, {}, shades, nullptr, nullptr, nullptr},
    {"guitar", SceneKind::Daily, true, 48, {C565(0xC8913E), C565(0x1C1A17), C565(0x5E3A1A), C565(0xD8D0C0), C565(0x3A2A1A)}, guitar, nullptr, nullptr, nullptr},
    {"butterfly", SceneKind::Daily, true, 72, {K_AMB}, butterfly, nullptr, nullptr, nullptr},
    {"idea", SceneKind::Daily, false, 45, {K_AMB, K_GREY, C565(0xFFD86B)}, idea, nullptr, nullptr, nullptr},
    {"ball", SceneKind::Daily, false, 50, {}, ball, nullptr, nullptr, nullptr},
    {"hide", SceneKind::Daily, false, 50, {K_DARK}, hide, nullptr, nullptr, nullptr},
    {"yoyo", SceneKind::Daily, false, 48, {K_RED, K_GREY}, yoyo, nullptr, nullptr, nullptr},
    {"valentine", SceneKind::Holiday, true, 60, {K_RED, K_PINK}, valentine, isValentine, "День закоханих", "14.02"},
    {"march8", SceneKind::Holiday, true, 48, {K_RED, K_PINK, K_AMB, K_GRN}, march8, isMarch8, "8 Березня", "08.03"},
    {"easter", SceneKind::Holiday, true, 64, {C565(0xC8102E), C565(0xF5C518), C565(0x1C1A17), C565(0xB0603A), C565(0x8A4A1A), C565(0x7A5A3A), C565(0xD8D4CC)}, easter, isEaster, "Великдень", ""},
    {"programmers", SceneKind::Holiday, false, 60, {K_GRN, 0, 0, C565(0x9A958C), C565(0x6E6A62)}, programmers, isProgrammers, "День програміста", "13.09"},
    {"independence", SceneKind::Holiday, true, 48, {C565(0x2E6BD8), K_GREY, C565(0xF5C518)}, independence, isIndependence, "День Незалежності", ""},
    {"defenders", SceneKind::Holiday, true, 48, {C565(0x2E6BD8), C565(0xF5C518)}, defenders, isDefenders, "День захисників", "01.10"},
    {"halloween", SceneKind::Holiday, true, 60, {C565(0xE8772E), K_GRN, C565(0xFFD86B), C565(0xE8E4DA), C565(0x7A3A10)}, halloween, isHalloween, "Гелловін", "31.10"},
    {"birthday", SceneKind::Holiday, true, 48, {K_RED, K_PINK, K_AMB, K_GRN, K_BLU, K_AMB, C565(0xFFD86B)}, birthday, isBirthday, "З днем народження!", ""},
};
const int SCENES_A_N = sizeof(SCENES_A) / sizeof(SCENES_A[0]);
