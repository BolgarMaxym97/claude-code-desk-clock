// Layout check: nothing on any screen overlaps anything else.
// Text extents come from the real VLW fonts, computed the way TFT_eSPI 2.5 draws smooth fonts:
//   maxAscent = header ascent, maxDescent = max(header descent, glyph height - dY), cheight = sum;
//   datum shifts the box top (middle: -cheight/2, baseline: -maxAscent); glyph rows = top + maxAscent - dY .. + height.
#include <unity.h>
#include <map>
#include <string>
#include "config.h"
#include "fonts.h"
#include "i18n.h"
#include "info.h"
#include "model.h"
#include "idle.h"
#include "scene.h"
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
    Box a = ink(F_TOP, "СР 25.11 НД ВТ ЧТ ПН ПТ СБ 23:59 +9", lay::TOPROW_BASE, BASELINE);
    TEST_ASSERT_TRUE_MESSAGE(a.top >= 3, "top row too close to the screen edge");
    return a;
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

void test_gauge_label_chip_and_rows() {  // drawGauge(): label on a chip at the top, bar, then % / timer rows
    Box lab = ink(F_LIM, "5h7d", lay::LABEL_BASE, BASELINE);
    TEST_ASSERT_TRUE_MESSAGE(lab.top >= lay::CHIP_Y + 1 && lab.bottom <= lay::CHIP_Y + lay::CHIP_H - 1, "label leaves its chip");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_LIM, "5h") + 4 <= lay::CHIP_W, "label wider than the chip");
    TEST_ASSERT_TRUE_MESSAGE(lay::CHIP_Y >= topBar().bottom + 2, "label chip touches the top row");
    // symmetric: label chip as far from the top edge as the percent chip from the bottom edge
    TEST_ASSERT_INT_WITHIN_MESSAGE(1, lay::CHIP_Y, 240 - (lay::PCT_CHIP_Y + lay::CHIP_H), "chips not symmetric");
    TEST_ASSERT_TRUE_MESSAGE(lay::TOP_L <= 8 && lay::TOP_R >= 232, "top row does not use the full width");
    TEST_ASSERT_TRUE_MESSAGE(13 + lay::CHIP_SHIFT - lay::CHIP_W / 2 >= 0, "chip leaves the screen");
    // the content column [CL, CR) is cleared on every redraw (time, work lines): the chips must stay outside it
    TEST_ASSERT_TRUE_MESSAGE(13 + lay::CHIP_SHIFT - lay::CHIP_W / 2 + lay::CHIP_W <= lay::CL, "left chip reaches into the column");
    TEST_ASSERT_TRUE_MESSAGE(227 - lay::CHIP_SHIFT - lay::CHIP_W / 2 >= lay::CR, "right chip reaches into the column");
    TEST_ASSERT_TRUE_MESSAGE(lay::CHIP_Y + lay::CHIP_H + 4 <= lay::GAUGE_TOP, "bar starts at the chip");
    Box pct = ink(F_LIM, "100", lay::PCT_BASE, BASELINE);
    Box tim = ink(F_S3, "4:59 23г 45хв 6д 0хв", lay::TIMER_BASE, BASELINE);
    // percent on its own chip (like the label): chip holds the digits, sits between the bar and the timer
    const int pcTop = lay::PCT_CHIP_Y, pcBottom = lay::PCT_CHIP_Y + lay::CHIP_H;
    TEST_ASSERT_TRUE_MESSAGE(pct.top >= pcTop + 1 && pct.bottom <= pcBottom - 1, "percent leaves its chip");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_LIM, "100") + 2 <= lay::CHIP_W, "100 wider than its chip");
    // text sits in the middle of its chip: same gap above and below, both chips; same gap chip-bar at both ends
    TEST_ASSERT_EQUAL_MESSAGE(lab.top - lay::CHIP_Y, lay::CHIP_Y + lay::CHIP_H - lab.bottom, "label not centred in its chip");
    TEST_ASSERT_EQUAL_MESSAGE(pct.top - pcTop, pcBottom - pct.bottom, "percent not centred in its chip");
    TEST_ASSERT_EQUAL_MESSAGE(lay::CHIP_Y, 240 - pcBottom, "chips not symmetric");
    TEST_ASSERT_EQUAL_MESSAGE(lay::GAUGE_TOP - (lay::CHIP_Y + lay::CHIP_H), pcTop - lay::GAUGE_BOTTOM, "bar gaps differ");
    TEST_ASSERT_TRUE_MESSAGE(lay::GAUGE_BOTTOM + 3 <= pcTop, "bar touches the percent chip");
    TEST_ASSERT_TRUE_MESSAGE(pcBottom + 2 <= tim.top, "percent chip touches the timer");
    TEST_ASSERT_TRUE_MESSAGE(tim.bottom <= 240, "timer off screen");
    const char *W[] = {"4:59", "1:05", "23г", "45хв", "6д"};
    // the timer row may be up to 32 px: below y=190 nothing else is drawn next to the gauges
    TEST_ASSERT_TRUE_MESSAGE(13 + lay::TIMER_SHIFT - lay::TIMER_HALF >= 0, "timer row leaves the screen");
    for (const char *w : W) TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(2 * lay::TIMER_HALF, advance(F_S3, w), w);
    TEST_ASSERT_TRUE(advance(F_LIM, "100") <= 26);
    TEST_ASSERT_TRUE_MESSAGE(pcTop >= lay::DETAIL_Y - 2 + 20, "percent chip reaches the work detail line");
    TEST_ASSERT_TRUE_MESSAGE(pcTop >= lay::CARD_Y + lay::CARD_H, "percent chip reaches the info card");
    TEST_ASSERT_TRUE_MESSAGE(tim.top >= lay::CARD_Y + lay::CARD_H, "timer row reaches the info card");
}

