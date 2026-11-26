// Layout check: nothing on any screen overlaps anything else.
// Text extents come from the real VLW fonts, computed the way TFT_eSPI 2.5 draws smooth fonts:
//   maxAscent = header ascent, maxDescent = max(header descent, glyph height - dY), cheight = sum;
//   datum shifts the box top (middle: -cheight/2, baseline: -maxAscent); glyph rows = top + maxAscent - dY .. + height.
#include <unity.h>
#include <map>
#include <string>
#include "config.h"
#include "fonts.h"
#include "idle.h"
#include "sprites.h"

void setUp() {}
void tearDown() {}

struct Box { int top, bottom; };  // rows [top, bottom)
enum Datum { TOP, MIDDLE, BASELINE };

static int be32(const uint8_t *p) { return int(int32_t(uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3])); }

static std::u32string utf32(const std::string &s) {
    std::u32string o;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        char32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
        for (int k = 1; k < n; k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        o += cp;
        i += n;
    }
    return o;
}

static Box ink(const uint8_t *f, const std::string &s, int y, Datum d) {
    int n = be32(f), asc = be32(f + 16), desc = be32(f + 20);
    std::map<char32_t, std::pair<int, int>> g;  // unicode -> (height, dY)
    for (int i = 0; i < n; i++) {
        const uint8_t *r = f + 24 + 28 * i;
        char32_t u = char32_t(be32(r));
        int h = be32(r + 4), dy = be32(r + 16);
        g[u] = {h, dy};
        if (((u > 0x20 && u < 0xA0 && u != 0x7F) || u > 0xFF) && h - dy > desc) desc = h - dy;
    }
    if (d == MIDDLE) y -= (asc + desc) / 2;
    if (d == BASELINE) y -= asc;
    Box b{10000, -10000};
    for (char32_t c : utf32(s)) {
        auto it = g.find(c);
        TEST_ASSERT_TRUE_MESSAGE(c == U' ' || it != g.end(), "glyph missing from font");
        if (it == g.end()) continue;
        int top = y + asc - it->second.second;
        if (top < b.top) b.top = top;
        if (top + it->second.first > b.bottom) b.bottom = top + it->second.first;
    }
    return b;
}

static int advance(const uint8_t *f, const std::string &s);

static void inside(Box in, int top, int bottom, const char *what) {
    TEST_ASSERT_TRUE_MESSAGE(in.top >= top && in.bottom <= bottom, what);
}
static void above(Box a, int belowTop, int gap, const char *what) {
    TEST_ASSERT_TRUE_MESSAGE(a.bottom + gap <= belowTop, what);
}

static const char *DIGITS = "0123456789:";
static Box topBar() {
    Box a = ink(F_TOP, "СР 25 ЛИСТ НД ВТ ЧТ", lay::TOP_BASE, BASELINE), b = ink(F_LIM, "5h7d", lay::TOP_BASE, BASELINE);
    return Box{a.top < b.top ? a.top : b.top, a.bottom > b.bottom ? a.bottom : b.bottom};
}

