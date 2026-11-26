// Work screens, board "Work" (design/idle-anim/src/work.js): big mascot (6 px cells) at (78, 48) + props.
#include <math.h>
#include "scene.h"
#include "scenes_internal.h"

namespace {
const int HX = 78, HY = 48;

const char PLUS[] PROGMEM = "...x..." "...x..." "...x..." "xxxxxxx" "...x..." "...x..." "...x...";
const char CROSS[] PROGMEM = "x.....x" ".x...x." "..x.x.." "...x..." "..x.x.." ".x...x." "x.....x";
const char PENCIL[] PROGMEM = "..p" ".y." "g..";
const char MINI_A[] PROGMEM = ".oooooo." ".oeooeo." "oooooooo" ".oooooo." ".o.o.o.o";
const char MINI_B[] PROGMEM = ".oooooo." ".oeooeo." "oooooooo" ".oooooo." "o.o..o.o";
const char QUESTION[] PROGMEM = ".xx." "x..x" "...x" "..x." "...." "..x.";
const char LOGO[] PROGMEM = "x.x" ".x." "x.x";

MascotAt H(Canvas &c, const SpriteFrame &f, int x, int y) {
    frame(c, f, x, y, 6);
    return MascotAt{x, y, true};
}
bool odd(uint32_t p) { return (p >> 1) % 2; }

MascotAt walk(Canvas &c, uint32_t p, uint32_t) {
    MascotAt m = H(c, odd(p) ? SPR_WALK_B : SPR_WALK_A, HX, HY + (odd(p) ? 0 : -3));
    art(c, (p % 2) ? CROSS : PLUS, 7, 178, 52, 3, "x1");
    return m;
}

MascotAt bash(Canvas &c, uint32_t p, uint32_t) {  // 7 window 8 title 9 grey a green
    MascotAt m = H(c, (p % 2) ? SPR_ARM_A : SPR_ARM_L, HX, HY);
    R(c, 168, 52, 40, 34, S_7, 3);
    R(c, 168, 52, 40, 4, S_8, 2);
    for (int i = 0; i < 4; i++) {
        int len = int(((p + 40 - i * 10) % 40) * 2);
        if (len > 30) len = 30;
        R(c, 172, 58 + i * 7, len, 3, i == 3 ? S_A : S_9);
    }
    if ((p % 4) < 2) R(c, 172 + (int((p % 40) * 2) > 30 ? 30 : int((p % 40) * 2)) + 2, 79, 3, 4, S_INK);
    return m;
}

MascotAt edit(Canvas &c, uint32_t p, uint32_t) {  // 7 paper 8 lines 9 pink a amber b dark
    MascotAt m = H(c, (p % 2) ? SPR_ARM_A : SPR_ARM_B, HX, HY);
    R(c, 166, 50, 40, 40, S_7, 2);
    int q = int(p % 48);
    for (int i = 0; i < 4; i++) {
        int w = q * 3 - i * 32;
        R(c, 170, 56 + i * 8, w < 0 ? 0 : (w > 32 ? 32 : w), 2, S_8);
    }
    int idx = q / 11 > 3 ? 3 : q / 11, px = q * 3 - idx * 32;
    px = px < 0 ? 0 : (px > 32 ? 32 : px);
    art(c, PENCIL, 3, 170 + px - 2, 48 + idx * 8, 3, "p9yagb");
    return m;
}

MascotAt read(Canvas &c, uint32_t p, uint32_t) {  // 7 paper 8 lines 9 glass a wood
    MascotAt m = H(c, ((p >> 3) % 2) ? SPR_LOOK_L : SPR_BLINK, HX, HY);
    R(c, 32, 50, 40, 44, S_7, 2);
    for (int i = 0; i < 5; i++) R(c, 36, 56 + i * 7, 30 - (i % 2) * 8, 2, S_8);
    int mx = 40 + int(lroundf(18 + 16 * sinf(p / 5.0f))), my = 58 + int((p >> 3) % 3) * 8;
    R(c, mx - 9, my - 9, 18, 18, S_9, 9);
    R(c, mx + 6, my + 6, 4, 10, S_A, 1);
    return m;
}

MascotAt agents(Canvas &c, uint32_t p, uint32_t) {  // 7 helper body
    MascotAt m = H(c, odd(p) ? SPR_WALK_B : SPR_WALK_LB, HX, HY);
    int s = int((p >> 1) % 6);
    art(c, odd(p) ? MINI_A : MINI_B, 8, 34 + s, 70, 3, "o7e2");
    art(c, odd(p) ? MINI_B : MINI_A, 8, 176 - s, 70, 3, "o7e2");
    return m;
}

MascotAt web(Canvas &c, uint32_t p, uint32_t) {  // 7 navy 8 green 9 highlight
    MascotAt m = H(c, SPR_UP, HX, HY);
    R(c, 168, 50, 36, 36, S_7, 18);
    for (int k = 0; k < 3; k++) R(c, 168 + (p * 2 + k * 12) % 36, 54 + k * 9, 6, 6, S_8, 2);
    R(c, 186 + 10 * sinf(p / 3.0f), 58 + 10 * cosf(p / 3.0f), 14, 14, S_9, 7);
    return m;
}

MascotAt think(Canvas &c, uint32_t p, uint32_t) {  // 7 bubble
    MascotAt m = H(c, SPR_UP, HX, HY);
    R(c, 166, 48, 40, 24, S_7, 12);
    R(c, 160, 74, 8, 8, S_7, 4);
    R(c, 156, 84, 4, 4, S_7, 2);
    int n = int((p >> 2) % 4);
    for (int i = 0; i < n && i < 3; i++) R(c, 174 + i * 10, 58, 5, 5, S_INK, 3);
    return m;
}

MascotAt compact(Canvas &c, uint32_t p, uint32_t) {  // 7 blue 8 green 9 amber a purple
    int cp = int(p % 30), gap = cp < 10 ? 10 - cp : 0;
    MascotAt m = H(c, cp > 12 && cp < 22 ? SPR_SQUISH : SPR_IDLE, HX, HY);
    for (int i = 0; i < 4; i++) R(c, 168 + i * (6 + gap / 2.0f), 64, 6, 22 - (cp > 18 ? cp - 18 : 0), uint8_t(S_7 + i), 1);
    return m;
}

MascotAt permission(Canvas &c, uint32_t p, uint32_t) {  // 7 pole 8 amber
    MascotAt m = H(c, SPR_ARM_A, HX, HY + hop(p % 14, 1, 7, 6));
    R(c, 166, 44, 4, 44, S_7);
    R(c, 168, 44, 34, 26, S_8, 3);
    R(c, 183, 48, 4, 12, S_EYE);
    R(c, 183, 63, 4, 4, S_EYE);
    return m;
}

MascotAt yourTurn(Canvas &c, uint32_t p, uint32_t) {  // 7 bubble 8 amber
    MascotAt m = H(c, odd(p) ? SPR_ARM_A : SPR_ARM_B, HX, HY);
    R(c, 168, 44, 30, 26, S_7, 10);
    art(c, QUESTION, 4, 177, 49, 3, "x8");
    return m;
}

MascotAt done(Canvas &c, uint32_t p, uint32_t) {  // 7 red 8 amber 9 green a blue
    for (int i = 0; i < 14; i++) {
        uint32_t q = (p * 2 + i * 13) % 90;
        if (q < 70) R(c, 34 + (i * 29) % 172 + 3 * sinf((p + i) / 3.0f), 30 + q, 4, 4, uint8_t(S_7 + i % 4));
    }
    return H(c, SPR_HAPPY, HX, HY + hop(p % 12, 1, 12, 10));
}

MascotAt error(Canvas &c, uint32_t p, uint32_t) {  // 7 smoke
    for (int i = 0; i < 3; i++) {
        uint32_t q = (p + i * 6) % 18;
        int s = 8 + int(q / 3);
        if (q < 12) R(c, 150 + i * 6 + q, 44 - q * 1.2f, s, s, S_7, 6);
    }
    return H(c, SPR_ERR, HX + ((p % 6) < 3 ? ((p % 2) ? 3 : -3) : 0), HY);
}

MascotAt laptopWork(Canvas &c, uint32_t p, uint32_t) {  // 7 grey 8 dark
    MascotAt m = H(c, (p % 2) ? SPR_ARM_A : SPR_ARM_L, HX, HY);
    R(c, 86, 80, 68, 18, S_7, 2);
    R(c, 80, 98, 80, 3, S_8, 1);
    art(c, LOGO, 3, 115, 84, 3, "x1");
    return m;
}

// ---- reactions and situations (board "Pack3", redrawn for the big mascot) ----
const char CHECK[] PROGMEM = ".....x" "....xx" "x..xx." "xxxx.." ".xx...";
const char XMARK[] PROGMEM = "x...x" ".x.x." "..x.." ".x.x." "x...x";
const char GROCKET[] PROGMEM = "..o.." ".ooo." ".owo." ".ooo." "o.o.o";
const char NOTE[] PROGMEM = "..x" "..x" "xxx" "xx.";
const char HEART[] PROGMEM = ".x.x." "xxxxx" "xxxxx" ".xxx." "..x..";
const char CUP[] PROGMEM = "ww." "wwk" "ww.";

void confetti(Canvas &c, uint32_t p, int n) {  // slots 7..a
    for (int i = 0; i < n; i++) {
        uint32_t q = (p * 2 + i * 13) % 70;
        R(c, 34 + (i * 29) % 172 + 3 * sinf((p + i) / 3.0f), 40 + q, 4, 4, uint8_t(S_7 + i % 4));
    }
}

MascotAt testsOk(Canvas &c, uint32_t p, uint32_t) {  // 7..a confetti b green
    confetti(c, p, 10);
    MascotAt m = H(c, SPR_HAPPY, HX, HY + hop(p % 10, 1, 10, 8));
    art(c, CHECK, 6, 172, 54, 4, "xb");
    return m;
}

MascotAt testsFail(Canvas &c, uint32_t p, uint32_t) {  // 7 red
    MascotAt m = H(c, (p % 30) < 20 ? SPR_SHOUT : SPR_ERR, HX + ((p % 30) < 20 ? ((p % 2) ? 2 : -2) : 0), HY);
    art(c, XMARK, 5, 174, 54, 4, "x7");
    return m;
}

MascotAt commit(Canvas &c, uint32_t p, uint32_t) {  // 7 flap 8 handle 9 red a green
    uint32_t q = p % 36;
    float sy = q < 14 ? 44 + q * 1.6f : q < 20 ? 66 : 66 - (q - 20) * 1.4f;
    MascotAt m = H(c, q < 20 ? SPR_ARM_A : SPR_HAPPY, HX, HY);
    R(c, 166, 70, 36, 24, S_INK, 1);
    R(c, 166, 70, 18, 2, S_7);
    R(c, 184, 70, 18, 2, S_7);
    if (q >= 18) art(c, CHECK, 6, 175, 74, 3, "xa");
    R(c, 181, sy - 12, 4, 12, S_8);
    R(c, 177, sy, 12, 4, S_9, 1);
    return m;
}

MascotAt push(Canvas &c, uint32_t p, uint32_t) {  // 7 orange 8 amber 9 smoke
    uint32_t q = p % 40;
    float y = q < 10 ? 70 : 70 - (q - 10) * 3.0f;
    art(c, GROCKET, 5, 176, y, 4, "o7w6");
    if (q >= 8)
        for (int k = 0; k < 4; k++) R(c, 182 + (k % 2), y + 20 + k * 5, 4, 4, k < 2 ? S_8 : S_9);
    return H(c, q < 10 ? SPR_WALK_A : (((p >> 1) % 2) ? SPR_ARM_A : SPR_ARM_B), HX, HY);
}

MascotAt install(Canvas &c, uint32_t p, uint32_t) {  // 7 box 8 lid 9 tape a..d items
    uint32_t q = p % 40;
    MascotAt m = H(c, q < 12 ? SPR_WALK_A : SPR_ARM_A, HX, HY);
    R(c, 168, 72, 32, 24, S_7, 1);
    R(c, 168, q < 12 ? 72 : 66, 32, 5, S_8);
    R(c, 183, 72, 2, 24, S_9);
    if (q >= 14)
        for (int i = 0; i < 4; i++) R(c, 172 + i * 7, 64 - ((q - 14 + i * 3) % 14) * 1.5f, 5, 5, uint8_t(S_A + i));
    return m;
}

MascotAt docker(Canvas &c, uint32_t p, uint32_t) {  // 7 whale 8 red 9 amber a green b spout
    int wy = int(lroundf(2 * sinf(p / 3.0f)));
    MascotAt m = H(c, SPR_WALK_A, HX, HY);
    for (int i = 0; i < 3; i++) R(c, 170 + i * 11, 62 + wy, 10, 9, uint8_t(S_8 + i), 1);
    R(c, 166, 72 + wy, 40, 22, S_7, 10);
    R(c, 200, 68 + wy, 10, 8, S_7, 4);
    R(c, 172, 78 + wy, 3, 3, S_INK);
    if ((p % 20) < 10) R(c, 186, 54 + wy - float(p % 10), 2, 6, S_B);
    return m;
}

MascotAt longCmd(Canvas &c, uint32_t p, uint32_t) {  // 7 wood 8 glass 9 sand
    bool flip = (p / 24) % 2;
    float sand = (p % 24) / 24.0f;
    MascotAt m = H(c, SPR_SQUISH, HX, HY + 4);
    R(c, 176, 48, 20, 3, S_7);
    R(c, 176, 92, 20, 3, S_7);
    R(c, 178, 51, 16, 41, S_8, 5);
    R(c, 180, flip ? 54 : 72, 12, int(15 * (1 - sand)), S_9, 2);
    R(c, 180, flip ? 89 - 15 * sand : 69 - 15 * sand, 12, int(15 * sand), S_9, 2);
    R(c, 185, 69, 2, 5, S_9);
    R(c, 160, (p % 4) < 2 ? 86 : 83, 8, 4, S_BODY);  // tapping fingers
    return m;
}

MascotAt edits(Canvas &c, uint32_t p, uint32_t) {  // 7 sheet 8 sheet2
    int n = int((p % 64) / 7) + 1;
    if (n > 9) n = 9;
    for (int i = 0; i < n; i++) R(c, 168 + (i % 2) * 2, 98 - i * 5, 34, 4, i % 2 ? S_7 : S_8);
    return H(c, ((p >> 1) % 2) ? SPR_ARM_A : SPR_ARM_B, HX, HY);
}

MascotAt orchestra(Canvas &c, uint32_t p, uint32_t) {  // 7 helpers
    bool a = (p >> 1) % 2;
    art(c, a ? MINI_A : MINI_B, 8, 34, 70, 3, "o7e2");
    art(c, a ? MINI_B : MINI_A, 8, 178, 70, 3, "o7e2");
    for (int i = 0; i < 2; i++) {
        uint32_t q = (p + i * 8) % 16;
        if (q < 12) art(c, NOTE, 3, 166 + i * 14 + q, 70 - q * 2.0f, 2, "x6");
    }
    MascotAt m = H(c, a ? SPR_ARM_A : SPR_ARM_B, HX, HY);
    R(c, 162, a ? 44 : 52, 2, 12, S_INK);
    return m;
}

MascotAt morningHello(Canvas &c, uint32_t p, uint32_t) {  // 7 sun
    uint32_t q = p % 60;
    R(c, 186, 104 - fminf(48, q * 1.6f), 18, 18, S_7, 9);
    const SpriteFrame &f = q < 12 ? SPR_SLEEP : q < 22 ? SPR_SQUISH : q < 30 ? SPR_UP : (((p >> 1) % 2) ? SPR_ARM_A : SPR_ARM_B);
    return H(c, f, HX, q < 12 ? HY + 8 : HY);
}

MascotAt backHello(Canvas &c, uint32_t p, uint32_t) {  // 7 pink
    uint32_t q = p % 50;
    float x = q < 18 ? 20 + q * 3.2f : HX;
    if (q >= 22 && q < 40) art(c, HEART, 5, 176, 52, 3, "x7");
    return H(c, q < 18 ? (((p >> 1) % 2) ? SPR_WALK_B : SPR_WALK_A) : (((p >> 1) % 2) ? SPR_ARM_A : SPR_HAPPY), int(lroundf(x)), HY);
}

MascotAt lateNight(Canvas &c, uint32_t p, uint32_t) {  // 7 moon 8 mug 9 coffee
    R(c, 186, 48, 16, 16, S_7, 8);
    R(c, 192, 46, 14, 14, S_BG, 7);
    MascotAt m = H(c, (p % 30) < 8 ? SPR_EAT_B : SPR_DROWSY, HX + int(lroundf(2 * sinf(p / 6.0f))), HY);
    art(c, CUP, 3, 164, 78, 5, "w8k8");
    R(c, 164, 78, 10, 2, S_9);
    return m;
}

MascotAt doneSmall(Canvas &c, uint32_t p, uint32_t) {  // 7 green
    MascotAt m = H(c, SPR_HAPPY, HX, HY + (((p >> 2) % 2) ? 2 : 0));
    art(c, CHECK, 6, 174, 56, 3, "x7");
    return m;
}

const uint16_t K_DARK = C565(0x1C1A17), K_GREY = C565(0xA8A399), K_GRN = C565(0x5FB97A), K_AMB = C565(0xF0B429),
               K_BLU = C565(0x6AA6F8), K_RED = C565(0xE5484D);
}  // namespace