void test_sleep_screen() {
    Box time = ink(F_TIME_S, DIGITS, lay::SLEEP_TIME_Y, TOP), date = ink(F_TOP, "СР 25.11", lay::SLEEP_DATE_Y, TOP);
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

void test_work_top_bar_has_time_and_date() {  // topBar() on work screens: "HH:MM  DAY DD MON" ... "● busy/total"
    int left = advance(F_TOP, "23:59  СР 30.09"), right = 6 * lay::DOT_STEP + 4 + advance(F_TOP, "+9");  // 6 dots + "+N"
    TEST_ASSERT_TRUE_MESSAGE(left + 6 + right <= lay::TOP_R - lay::TOP_L, "time + date run into the session counter");
}

void test_setup_result_texts_fit() {  // drawSetup() page 3: the reason is centred in F_H on a 240 px screen
    const char *R[] = {"Невірний пароль", "Мережі не видно", "Не підключився", "Немає адреси"};
    for (const char *r : R) TEST_ASSERT_TRUE_MESSAGE(advance(F_H, r) <= 228, r);
    TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, "Роутер не видав IP-адресу (DHCP)") <= 228, "dhcp hint");
}

void test_pairing_screen_texts_fit() {  // drawPair(): widths on a 240 px screen, code inside its 180 px box
    TEST_ASSERT_TRUE_MESSAGE(advance(F_TOP, "ПІДКЛЮЧЕННЯ КОМП’ЮТЕРА") <= 232, "pair title");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_S, "Введи код на комп’ютері:") <= 232, "pair hint");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_H, "WWWWWW") <= 170, "pair code");
    TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, "claude-clock.local · 192.168.100.200") <= 232, "pair address");
    Box code = ink(F_H, "WMQ2K7", 116, MIDDLE);
    inside(code, 92, 140, "code leaves its box");
}

