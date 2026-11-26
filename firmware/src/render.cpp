#include "render.h"
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

void drawTopBar(const String &left, const String &right, uint16_t rightCol) {
    tft.fillRect(lay::CL - 2, 0, lay::CR - lay::CL + 4, 26, col::BG);
    text(F_TOP, left, lay::CL, lay::TOP_BASE, col::MUTED, col::BG, L_BASELINE);
    if (right.length() == 0) return;
    int x = lay::CR - textWidth(F_TOP, right);
    text(F_TOP, right, x, lay::TOP_BASE, rightCol, col::BG, L_BASELINE);
    tft.fillSmoothCircle(x - 8, 15, 4, rightCol, col::BG);
}

void drawGauge(int cx, const char *label, int pct, bool warn, bool stale, const char *line1, const char *line2, int pace) {
    bool timer = line1 && *line1;
    int bottom = timer ? lay::GAUGE_BOTTOM_T : lay::GAUGE_BOTTOM;
    uint16_t labelC = stale ? col::DIM : (warn ? col::PINK : col::MUTED);
    uint16_t fillC = stale ? col::DIM : (warn ? col::RED : col::CLAY);
    uint16_t pctC = stale ? col::DIM : (warn ? col::PINK : col::INK);
    tft.fillRect(cx - 13, 0, 26, 240, col::BG);
    text(F_LIM, label, cx, lay::TOP_BASE, labelC, col::BG, C_BASELINE);
    int h = bottom - lay::GAUGE_TOP;
    tft.fillSmoothRoundRect(cx - lay::GAUGE_W / 2, lay::GAUGE_TOP, lay::GAUGE_W, h, 3, col::TRACK, col::BG);
    int fh = h * pct / 100;
    if (fh > 0) tft.fillSmoothRoundRect(cx - lay::GAUGE_W / 2, bottom - fh, lay::GAUGE_W, fh, 3, fillC, col::TRACK);
    if (pace >= 0 && !stale) tft.fillRect(cx - lay::GAUGE_W / 2 - 3, bottom - h * pace / 100 - 1, lay::GAUGE_W + 6, 2, col::INK);
    text(F_LIM, String(pct), cx, timer ? lay::PCT_BASE_T : lay::BOTTOM_BASE, pctC, col::BG, C_BASELINE);
    if (timer) {
        text(F_S3, line1, cx, lay::RESET1_BASE, col::MUTED, col::BG, C_BASELINE);
        if (line2 && *line2) text(F_S3, line2, cx, lay::RESET2_BASE, col::MUTED, col::BG, C_BASELINE);
    }
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