void test_clock_screen() {
    Box bar = topBar(), time = ink(F_TIME_S, DIGITS, lay::TIME_Y, TOP);
    above(bar, time.top, 3, "top bar touches the time");
    inside(time, lay::TIME_CLEAR_TOP, lay::STAGE_Y - 3, "time is cut by the mascot stage / not cleared");
    TEST_ASSERT_TRUE_MESSAGE(lay::STAGE_Y + lay::STAGE_H <= lay::CARD_Y, "stage overlaps the card");
    inside(ink(F_S, "Немає Wi-Fi Claude мовчить", lay::CARD_Y + 17, MIDDLE), lay::CARD_Y + 2, lay::CARD_Y + lay::CARD_H - 2,
           "card text leaves the card");
    inside(ink(F_LIM, "59′ 0123456789", lay::CARD_Y + 17, MIDDLE), lay::CARD_Y + 2, lay::CARD_Y + lay::CARD_H - 2, "card minutes");
    int cardW = lay::CR - lay::CL - 4;  // drawSilentCard: text at x+10, minutes right-aligned at x+w-8
    TEST_ASSERT_TRUE_MESSAGE(10 + advance(F_S, "Claude мовчить") + 6 + advance(F_LIM, "999′") + 8 <= cardW,
                             "silent card text runs into the minutes");
    Box year = ink(F_H, "2027", lay::YEAR_Y, MIDDLE);
    inside(year, lay::YEAR_Y - lay::YEAR_CLEAR_H / 2, lay::YEAR_Y + lay::YEAR_CLEAR_H / 2, "year not cleared");
    TEST_ASSERT_TRUE_MESSAGE(year.top >= lay::STAGE_Y + lay::STAGE_H, "year overlaps the stage");
    inside(ink(F_LIM, "100", lay::BOTTOM_BASE, BASELINE), 0, 240, "percent off screen");
}

static int advance(const uint8_t *f, const std::string &s) {
    int n = be32(f), w = 0;
    for (char32_t c : utf32(s))
        for (int i = 0; i < n; i++) {
            const uint8_t *r = f + 24 + 28 * i;
            if (char32_t(be32(r)) == c) { w += be32(r + 12); break; }
        }
    return w;
}

void test_reset_timer_under_gauge() {
    Box pct = ink(F_LIM, "100", lay::PCT_BASE_T, BASELINE);
    Box l1 = ink(F_S3, "2г14х3д4г05х", lay::RESET1_BASE, BASELINE), l2 = ink(F_S3, "2г14х3д4г05х", lay::RESET2_BASE, BASELINE);
    TEST_ASSERT_TRUE_MESSAGE(lay::GAUGE_BOTTOM_T + 3 <= pct.top, "bar touches the percent");
    above(pct, l1.top, 1, "percent touches the first timer line");
    above(l1, l2.top, 0, "timer lines overlap");
    TEST_ASSERT_TRUE(l2.bottom <= 240);
    const char *W[] = {"14х", "05х", "45х", "23г", "6д", "100"};
    for (const char *w : W) TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, w) <= 26, w);  // fits the 26 px gauge column
}

void test_sleep_screen() {
    Box time = ink(F_TIME_S, DIGITS, lay::SLEEP_TIME_Y, TOP), date = ink(F_TOP, "СР 25 ЛИСТ", lay::SLEEP_DATE_Y, TOP);
    inside(time, lay::SLEEP_CLEAR_TOP, lay::SLEEP_CLEAR_BOTTOM, "sleep time not cleared");
    inside(date, lay::SLEEP_CLEAR_TOP, lay::SLEEP_CLEAR_BOTTOM, "sleep date not cleared");
    above(time, date.top, 3, "sleep time touches the date");
    above(date, lay::SLEEP_STAGE_Y, 1, "sleep stage covers the date");
    TEST_ASSERT_TRUE(lay::SLEEP_STAGE_Y + lay::STAGE_H <= 240);
    TEST_ASSERT_TRUE(lay::SLEEP_SPRITE_Y + 8 * lay::SMALL_PX + 1 <= lay::SLEEP_STAGE_Y + lay::STAGE_H);
}

void test_idle_mascot_stays_inside_stage_also_with_hat() {
    const int sprY = lay::SMALL_Y - lay::STAGE_Y, h = 8 * lay::SMALL_PX;
    for (int season = 0; season < 2; season++) {
        IdleDirector d(21);
        d.setCalendar(season, false);
        for (uint32_t t = 0; t < 40000; t++) {
            IdlePose p = d.pose(t, 30, (t / 5000) % 2);
            int dy = p.dy;
            bool hat = season && !p.band;
            if (hat && dy < -1) dy = -1;  // same rule as drawStage()
            if (season) dy += 4;
            int top = sprY + dy - (hat ? 3 * lay::SMALL_PX : 0), bottom = sprY + dy + h;
            TEST_ASSERT_TRUE_MESSAGE(top >= 0, "mascot / hat clipped at the stage top");
            TEST_ASSERT_TRUE_MESSAGE(bottom <= lay::STAGE_H, "mascot clipped at the stage bottom");
        }
    }
}

