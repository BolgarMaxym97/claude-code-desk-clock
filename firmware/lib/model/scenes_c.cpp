// Scene pack, board "Pack3" (design/idle-anim/src/pack3.js), clock-screen part: rare surprises, weekend,
// your rhythm, more holidays, stage-2 events. Work-screen reactions from the same board live in scenes_w.cpp.
#include <math.h>
#include "scene.h"
#include "scenes_internal.h"

namespace {
const int X0 = 85, Y0 = 96;

const char HEART[] PROGMEM = ".x.x." "xxxxx" "xxxxx" ".xxx." "..x..";
const char CHECK[] PROGMEM = ".....x" "....xx" "x..xx." "xxxx.." ".xx...";
const char PLUS[] PROGMEM = "...x..." "...x..." "...x..." "xxxxxxx" "...x..." "...x..." "...x...";
const char CROSS[] PROGMEM = "x.....x" ".x...x." "..x.x.." "...x..." "..x.x.." ".x...x." "x.....x";
const char DRAGON_UP[] PROGMEM = ".ww....." "gggggg.." ".gggggge" ".g...g..";
const char DRAGON_DN[] PROGMEM = "........" "gggggg.." ".gggggge" ".g...g..";
const char HELMET[] PROGMEM = "....gggggg...." "..gggggggggg.." ".gggggggggggg." "gggggggggggggg";
const char BOLT[] PROGMEM = "..x" ".x." "xxx" ".x." "x..";
const char UMBRELLA[] PROGMEM = "...rrrrr..." ".rrrrrrrrr." "rrrrrrrrrrr" ".....h....." ".....h....." "....hh.....";
const char CAT[] PROGMEM = "k...k.." "kk.kk.." "kekkek." "kkkkkkk" "kkkkkkt" "k.k.k.t";
const char KITTEN[] PROGMEM = "k...k" "kkkkk" "kekek" "kkkkk";

MascotAt M(Canvas &c, const SpriteFrame &f, int x, int y, bool hatOk = true, uint8_t body = S_BODY) {
    frame(c, f, x, y, 5, body);
    return MascotAt{x, y, hatOk};
}
bool alt(uint32_t p) { return (p >> 1) % 2; }
void stars(Canvas &c, uint32_t p, int n) {
    for (int i = 0; i < n; i++)
        if (((p + i * 3) % 10) < 6) R(c, 34 + (i * 47) % 172, 82 + (i * 13) % 26, 2, 2, S_INK);
}
void ring(Canvas &c, int cx, int cy, int r, uint8_t s) {
    R(c, cx - r, cy - r, 2 * r, 2, s);
    R(c, cx - r, cy + r - 2, 2 * r, 2, s);
    R(c, cx - r, cy - r, 2, 2 * r, s);
    R(c, cx + r - 2, cy - r, 2, 2 * r, s);
}

// ---- rare surprises ----
MascotAt ufo(Canvas &c, uint32_t p, uint32_t) {  // 7 hull 8 dome 9 lights a beam
    int lift = p < 15 ? 0 : p < 30 ? int(p - 15) * 3 : p < 40 ? 99 : p < 55 ? 45 - int(p - 40) * 3 : 0;
    if (p >= 10 && p < 55) R(c, 106, 90, 28, 50, S_A);
    MascotAt m{X0, Y0, false};
    if (lift != 99) m = M(c, lift > 0 ? SPR_UP : SPR_IDLE, X0, Y0 - lift, lift == 0);
    if (p >= 5 && p < 60) {
        R(c, 98, 82, 44, 8, S_7, 4);
        R(c, 110, 78, 20, 6, S_8, 4);
        for (int i = 0; i < 3; i++) R(c, 104 + i * 14, 84, 3, 3, S_9);
    }
    return m;
}

MascotAt asterisk(Canvas &c, uint32_t p, uint32_t) {
    if (p >= 10 && p < 30) {
        art(c, (p % 2) ? CROSS : PLUS, 7, 102, 98, 5, "x1");
        return MascotAt{X0, Y0, false};
    }
    return M(c, p < 10 ? SPR_UP : SPR_HAPPY, X0, Y0);
}

MascotAt guest(Canvas &c, uint32_t p, uint32_t) {  // 7 guest body 8 amber
    float gx = p < 22 ? 214 - p * 2.4f : 161;
    frame(c, p >= 22 && p < 34 ? SPR_ARM_L : (p < 22 ? SPR_WALK_LB : SPR_HAPPY), int(lroundf(gx)), 106, 4, S_7);
    if (p >= 22 && p < 30) {
        R(c, 157, 98, 3, 3, S_8);
        R(c, 161, 94, 3, 3, S_8);
        R(c, 153, 94, 3, 3, S_8);
    }
    return M(c, p >= 22 && p < 34 ? SPR_ARM_A : SPR_WALK_A, X0, Y0);
}

MascotAt dragon(Canvas &c, uint32_t p, uint32_t) {  // 7 green 8 wing 9 eye a amber b orange c red
    float x = 214 - p * 3.4f;
    MascotAt m = M(c, x > 120 ? SPR_WALK_A : SPR_LOOK_L, X0, Y0);
    if (x > 20) {
        art(c, (p % 2) ? DRAGON_UP : DRAGON_DN, 8, x, 86 + 3 * sinf(p / 3.0f), 3, "g7w8e9");
        if (p > 25 && p < 32)
            for (int k = 0; k < 3; k++) R(c, x - 6 - k * 4, 92 + k * 2, 3, 3, uint8_t(S_A + k));
    }
    return m;
}

MascotAt knock(Canvas &c, uint32_t p, uint32_t) {
    bool close = p >= 10 && p < 38;
    MascotAt m{X0, Y0, true};
    if (close) {
        frame(c, SPR_ARM_B, X0 - 7, Y0 - 6, 6);
        m = MascotAt{X0 - 7, Y0 - 6, false};
        if ((p % 8) < 4) ring(c, 120, 112, 8 + int(p % 8) * 4, S_INK);
    } else {
        m = M(c, SPR_IDLE, X0, Y0);
    }
    return m;
}

MascotAt glitch(Canvas &c, uint32_t p, uint32_t) {  // 7 red ghost 8 blue ghost 9..b noise
    bool g = p >= 10 && p < 26;
    int j = g ? ((p % 2) ? 4 : -4) : 0;
    if (g) {
        frame(c, SPR_IDLE, X0 - 4 + j, Y0, 5, S_7);
        frame(c, SPR_IDLE, X0 + 4 - j, Y0, 5, S_8);
    }
    MascotAt m = M(c, SPR_IDLE, X0 + j, Y0, !g);
    if (g)
        for (int i = 0; i < 8; i++) R(c, 60 + (i * 41 + p * 7) % 120, 90 + (i * 17 + p * 5) % 50, 4, 2, uint8_t(S_9 + i % 3));
    return m;
}

MascotAt shootingStar(Canvas &c, uint32_t p, uint32_t) {  // 7 pink
    stars(c, p, 8);
    if (p < 14)
        for (int k = 0; k < 6; k++) R(c, 200 - p * 10 + k * 4, 84 + p * 1.5f - k, 3, 2, S_INK);
    if (p >= 26 && p < 40) art(c, HEART, 5, 160, 88, 2, "x7");
    return M(c, p < 14 ? SPR_UP : p < 26 ? SPR_DROWSY : SPR_HAPPY, X0, Y0);
}

// ---- weekend ----
MascotAt hammock(Canvas &c, uint32_t p, uint32_t) {  // 7 post 8 net a net2
    int sw = int(lroundf(3 * sinf(p / 5.0f)));
    R(c, 36, 104, 3, 40, S_7);
    R(c, 200, 104, 3, 40, S_7);
    MascotAt m = M(c, SPR_SLEEP, X0, Y0 + 8 + sw, false);
    for (int k = 0; k < 14; k++) {
        float u = k / 13.0f;
        R(c, 38 + 162 * u, 110 + 26 * sinf(3.14159f * u) + sw, 12, 3, k % 2 ? S_8 : S_9);
    }
    return m;
}

MascotAt picnic(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 basket 9 dark basket a bread b green
    for (int i = 0; i < 16; i++) R(c, 60 + (i % 8) * 14, 136 + (i / 8) * 6, 14, 6, (i + i / 8) % 2 ? S_7 : S_INK);
    R(c, 170, 120, 22, 14, S_8, 3);
    R(c, 174, 114, 14, 4, S_9, 2);
    R(c, 176, 110, 4, 6, S_7, 2);
    MascotAt m = M(c, (p % 20) < 10 ? SPR_EAT_A : SPR_EAT_B, X0, Y0);
    R(c, 152, 112, 12, 3, S_A);
    R(c, 152, 115, 12, 2, S_B);
    R(c, 152, 117, 12, 3, S_A);
    return m;
}

MascotAt sunbath(Canvas &c, uint32_t, uint32_t) {  // 7 blue 8 leg 9 sun
    for (int i = 0; i < 9; i++) R(c, 70 + i * 12, 136 - (i > 6 ? (i - 6) * 4 : 0), 12, 5, i % 2 ? S_7 : S_INK);
    R(c, 80, 141, 2, 8, S_8);
    R(c, 170, 141, 2, 8, S_8);
    R(c, 186, 82, 16, 16, S_9, 8);
    MascotAt m = M(c, SPR_SLEEP, X0, Y0 + 2, false);
    R(c, 110, 121, 20, 5, S_EYE, 2);
    return m;
}

MascotAt chess(Canvas &c, uint32_t p, uint32_t) {  // 7 dark 8 light
    int step = int((p % 48) / 12);
    MascotAt m = M(c, (p % 12) < 6 ? SPR_UP : SPR_ARM_A, X0, Y0);
    for (int i = 0; i < 16; i++) R(c, 160 + (i % 4) * 8, 112 + (i / 4) * 8, 8, 8, (i + i / 4) % 2 ? S_7 : S_8);
    R(c, 162 + (step % 4) * 8, 114 + (step > 1 ? 8 : 0), 4, 5, S_INK, 2);
    R(c, 180, 130, 4, 5, S_EYE, 2);
    return m;
}

// ---- your rhythm ----
MascotAt bored(Canvas &c, uint32_t p, uint32_t) {  // 7 sigh
    uint32_t q = p % 30;
    if (q > 20) R(c, 154 + (q - 20), 104 - (q - 20), 8, 5, S_7, 3);
    return M(c, ((p >> 4) % 2) ? SPR_LOOK_L : SPR_WALK_A, X0, Y0 + 4);
}

MascotAt goodnight(Canvas &c, uint32_t p, uint32_t) {  // 7 lamp 8 light
    bool on = p < 36;
    R(c, 176, 134, 18, 4, S_7, 1);
    R(c, 183, 104, 3, 30, S_7);
    R(c, 172, 98, 22, 8, S_7, 3);
    if (on) R(c, 170, 106, 26, 28, S_8, 4);
    return on ? M(c, alt(p) ? SPR_ARM_A : SPR_ARM_B, X0, Y0) : M(c, SPR_SLEEP, X0, Y0 + 8, false);
}

MascotAt closed(Canvas &c, uint32_t p, uint32_t) {  // 7 pole 8 board
    MascotAt m = M(c, alt(p) ? SPR_ARM_A : SPR_ARM_B, X0, Y0);
    R(c, 164, 84, 2, 10, S_7);
    R(c, 184, 84, 2, 10, S_7);
    R(c, 156, 94, 38, 22, S_8, 2);
    R(c, 160, 98, 30, 3, S_INK);
    R(c, 160, 104, 22, 3, S_INK);
    R(c, 160, 110, 26, 3, S_INK);
    return m;
}

// ---- holidays ----
MascotAt kyiv(Canvas &c, uint32_t p, uint32_t t) {  // 7 trunk 8 crown
    R(c, 178, 110, 4, 32, S_7);
    R(c, 160, 84, 40, 30, S_8, 12);
    const int F[5][2] = {{166, 88}, {182, 86}, {190, 96}, {170, 100}, {184, 104}};
    for (auto &f : F) R(c, f[0], f[1] + ((p % 8) < 4 ? 0 : 1), 3, 7, S_INK);
    return M(c, ((t + 2) % 21) < 2 ? SPR_BLINK : SPR_IDLE, X0, Y0);
}

MascotAt unity(Canvas &c, uint32_t p, uint32_t) {  // 7 friend 8 pole 9 blue a yellow
    MascotAt m = M(c, SPR_IDLE, X0 - 22, Y0);
    frame(c, ((p >> 2) % 2) ? SPR_HAPPY : SPR_IDLE, 156, 106, 4, S_7);
    R(c, 128, 118, 30, 4, S_BODY);
    R(c, 186, 90, 2, 30, S_8);
    R(c, 188, 90, 12, 5, S_9);
    R(c, 188, 95, 12, 5, S_A);
    return m;
}

MascotAt flagDay(Canvas &c, uint32_t p, uint32_t) {  // 7 pole 8 blue 9 yellow
    float rise = fminf(40, (p % 60) * 1.2f);
    R(c, 166, 82, 2, 60, S_7);
    for (int k = 0; k < 6; k++) {
        float dy = 1.5f * sinf(p / 2.0f + k * 0.7f);
        R(c, 168 + k * 4, 128 - rise + dy, 4, 7, S_8);
        R(c, 168 + k * 4, 135 - rise + dy, 4, 7, S_9);
    }
    return M(c, SPR_ARM_A, X0 - 10, Y0);
}

MascotAt catDay(Canvas &c, uint32_t p, uint32_t) {  // 7 grey 8 ginger 9 pink
    MascotAt m = M(c, SPR_HAPPY, X0, Y0, false);
    art(c, CAT, 7, 166, 118, 3, (p % 6) < 3 ? "k7e2t7" : "k7e2");
    art(c, KITTEN, 5, 100, 84 + ((p >> 2) % 2), 2, "k8e2");
    art(c, HEART, 5, 150, 88, 2, "x9");
    return m;
}

MascotAt clockBirthday(Canvas &c, uint32_t p, uint32_t) {  // 7 pink 8 frosting 9 flame a smoke
    uint32_t q = p % 48;
    bool lit = q < 24;
    R(c, 162, 122, 20, 12, S_7, 2);
    R(c, 160, 116, 24, 8, S_8, 4);
    R(c, 171, 106, 3, 10, S_INK);
    if (lit) R(c, 170, 100 - (p % 2), 5, 6, S_9, 3);
    else R(c, 170, 96 - float((q - 24) % 12), 5, 5, S_A, 3);
    return M(c, lit ? SPR_HAPPY : SPR_EAT_B, X0, Y0);
}

// ---- stage-2 events ----
MascotAt allClear(Canvas &c, uint32_t p, uint32_t) {  // 7 khaki 8 green
    float hy = p < 16 ? 87 : 87 - (p - 16) * 3.0f;
    MascotAt m = M(c, p < 16 ? SPR_SQUISH : SPR_HAPPY, X0, Y0 + (p < 16 ? 4 : 0), false);
    art(c, HELMET, 14, 85, hy, 5, "g7");
    if (p > 30) art(c, CHECK, 6, 170, 90, 3, "x8");
    return m;
}

MascotAt storm(Canvas &c, uint32_t p, uint32_t) {  // 7 flash 8 rain 9 amber a blue b grey
    bool flash = (p % 30) < 2 || (p % 30) == 4;
    if (flash) R(c, 28, 80, 184, 70, S_7);
    for (int i = 0; i < 16; i++) R(c, 32 + (i * 23) % 176, 80 + (p * 4 + i * 29) % 70, 1, 6, S_8);
    if (flash) art(c, BOLT, 3, 186, 84, 4, "x9");
    MascotAt m = M(c, flash ? SPR_SHOUT : SPR_ARM_A, X0, Y0, false);
    art(c, UMBRELLA, 11, 98, 82, 4, "rahb");
    return m;
}

MascotAt fog(Canvas &c, uint32_t p, uint32_t) {  // 7 fog
    MascotAt m = M(c, alt(p) ? SPR_ARM_A : SPR_ARM_B, X0, Y0);
    for (int i = 0; i < 3; i++) R(c, 28 + float((p * (1 + i) + i * 60) % 260) - 80, 94 + i * 16, 90, 10, S_7, 5);
    return m;
}

MascotAt meetingNow(Canvas &c, uint32_t p, uint32_t) {  // 7 laptop
    uint32_t q = p % 50;
    float x = q < 25 ? X0 + q * 5.0f : X0 - 120 + (q - 25) * 4.8f;
    MascotAt m = M(c, alt(p) ? SPR_WALK_B : SPR_WALK_A, int(lroundf(x)), Y0, false);
    R(c, x + 62, 114, 16, 11, S_7, 2);
    return m;
}

bool on(const SceneCtx &x, int m, int d) { return x.synced && x.month == m && x.day == d; }
bool hKyiv(const SceneCtx &x) {
    if (!x.synced || x.month != 5) return false;
    int d = nthWeekdayOfMonth(x.year, 5, 0, 5);
    if (d > 31) d -= 7;  // last Sunday of May
    return x.day == d;
}
bool hUnity(const SceneCtx &x) { return on(x, 1, 22); }
bool hFlag(const SceneCtx &x) { return on(x, 8, 23); }
bool hCat(const SceneCtx &x) { return on(x, 8, 8); }
bool hClock(const SceneCtx &x) { return on(x, 11, 25) && x.year > 2026; }  // first flashed 25.11.2026
bool eBored(const SceneCtx &x) { return x.bored; }
bool eGoodnight(const SceneCtx &x) { return x.goodnight && !x.weekClosed; }
bool eClosed(const SceneCtx &x) { return x.goodnight && x.weekClosed; }
bool eAllClear(const SceneCtx &x) { return x.alertEnded; }
bool eStorm(const SceneCtx &x) { return x.weather == Weather::Storm; }
bool eFog(const SceneCtx &x) { return x.weather == Weather::Fog; }
bool eMeetingNow(const SceneCtx &x) { return x.meetingNow; }

const uint16_t K_GREY = C565(0x9A958C), K_AMB = C565(0xF0B429), K_GRN = C565(0x5FB97A), K_BLU = C565(0x6AA6F8),
               K_RED = C565(0xE5484D), K_PINK = C565(0xF08CB0), K_UB = C565(0x2E6BD8), K_UY = C565(0xF5C518),
               K_WOOD = C565(0x7A5A3A);
}  // namespace