void test_texts_fit_in_both_languages() {  // every place a translated text is drawn, for EN and UK
    for (Lang l : {Lang::En, Lang::Uk}) {
        setLang(l);
        const int full = 236, centre = 184;  // whole screen; content between the gauges
        Txt verbs[] = {T_V_RUN, T_V_EDIT, T_V_READ, T_V_DELEGATE, T_V_SEARCH, T_V_WORK, T_V_THINK, T_V_COMPACT,
                       T_V_ALLOW, T_V_YOURTURN, T_V_ERROR, T_DONE, T_V_HELLO, T_LIMIT};
        for (Txt t : verbs) TEST_ASSERT_TRUE_MESSAGE(advance(F_H, tr(t)) <= centre - 4, tr(t));
        for (Txt t : {T_WHY_FAIL, T_WHY_PASS, T_WHY_NOSSID, T_WHY_NOIP, T_CONNECTED})
            TEST_ASSERT_TRUE_MESSAGE(advance(F_H, tr(t)) <= 228, tr(t));
        for (Txt t : {T_STEP1, T_STEP2, T_STEP3, T_STEP3B, T_CHECKING})  // setup steps start at x = 24
            TEST_ASSERT_TRUE_MESSAGE(24 + advance(F_S, tr(t)) <= full, tr(t));
        for (Txt t : {T_SCAN_HINT, T_OR_OPEN, T_DHCP_HINT, T_TRY_AGAIN, T_NOT_SAVED, T_PAIR_VALID, T_SYNCING})
            TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, tr(t)) <= 228, tr(t));
        TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, (std::string(tr(T_CONNECTING)) + "99").c_str()) <= 228, "connecting");
        TEST_ASSERT_TRUE_MESSAGE(advance(F_S, tr(T_PAIR_ENTER)) <= 228, tr(T_PAIR_ENTER));
        for (Txt t : {T_PAIR_TITLE, T_SETUP_TITLE}) TEST_ASSERT_TRUE_MESSAGE(advance(F_TOP, tr(t)) <= 228, tr(t));
        int cardW = lay::CR - lay::CL - 4;  // silent / offline card: text at x+10, minutes right-aligned at x+w-8
        for (Txt t : {T_SILENT, T_NO_WIFI})
            TEST_ASSERT_TRUE_MESSAGE(10 + advance(F_S, tr(t)) + 6 + advance(F_LIM, "999′") + 8 <= cardW, tr(t));
        // top row on work screens: "HH:MM  date" + up to 6 session dots and "+N"
        int top = advance(F_TOP, ("23:59  " + dateShort(3, 30, 9)).c_str()) + 6 + 6 * lay::DOT_STEP + 4 + advance(F_TOP, "+9");
        TEST_ASSERT_TRUE_MESSAGE(top <= lay::TOP_R - lay::TOP_L, "top row");
        // holiday caption card: caption + date (drawCaption shortens with "…", but the full text should fit)
        for (int i = 0; i < sceneCount(); i++) {
            Scene sc = sceneAt(i);
            if (sc.kind != SceneKind::Holiday || !sc.caption || !*sc.caption) continue;
            // drawCaption drops the date when caption + date do not fit; the caption alone must fit
            TEST_ASSERT_TRUE_MESSAGE(advance(F_S, sceneCaption(sc.name, sc.caption)) <= cardW - 20, sc.name);
        }
        // info card: icon, then line 1 (F_S) and line 2 (F_S3) between INFO_TEXT_DX and INFO_PAD_R
        int lineW = lay::INFO_W - lay::INFO_TEXT_DX - lay::INFO_PAD_R;
        std::string l1[] = {weatherText(WeatherNow{202, -28, 1, 0, false}), weatherText(WeatherNow{801, -28, 1, 0, false}),
                            weatherText(WeatherNow{301, -28, 1, 0, false}), weatherText(WeatherNow{741, -28, 1, 0, false}),
                            std::string(tr(T_TODAY)) + " " + resetShort(23 * 3600 + 59 * 60, false),
                            std::string(tr(T_ALERT)) + " " + resetShort(35999, false),
                            std::string(tr(T_ALERT_MISSILE)) + " 9:59", std::string(tr(T_ALERT_DRONE)) + " 9:59"};
        {
            Info mi;
            Meeting m{1000000, 1000000 + 3600 * 9, "", "23:59"};
            mi.onMeetings(&m, 1);
            std::string meet[] = {mi.card(0, 0, 1000000 + 60).line1, mi.card(0, 0, 1000000 - 3540).line1,
                                  mi.card(0, 0, 1000000 - 7200).line1, mi.card(0, 0, 1000000 + 60).line2};
            for (const std::string &w : meet) TEST_ASSERT_TRUE_MESSAGE(advance(F_S, w.c_str()) <= lineW, w.c_str());
        }
        for (const std::string &w : l1) TEST_ASSERT_TRUE_MESSAGE(advance(F_S, w.c_str()) <= lineW, w.c_str());
        Info in;
        for (int i = 0; i < 288; i++) in.onAnswer(1);
        for (int i = 0; i < 88; i++) in.onCommit(1);
        std::string l2[] = {weatherDetail(WeatherNow{800, -28, 25, -38, false}), in.card(0, 1).line2};
        for (const std::string &w : l2) TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, w.c_str()) <= lineW, w.c_str());
        for (std::string w : {resetShort(4 * 3600 + 59 * 60, false), resetShort(59 * 60, true), resetShort(23 * 3600, true)})
            TEST_ASSERT_TRUE_MESSAGE(advance(F_S3, w.c_str()) <= 2 * lay::TIMER_HALF, w.c_str());
    }
    setLang(Lang::En);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_clock_screen);
    RUN_TEST(test_sleep_screen);
    RUN_TEST(test_gauge_label_chip_and_rows);
    RUN_TEST(test_idle_mascot_stays_inside_stage_also_with_hat);
    RUN_TEST(test_work_screens);
    RUN_TEST(test_setup_qr_page);
    RUN_TEST(test_pairing_screen_texts_fit);
    RUN_TEST(test_setup_result_texts_fit);
    RUN_TEST(test_work_top_bar_has_time_and_date);
    RUN_TEST(test_texts_fit_in_both_languages);
    return UNITY_END();
}