void test_work_screens() {
    Box bar = topBar();
    TEST_ASSERT_TRUE_MESSAGE(bar.bottom + 3 <= lay::HERO_Y - 3 * lay::HERO_PX + 4, "hat on the big mascot hits the top bar");
    // line(slot, y, h): clears [y-2, y-2+h), text TC at y
    inside(ink(F_H, "ВиконуєРедагуєЧитаєДелегуєШукаєПрацюєГотово", lay::VERB_Y, TOP), lay::VERB_Y - 2, lay::VERB_Y - 2 + 32, "verb");
    inside(ink(F_MONO, "npm test…_gy[]{}|", lay::TARGET_Y, TOP), lay::TARGET_Y - 2, lay::TARGET_Y - 2 + 22, "target");
    inside(ink(F_S3, "5 хв · main · агентів 2", lay::DETAIL_Y, TOP), lay::DETAIL_Y - 2, lay::DETAIL_Y - 2 + 20, "detail");
    TEST_ASSERT_TRUE_MESSAGE(lay::HERO_Y + lay::HERO_H <= lay::VERB_Y - 2, "mascot overlaps the verb");
    TEST_ASSERT_TRUE(lay::VERB_Y - 2 + 32 <= lay::TARGET_Y - 2);
    TEST_ASSERT_TRUE(lay::TARGET_Y - 2 + 22 <= lay::DETAIL_Y - 2);
    // limit screen: line(0, 80, 32, F_H) and line(1, 112, 56, F_BIG)
    inside(ink(F_H, "Ліміт", 80, TOP), 78, 110, "limit title");
    inside(ink(F_BIG, DIGITS, 112, TOP), 110, 166, "limit countdown");
}

void test_setup_qr_page() {
    const int qrBottom = lay::QR_TOP + (29 + 2 * 2) * 5;  // version 3 + quiet zone, 5 px modules
    Box ap = ink(F_S, "Claude-Clock · abcdefgh", lay::QR_AP_Y, MIDDLE);
    Box hint = ink(F_S3, "Скануй камерою телефона", lay::BOTTOM_BASE, BASELINE);
    TEST_ASSERT_TRUE_MESSAGE(topBar().bottom + 3 <= lay::QR_TOP, "title touches the QR");
    above(Box{lay::QR_TOP, qrBottom}, ap.top, 3, "QR touches the AP line");
    above(ap, hint.top, 2, "AP line touches the hint");
    TEST_ASSERT_TRUE(hint.bottom <= 240);
}

void test_pairing_screen_texts_fit() {  // drawPair(): widths on a 240 px screen, code inside its 180 px box
    TEST_ASSERT_TRUE_MESSAGE(advance(F_TOP, "ПІДКЛЮЧЕННЯ КОМП’ЮТЕРА") <= 232, "pair title");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_S, "Введи код на комп’ютері:") <= 232, "pair hint");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_H, "WWWWWW") <= 170, "pair code");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, "claude-clock.local · 192.168.100.200") <= 232, "pair address");
    Box code = ink(F_H, "WMQ2K7", 116, MIDDLE);
    inside(code, 92, 140, "code leaves its box");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_clock_screen);
    RUN_TEST(test_sleep_screen);
    RUN_TEST(test_reset_timer_under_gauge);
    RUN_TEST(test_idle_mascot_stays_inside_stage_also_with_hat);
    RUN_TEST(test_work_screens);
    RUN_TEST(test_setup_qr_page);
    RUN_TEST(test_pairing_screen_texts_fit);
    return UNITY_END();
}