const Scene SCENES_C[] PROGMEM = {
    {"ufo", SceneKind::Rare, false, 70, {K_GREY, K_BLU, K_AMB, C565(0x5A5434)}, ufo, nullptr, nullptr, nullptr, 0},
    {"asterisk", SceneKind::Rare, false, 40, {}, asterisk, nullptr, nullptr, nullptr, 0},
    {"guest", SceneKind::Rare, false, 60, {K_BLU, K_AMB}, guest, nullptr, nullptr, nullptr, 0},
    {"dragon", SceneKind::Rare, false, 60, {K_GRN, C565(0x3E8A5A), C565(0xF0B429), K_AMB, C565(0xE8772E), K_RED}, dragon, nullptr, nullptr, nullptr, 0},
    {"knock", SceneKind::Rare, false, 48, {}, knock, nullptr, nullptr, nullptr, 0},
    {"glitch", SceneKind::Rare, false, 40, {C565(0x7A2A2E), C565(0x36557C), K_GRN, K_RED, K_BLU}, glitch, nullptr, nullptr, nullptr, 0},
    {"shooting-star", SceneKind::Rare, true, 48, {K_PINK}, shootingStar, nullptr, nullptr, nullptr, 0},
    {"hammock", SceneKind::Weekend, true, 60, {K_WOOD, C565(0xE8A060), C565(0xD07A3A)}, hammock, nullptr, nullptr, nullptr, 0},
    {"picnic", SceneKind::Weekend, true, 40, {K_RED, C565(0xB0803A), C565(0x8A5A2A), C565(0xD8A460), K_GRN}, picnic, nullptr, nullptr, nullptr, 0},
    {"sunbath", SceneKind::Weekend, true, 40, {K_BLU, C565(0x8A8478), K_AMB}, sunbath, nullptr, nullptr, nullptr, 0},
    {"chess", SceneKind::Weekend, true, 48, {C565(0x3A3732), C565(0xC8C0B0)}, chess, nullptr, nullptr, nullptr, 0},
    {"bored", SceneKind::Event, true, 60, {C565(0x8A8478)}, bored, eBored, nullptr, nullptr, 15},
    {"goodnight", SceneKind::Event, true, 60, {C565(0x5E5A53), C565(0x3A3220)}, goodnight, eGoodnight, nullptr, nullptr, 60},
    {"closed", SceneKind::Event, true, 48, {C565(0x8A8478), C565(0x8A5A2A)}, closed, eClosed, "До понеділка!", "", 120},
    {"kyiv", SceneKind::Holiday, true, 48, {K_WOOD, C565(0x2E7A3A)}, kyiv, hKyiv, "День Києва", "", 0},
    {"unity", SceneKind::Holiday, true, 48, {C565(0xE8A07A), C565(0x8A8478), K_UB, K_UY}, unity, hUnity, "День Соборності", "22.01", 0},
    {"flag-day", SceneKind::Holiday, true, 60, {C565(0xA8A399), K_UB, K_UY}, flagDay, hFlag, "День Прапора", "23.08", 0},
    {"cat-day", SceneKind::Holiday, true, 48, {C565(0x8A8478), C565(0xE8A060), K_PINK}, catDay, hCat, "День кота", "08.08", 0},
    {"clock-birthday", SceneKind::Holiday, true, 48, {K_PINK, C565(0xF4D8E0), K_AMB, C565(0x8A8478)}, clockBirthday, hClock, "Свято годинника", "25.11", 0},
    {"all-clear", SceneKind::Event, true, 48, {C565(0x5B6B3A), K_GRN}, allClear, eAllClear, nullptr, nullptr, 10},
    {"storm", SceneKind::Event, true, 60, {C565(0x5A5A5A), K_BLU, K_AMB, C565(0x3B7DD8), C565(0x8A8478)}, storm, eStorm, nullptr, nullptr, 30},
    {"fog", SceneKind::Event, true, 60, {C565(0x55524C)}, fog, eFog, nullptr, nullptr, 40},
    {"meeting-now", SceneKind::Event, true, 50, {K_GREY}, meetingNow, eMeetingNow, nullptr, nullptr, 5},
};
const int SCENES_C_N = sizeof(SCENES_C) / sizeof(SCENES_C[0]);
