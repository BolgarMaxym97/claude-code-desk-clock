// Device palette (calibrated on the real panel), layout and timing.
#pragma once
#include <stdint.h>

// Optional developer secrets (built-in Wi-Fi, static IP, fixed token, birthday). Release builds have none:
// Wi-Fi comes from the setup page, the token is made on the device (device.h).
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif
#ifndef BIRTH_MONTH
#define BIRTH_MONTH 0  // birthday scene off unless set on the device (claude-clock config --birthday MM-DD)
#define BIRTH_DAY 0
#endif
#ifndef FW_VERSION
#define FW_VERSION "dev"  // CI passes -D FW_VERSION=\"x.y.z\"
#endif

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Palette calibrated on the real panel (it adds blue and lifts mid-tones).
namespace col {
constexpr uint16_t BG = rgb565(0x00, 0x00, 0x00);
constexpr uint16_t CARD = rgb565(0x0E, 0x0C, 0x0A);
constexpr uint16_t INK = rgb565(0xFF, 0xE8, 0xB0);
constexpr uint16_t MUTED = rgb565(0x70, 0x60, 0x48);
constexpr uint16_t DIM = rgb565(0x58, 0x4A, 0x36);
constexpr uint16_t CLAY = rgb565(0xD8, 0x40, 0x00);
constexpr uint16_t AMBER = rgb565(0xF0, 0xA0, 0x00);
constexpr uint16_t RED = rgb565(0xC8, 0x18, 0x18);
constexpr uint16_t PINK = rgb565(0xE8, 0x40, 0x30);
constexpr uint16_t GREEN = rgb565(0x50, 0xC0, 0x20);
constexpr uint16_t BLUE = rgb565(0x6A, 0xA6, 0xF8);
constexpr uint16_t TRACK = rgb565(0x1C, 0x18, 0x12);
constexpr uint16_t CHIP_WARN = rgb565(0x60, 0x10, 0x08);  // percent chip at >= 90 %
constexpr uint16_t CHIP = rgb565(0x4C, 0x2E, 0x0A);   // gauge label chip: warm dark amber (the panel adds blue to neutral greys)
constexpr uint16_t EYE = rgb565(0x00, 0x00, 0x00);
constexpr uint16_t LID = rgb565(0x70, 0x20, 0x00);
constexpr uint16_t GREY_BODY = rgb565(0x40, 0x38, 0x30);
constexpr uint16_t HAT_RED = rgb565(0xC8, 0x10, 0x10);
constexpr uint16_t HAT_WHITE = rgb565(0xFF, 0xF0, 0xD8);
constexpr uint16_t AMBER_BG = rgb565(0x24, 0x18, 0x00);
constexpr uint16_t SLEEP_INK = rgb565(0x50, 0x42, 0x30);
}  // namespace col

// Layout (device px).
namespace lay {
constexpr int CL = 28, CR = 212, CX = 120;     // content column between the gauges
constexpr int TOP_BASE = 20, BOTTOM_BASE = 230; // shared text baselines
// Both gauges: label (5h, 7d) on a chip at the top, then the bar (same length always), then % and time to reset.
constexpr int CHIP_Y = 26, CHIP_H = 20, CHIP_W = 28, CHIP_SHIFT = 1;  // both chips (label, percent): same size, 1 px inwards;
constexpr int LABEL_BASE = 41;                                       // label chip under the top row, as far from the top as the % chip from the bottom
constexpr int TOP_L = 6, TOP_R = 234, DOT_STEP = 11;
constexpr int TOPROW_BASE = 16;  // time / date / session dots: a little higher than the setup-screen titles (TOP_BASE)                                 // the top row spans the whole width
constexpr int GAUGE_TOP = 50, GAUGE_BOTTOM = 190, GAUGE_W = 8;
constexpr int PCT_CHIP_Y = 194, PCT_BASE = 209;  // percent on its own chip, like the label
constexpr int TIMER_BASE = 231;
constexpr int TIMER_HALF = 16, TIMER_SHIFT = 3;  // timer row: up to 32 px, centred 3 px inwards (screen bottom is free there)
constexpr int HERO_X = CL, HERO_Y = 44, HERO_W = 184, HERO_H = 60, HERO_PX = 6;
constexpr int VERB_Y = 112, TARGET_Y = 146, DETAIL_Y = 172;
constexpr int TIME_Y = 12, SMALL_Y = 96, SMALL_PX = 5, CARD_Y = 152, CARD_H = 34;
constexpr int TIME_CLEAR_TOP = 26;                  // clock: time area is cleared from here down to STAGE_Y
constexpr int STAGE_Y = 80, STAGE_H = 70;           // clock: idle mascot stage (4-bit sprite) between the gauges
constexpr int SLEEP_TIME_Y = 40, SLEEP_DATE_Y = 120, SLEEP_CLEAR_TOP = 30, SLEEP_CLEAR_BOTTOM = 140;
constexpr int SLEEP_STAGE_Y = 134, SLEEP_SPRITE_Y = 156;
constexpr int YEAR_Y = 172, YEAR_CLEAR_H = 40;      // New Year midnight banner (clock screen)
constexpr int QR_TOP = 26, QR_AP_Y = 208;           // setup screen
}  // namespace lay

constexpr uint32_t FRAME_MS = 420;
constexpr uint32_t NET_AP_AFTER_MS = 60000;
constexpr uint32_t NET_RETRY_MS = 30000;