const Scene SCENES_W[] PROGMEM = {
    {"work:walk", SceneKind::Work, false, 48, {}, walk, nullptr, nullptr, nullptr},
    {"work:bash", SceneKind::Work, false, 80, {K_DARK, C565(0x3A3732), K_GREY, K_GRN}, bash, nullptr, nullptr, nullptr},
    {"work:edit", SceneKind::Work, false, 96, {C565(0xE8E2D4), C565(0x6E6A62), C565(0xF08CB0), K_AMB, C565(0x3A3732)}, edit, nullptr, nullptr, nullptr},
    {"work:read", SceneKind::Work, false, 60, {C565(0xE8E2D4), C565(0x8A8478), C565(0x3A5A80), C565(0xB0603A)}, read, nullptr, nullptr, nullptr},
    {"work:agents", SceneKind::Work, false, 48, {C565(0xE8A07A)}, agents, nullptr, nullptr, nullptr},
    {"work:web", SceneKind::Work, false, 72, {C565(0x1F3A5C), K_GRN, C565(0x4A6A8C)}, web, nullptr, nullptr, nullptr},
    {"work:think", SceneKind::Work, false, 48, {C565(0x2A2825)}, think, nullptr, nullptr, nullptr},
    {"work:compact", SceneKind::Work, false, 60, {K_BLU, K_GRN, K_AMB, C565(0xB07AE8)}, compact, nullptr, nullptr, nullptr},
    {"work:permission", SceneKind::Work, false, 42, {K_GREY, K_AMB}, permission, nullptr, nullptr, nullptr},
    {"work:your-turn", SceneKind::Work, false, 48, {C565(0x2A2825), K_AMB}, yourTurn, nullptr, nullptr, nullptr},
    {"work:done", SceneKind::Work, false, 48, {K_RED, K_AMB, K_GRN, K_BLU}, done, nullptr, nullptr, nullptr},
    {"work:error", SceneKind::Work, false, 36, {C565(0x6E6A62)}, error, nullptr, nullptr, nullptr},
    {"work:laptop", SceneKind::Work, false, 48, {C565(0x9A958C), C565(0x6E6A62)}, laptopWork, nullptr, nullptr, nullptr},
    {"work:tests-ok", SceneKind::Work, false, 40, {K_RED, K_AMB, K_GRN, K_BLU, K_GRN}, testsOk, nullptr, nullptr, nullptr},
    {"work:tests-fail", SceneKind::Work, false, 30, {K_RED}, testsFail, nullptr, nullptr, nullptr},
    {"work:commit", SceneKind::Work, false, 36, {C565(0xC8C0B0), C565(0x8A4A1A), K_RED, K_GRN}, commit, nullptr, nullptr, nullptr},
    {"work:push", SceneKind::Work, false, 40, {C565(0xF05032), K_AMB, C565(0x8A8478)}, push, nullptr, nullptr, nullptr},
    {"work:install", SceneKind::Work, false, 40, {C565(0xB0803A), C565(0x8A5A2A), C565(0xD8B070), K_AMB, K_GRN, K_BLU, C565(0xF08CB0)}, install, nullptr, nullptr, nullptr},
    {"work:docker", SceneKind::Work, false, 40, {C565(0x3B7DD8), K_RED, K_AMB, K_GRN, K_BLU}, docker, nullptr, nullptr, nullptr},
    {"work:long", SceneKind::Work, false, 48, {C565(0xB0603A), C565(0x2A2E33), K_AMB}, longCmd, nullptr, nullptr, nullptr},
    {"work:edits", SceneKind::Work, false, 64, {C565(0xF4F1EA), C565(0xE2DCCD)}, edits, nullptr, nullptr, nullptr},
    {"work:orchestra", SceneKind::Work, false, 48, {C565(0xE8A07A)}, orchestra, nullptr, nullptr, nullptr},
    {"work:morning", SceneKind::Work, false, 60, {K_AMB}, morningHello, nullptr, nullptr, nullptr},
    {"work:back", SceneKind::Work, false, 50, {C565(0xF08CB0)}, backHello, nullptr, nullptr, nullptr},
    {"work:late", SceneKind::Work, false, 60, {C565(0xE8E2D4), C565(0xC8C0B0), C565(0x7A4A2A)}, lateNight, nullptr, nullptr, nullptr},
    {"work:done-small", SceneKind::Work, false, 24, {K_GRN}, doneSmall, nullptr, nullptr, nullptr},
};
const int SCENES_W_N = sizeof(SCENES_W) / sizeof(SCENES_W[0]);
