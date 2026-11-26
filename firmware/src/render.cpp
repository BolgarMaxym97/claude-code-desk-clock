#include "render.h"
#include "model.h"
#include "config.h"
#include "fonts.h"

void text(const uint8_t *font, const String &s, int x, int y, uint16_t fg, uint16_t bg, uint8_t datum, bool fill) {
    tft.loadFont(font);
    tft.setTextDatum(datum);
    tft.setTextColor(fg, bg, fill);
    tft.drawString(s, x, y);
    tft.unloadFont();
}

int textWidth(const uint8_t *font, const String &s) {
    tft.loadFont(font);
    int w = tft.textWidth(s);
    tft.unloadFont();
    return w;
}

void drawTopBar(const String &left, int busy, int total, uint16_t dotCol, uint16_t mark) {
    tft.fillRect(lay::TOP_L - 2, 0, lay::TOP_R - lay::TOP_L + 4, lay::CHIP_Y - 1, col::BG);
    text(F_TOP, left, lay::TOP_L, lay::TOPROW_BASE, col::INK, col::BG, L_BASELINE);
    if (mark) text(F_TOP, "!", lay::TOP_L + textWidth(F_TOP, left) + 8, lay::TOPROW_BASE, mark, col::BG, L_BASELINE);
    SessionDots d = sessionDots(busy, total);
    int x = lay::TOP_R;
    if (d.more > 0) {  // "+3" after the dots
        String m = String("+") + d.more;
        x -= textWidth(F_TOP, m);
        text(F_TOP, m, x, lay::TOPROW_BASE, dotCol, col::BG, L_BASELINE);
        x -= 4;
    }
    for (int i = d.shown - 1; i >= 0; i--) {  // filled (busy) first from the left, idle rings after
        int cx = x - 4 - (d.shown - 1 - i) * lay::DOT_STEP;
        tft.fillSmoothCircle(cx, lay::TOPROW_BASE - 5, 4, dotCol, col::BG);
        if (i >= d.filled) tft.fillSmoothCircle(cx, lay::TOPROW_BASE - 5, 2, col::BG, dotCol);
    }
}

// Chip with pixel-exact, identical corners on all four sides (fillSmoothRoundRect draws the bottom corners
// differently from the top ones at this size).
static void chip(int x, int y, int w, int h, uint16_t c) {
    tft.fillRect(x + 2, y, w - 4, h, c);
    tft.fillRect(x + 1, y + 1, w - 2, h - 2, c);
    tft.fillRect(x, y + 2, w, h - 4, c);
}

static int barX(int cx) { return (cx < 120 ? cx + lay::CHIP_SHIFT : cx - lay::CHIP_SHIFT) - lay::GAUGE_W / 2; }

void drawGaugeBar(int cx, int pct, bool warn, bool stale, int pace, bool full) {
    int x = barX(cx), bottom = lay::GAUGE_BOTTOM, h = bottom - lay::GAUGE_TOP, fh = h * pct / 100;
    uint16_t fillC = stale ? col::DIM : (warn ? col::RED : col::CLAY);
    if (full) tft.fillSmoothRoundRect(x, lay::GAUGE_TOP, lay::GAUGE_W, h, 3, col::TRACK, col::BG);
    else if (bottom - fh > lay::GAUGE_TOP + 4) tft.fillRect(x, lay::GAUGE_TOP + 4, lay::GAUGE_W, bottom - fh - lay::GAUGE_TOP - 4, col::TRACK);  // animation: no flicker
    if (fh > 0) tft.fillSmoothRoundRect(x, bottom - fh, lay::GAUGE_W, fh, 3, fillC, col::TRACK);
    if (pace >= 0 && !stale) tft.fillRect(x - 3, bottom - h * pace / 100 - 1, lay::GAUGE_W + 6, 2, col::INK);
}

void drawGauge(int cx, const char *label, int pct, int shownPct, bool warn, bool stale, const char *timer, int pace) {
    uint16_t textC = stale ? col::DIM : (warn ? col::PINK : col::INK);
    tft.fillRect(cx - 13, lay::CHIP_Y, 26, 240 - lay::CHIP_Y, col::BG);  // the top row above belongs to drawTopBar
    int chx = cx < 120 ? cx + lay::CHIP_SHIFT : cx - lay::CHIP_SHIFT;  // chips and bar share this centre
    tft.fillRect(chx - lay::CHIP_W / 2, lay::CHIP_Y, lay::CHIP_W, lay::CHIP_H, col::BG);
    chip(chx - lay::CHIP_W / 2, lay::CHIP_Y, lay::CHIP_W, lay::CHIP_H, col::CHIP);
    text(F_LIM, label, chx, lay::LABEL_BASE, stale ? col::DIM : (warn ? col::PINK : col::INK), col::CHIP, C_BASELINE, false);  // no box fill: it would stick out of the chip
    drawGaugeBar(cx, shownPct, warn, stale, pace, true);
    int tx = cx < 120 ? cx + lay::TIMER_SHIFT : cx - lay::TIMER_SHIFT;  // a bit inwards: "45хв" / "100" are wider than the column
    uint16_t chipC = warn && !stale ? col::CHIP_WARN : col::CHIP;
    tft.fillRect(tx - lay::TIMER_HALF, lay::PCT_CHIP_Y, 2 * lay::TIMER_HALF, lay::CHIP_H, col::BG);
    chip(chx - lay::CHIP_W / 2, lay::PCT_CHIP_Y, lay::CHIP_W, lay::CHIP_H, chipC);
    text(F_LIM, String(pct), chx, lay::PCT_BASE, warn && !stale ? col::INK : textC, chipC, C_BASELINE, false);
    tft.fillRect(tx - lay::TIMER_HALF, lay::TIMER_BASE - 13, 2 * lay::TIMER_HALF, 16, col::BG);
    if (timer && *timer) text(F_S3, timer, tx, lay::TIMER_BASE, stale ? col::DIM : col::MUTED, col::BG, C_BASELINE);  // quiet
}

void drawSprite(TFT_eSPI &g, const SpriteFrame &f, int x, int y, int px, uint16_t body) {
    for (int r = 0; r < f.rows; r++) {
        for (int c = 0; c < 14; c++) {
            uint16_t color;
            switch (spx(f, r, c)) {
                case 'o': color = body; break;
                case 'e': color = col::EYE; break;
                case 'b': color = col::LID; break;
                case 'r': color = col::RED; break;
                case 'h': color = col::HAT_RED; break;
                case 'w': color = col::HAT_WHITE; break;
                default: continue;
            }
            g.fillRect(x + c * px, y + r * px, px, px, color);
        }
    }
}

void drawSpinner(TFT_eSPI &g, int cx, int cy, uint32_t tick, uint16_t c, uint16_t bg) {
    float a0 = (tick % 8) * (PI / 8);
    for (int i = 0; i < 3; i++) {
        float a = a0 + i * PI / 3;
        float dx = cos(a) * 10, dy = sin(a) * 10;
        g.drawWideLine(cx - dx, cy - dy, cx + dx, cy + dy, 3, c, bg);
    }
}

void drawBang(TFT_eSPI &g, int cx, int top, uint16_t c) {
    g.fillRect(cx - 1, top, 2, 7, c);
    g.fillRect(cx - 1, top + 9, 2, 2, c);
}
