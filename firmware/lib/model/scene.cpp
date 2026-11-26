#include "scene.h"
#include <math.h>
#include <string.h>

void R(Canvas &c, float x, float y, int w, int h, uint8_t slot, int radius) {
    if (w <= 0 || h <= 0) return;
    c.rect(int(lroundf(x)), int(lroundf(y)), w, h, slot, radius);
}

void frame(Canvas &c, const SpriteFrame &f, int x, int y, int px, uint8_t body, int rows) {
    for (int r = 0; r < f.rows && r < rows; r++)
        for (int k = 0; k < 14; k++) {
            uint8_t s;
            switch (spx(f, r, k)) {
                case 'o': s = body; break;
                case 'e': s = S_EYE; break;
                case 'b': s = S_LID; break;
                case 'r': s = S_HATR; break;
                case 'h': s = S_HATR; break;
                case 'w': s = S_HATW; break;
                case 'k': s = S_INK; break;
                default: continue;
            }
            c.rect(x + k * px, y + r * px, px, px, s, 0);
        }
}

static int hexSlot(char h) { return h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : -1; }

void art(Canvas &c, const char *pgmRows, int cols, float x, float y, int s, const char *map) {
    int n = int(strlen_P(pgmRows)), ix = int(lroundf(x)), iy = int(lroundf(y));
    for (int i = 0; i < n; i++) {
        char ch = char(pgm_read_byte(pgmRows + i));
        if (ch == '.') continue;
        for (const char *m = map; m[0] && m[1]; m += 2)
            if (m[0] == ch) {
                int slot = hexSlot(m[1]);
                if (slot >= 0) c.rect(ix + (i % cols) * s, iy + (i / cols) * s, s, s, uint8_t(slot), 0);
                break;
            }
    }
}

int hop(uint32_t p, uint32_t n, uint32_t len, int h) {
    if (p >= n * len) return 0;
    float q = float(p % len) / len;
    return -int(lroundf(h * 4 * q * (1 - q)));
}

void orthodoxEaster(int year, int &month, int &day) {
    // Meeus: Julian calendar Easter, then +13 days (valid 1900-2099) to the Gregorian calendar.
    int a = year % 4, b = year % 7, c = year % 19, d = (19 * c + 15) % 30, e = (2 * a + 4 * b - d + 34) % 7;
    int m = (d + e + 114) / 31, dd = (d + e + 114) % 31 + 1 + 13;
    int dim = m == 3 ? 31 : m == 4 ? 30 : 31;
    if (dd > dim) { dd -= dim; m++; }
    month = m;
    day = dd;
}

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int dayOfYear(int year, int month, int day) {
    static const int CUM[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return CUM[month - 1] + day + (month > 2 && leap(year) ? 1 : 0);
}

static int weekday(int y, int m, int d) {  // 0 = Sunday (Sakamoto)
    static const int T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7;
}

int nthWeekdayOfMonth(int year, int month, int wday, int n) {
    int first = weekday(year, month, 1);
    return 1 + (wday - first + 7) % 7 + 7 * (n - 1);
}
