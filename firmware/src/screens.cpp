#include "screens.h"
#include <ESP8266WiFi.h>
#include "device.h"
#include "i18n.h"
#include "config.h"
#include "fonts.h"
#include "panel.h"
#include "render.h"
#include "text.h"
#include "idle.h"
#include "fetch.h"
#include "qrcode.h"
#include "wifisetup.h"
#include "scenetitles.h"
#include "device.h"

extern bool safeBoot;
static TFT_eSprite stage(&tft);  // 4-bit palette stage between the gauges for the idle mascot
// The work screens' big mascot shares the stage buffer (never on screen together, both fully redrawn before each
// push): saves 5.5 KB of heap. Only its top HERO_H rows are pushed.
static TFT_eSprite &hero = stage;
static_assert(lay::HERO_W == lay::CR - lay::CL && lay::HERO_H <= lay::STAGE_H, "hero must fit the stage buffer");
static void pushHero() { hero.pushSprite(lay::HERO_X, lay::HERO_Y, 0, 0, lay::HERO_W, lay::HERO_H); }
static void pushStageScaled();  // clock stage at 1.2x
static IdleDirector idle;
static uint32_t lastIdleTick = UINT32_MAX;
static uint32_t nyPreviewUntil = 0;
static int demoNext = -1, demoOnly = -1;  // scene preview: next index for "all", or a single scene

bool screensPreviewScene(const char *name) {
    if (name) {
        demoOnly = sceneIndexByName(name);
        if (demoOnly < 0 || sceneAt(demoOnly).kind == SceneKind::Work) { demoOnly = -1; return false; }
        demoNext = -1;
        return true;
    }
    demoOnly = -1;
    demoNext = 0;
    return true;
}
static bool nyPreviewMidnight = false;

void screensPreviewNewYear(uint32_t forMs, bool midnight) {
    nyPreviewUntil = (millis() + forMs) | 1;
    nyPreviewMidnight = midnight;
}

static Screen lastScreen = Screen::Boot;
static bool firstDraw = true;
static uint8_t lastBright = 255;
static int lastMinuteKey = -1000;
static String lastTopL, lastTopR, lastGauge;
static String lineCache[3];  // verb / target / detail

static const int SMALL_W = 14 * lay::SMALL_PX;

// Stage: x CL..CR, y STAGE_Y..STAGE_Y+STAGE_H; mascot rests at (SPR_X, SPR_Y) inside it (screen y = SMALL_Y).
static const int STAGE_Y = lay::STAGE_Y, STAGE_H = lay::STAGE_H, SPR_X = (lay::CR - lay::CL - SMALL_W) / 2, SPR_Y = lay::SMALL_Y - STAGE_Y;
enum : uint8_t { P_BG, P_BODY, P_EYE, P_LID, P_HAT_RED, P_HAT_WHITE, P_BAND, P_APPLE, P_LEAF, P_SWEAT, P_STAR,
                 P_Z, P_CRUMB, P_GREY, P_SLEEP_INK };
static uint16_t PALETTE[16] = {col::BG,      col::CLAY, col::EYE,   col::LID,   col::HAT_RED, col::HAT_WHITE,
                               col::INK,     col::RED,  col::GREEN, col::BLUE,  col::AMBER,   col::MUTED,
                               col::DIM,     col::GREY_BODY, col::SLEEP_INK, col::GREEN};  // 15: season green

void screensBegin() {
    stage.setColorDepth(4);
    stage.createSprite(lay::CR - lay::CL, STAGE_H);
    stage.createPalette(PALETTE, 16);
    idle = IdleDirector(ESP.random());
}

// ---------- formatting ----------
static String mmss(uint32_t ms) {
    uint32_t s = ms / 1000;
    char b[16];
    if (s >= 3600) snprintf(b, sizeof b, "%lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    else snprintf(b, sizeof b, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    return b;
}
static String hhmm(const LocalTime &lt) {
    if (!lt.synced) return "--:--";
    char b[6];
    snprintf(b, sizeof b, "%02d:%02d", lt.hour, lt.min);
    return b;
}
static String dateShort(const LocalTime &lt) { return String(::dateShort(lt.wday, lt.day, lt.month).c_str()); }  // "THU 26 NOV" / "ЧТ 26.11"
static String diag(const NetInfo &net) {
    if (net.connected) return "NTP…";
    return String("WIFI ") + net.wifiStatus + " #" + net.attempts;
}
static String S(const std::string &s) { return String(s.c_str()); }

// ---------- static parts ----------
static bool isWorkScreen(Screen s) {
    return s == Screen::Working || s == Screen::Permission || s == Screen::YourTurn || s == Screen::Error ||
           s == Screen::Done || s == Screen::Hello;
}

static void topBar(const Model &m, const View &v, const LocalTime &lt, const NetInfo &net) {
    bool dateLeft = v.screen == Screen::Clock || v.screen == Screen::LimitReached || v.screen == Screen::MacSilent;
    // big time on the clock screens -> date only; work screens have no big time -> time and date
    String left = !lt.synced ? diag(net) : (dateLeft ? dateShort(lt) : hhmm(lt) + "  " + dateShort(lt));
    uint16_t rc = col::INK;
    int total = int(m.count()), busy = int(m.busyCount());
    if (isWorkScreen(v.screen) || v.screen == Screen::Multi) {
        if (v.screen == Screen::Permission || v.screen == Screen::YourTurn) rc = col::AMBER;
        else if (v.screen == Screen::Error) rc = col::RED;
        else if (v.screen == Screen::Done) rc = col::GREEN;
        else rc = col::CLAY;
    } else if (v.screen != Screen::Clock) {
        total = busy = 0;  // limit / offline screens: no session dots
    }
    // after the date: red / amber "!" during an alert, else blue minutes to a meeting starting within 10 min
    AlertLevel al = infoState().alert(millis());
    uint16_t mark = al == AlertLevel::None ? 0 : al == AlertLevel::Drone ? col::AMBER : al == AlertLevel::Threat ? col::YELLOW
                  : al == AlertLevel::Partial ? col::PINK : col::RED;
    String markText = "!";
    int soon = lt.synced ? infoState().meetingSoonMin(uint32_t(time(nullptr))) : -1;
    if (!mark && soon >= 0) {
        mark = col::BLUE;
        markText = resetShort(long(soon) * 60, false).c_str();
    }
    if (!mark && (safeBoot || infoState().alertsUnknown(millis()))) {  // grey: safe mode, or alerts can't be trusted
        mark = col::DIM;
        markText = safeBoot ? "safe" : "?";
    }
    String key = left + "|" + busy + "/" + total + "|" + String(rc) + "|" + mark + markText;
    if (key == lastTopL) return;
    lastTopL = key;
    drawTopBar(left, busy, total, rc, mark, markText);
}

static int shownA = -1, shownB = -1;  // bar heights on screen while they ease to the new %
static uint32_t lastAnim = 0;

static void gauges(const Model &m, uint32_t now, time_t epoch) {
    bool stale = m.limitsStale(now);
    int a = m.hasLimits() ? m.limit5h().pct : 0, b = m.hasLimits() ? m.limit7d().pct : 0;
    // time to reset and the pace mark under/on both bars (with fresh limits and a known clock)
    bool clock = epoch > 1600000000 && !stale;
    std::string ta, tb;
    int pa = -1, pb = -1;
    if (clock && m.limit5h().resetsAt) {
        ta = resetShort(long(m.limit5h().resetsAt) - long(epoch), false);
        pa = pacePercent(m.limit5h().resetsAt, uint32_t(epoch), 5 * 3600);
    }
    if (clock && m.limit7d().resetsAt) {
        tb = resetShort(long(m.limit7d().resetsAt) - long(epoch), true);
        pb = pacePercent(m.limit7d().resetsAt, uint32_t(epoch), 7 * 86400);
    }
    String key = String(a) + "|" + b + "|" + stale + "|" + ta.c_str() + pa + "|" + tb.c_str() + pb;
    if (key != lastGauge) {
        lastGauge = key;
        if (shownA < 0) shownA = a;
        if (shownB < 0) shownB = b;
        drawGauge(13, "5h", a, shownA, a >= 90, stale, ta.c_str(), pa);
        drawGauge(227, "7d", b, shownB, b >= 90, stale, tb.c_str(), pb);
    }
    if ((shownA != a || shownB != b) && now - lastAnim >= 40) {  // ease the bars, ~0.5 s
        lastAnim = now;
        if (shownA != a) { shownA = gaugeStep(shownA, a); drawGaugeBar(13, shownA, a >= 90, stale, pa, false); }
        if (shownB != b) { shownB = gaugeStep(shownB, b); drawGaugeBar(227, shownB, b >= 90, stale, pb, false); }
    }
}

// Shorten s (UTF-8 safe, with "…") until it fits maxW pixels in font. Character counts are not
// enough: 22 mono chars + quotes are wider than the 184 px column and spill over the gauges.
static String fitText(const uint8_t *font, const String &s, int maxW) {
    if (textWidth(font, s) <= maxW) return s;
    std::string src(s.c_str());
    for (size_t n = 40; n > 1; n--) {
        String t = String(utf8Truncate(src, n).c_str());
        if (t.length() < s.length() && textWidth(font, t) <= maxW) return t;
    }
    return "";
}

// One cached text line of the work template (slot 0..2). Redraws only when the text changes.
static void line(int slot, int y, int h, const uint8_t *font, const String &s, uint16_t c) {
    String key = s + "|" + String(c);
    if (lineCache[slot] == key) return;
    lineCache[slot] = key;
    tft.fillRect(lay::CL, y - 2, lay::CR - lay::CL, h, col::BG);  // exactly the column: the gauge chips start right next to it
    if (s.length()) text(font, fitText(font, s, lay::CR - lay::CL - 4), lay::CX, y, c, col::BG, TC_DATUM);
}

static void workLines(const String &verb, uint16_t vc, const String &target, const String &detail) {
    line(0, lay::VERB_Y, 32, F_H, verb, vc);
    line(1, lay::TARGET_Y, 22, F_MONO, target, col::INK);
    line(2, lay::DETAIL_Y, 20, F_S3, detail, col::INK);
}

// Context window of the focused session under the work lines: "context 72%" + a thin bar; amber from 80 %,
// red "compact soon" from 90 %. Kept clear of the reset timers next to the gauges (x 34..206).
static const uint32_t PERMISSION_PULSE_MS = 5u * 60000u;
static String ctxShown;
static void contextBar(const ClaudeSession *f) {
    int pct = f ? f->ctx : -1;
    String key = String(pct) + int(lang());
    if (key == ctxShown) return;
    ctxShown = key;
    static const int X0 = 34, X1 = 206, BASE = 212, BAR_Y = 218, BAR_X = 50, BAR_W = 140;
    tft.fillRect(X0, 198, X1 - X0, 26, col::BG);
    if (pct < 0) return;
    int lv = contextLevel(pct);
    uint16_t c = lv == 2 ? col::RED : lv == 1 ? col::AMBER : col::MUTED;
    String label = String(tr(lv == 2 ? T_COMPACT_SOON : T_CONTEXT)) + " " + pct + "%";
    text(F_S3, label, lay::CX, BASE, lv ? c : col::MUTED, col::BG, C_BASELINE);
    tft.fillRect(BAR_X, BAR_Y, BAR_W, 4, col::TRACK);
    tft.fillRect(BAR_X, BAR_Y, BAR_W * pct / 100, 4, lv ? c : col::CLAY);
}

static const ClaudeSession *newest(const Model &m) {
    const ClaudeSession *n = nullptr;
    for (const auto &s : m.sessions())
        if (!n || s.lastMs > n->lastMs) n = &s;
    return n;
}

static void drawWorkBody(const Model &m, const View &v, uint32_t now) {
    const ClaudeSession *f = v.focus;
    switch (v.screen) {
        case Screen::Working: {
            String verb = f->state == SState::Thinking ? String(tr(T_V_THINK))
                        : f->state == SState::Compacting ? String(tr(T_V_COMPACT))
                        : f->agents > 0 ? (String(f->agents) + tr(f->agents == 1 ? T_AGENT1 : T_AGENTS))
                        : String(verbFor(f->tool));
            String target = f->state == SState::Thinking ? (f->target.empty() ? String("") : String(tr(T_Q_OPEN)) + S(utf8Truncate(f->target, 20)) + tr(T_Q_CLOSE))
                          : f->state == SState::Compacting ? String(tr(T_CONTEXT))
                          : S(utf8Truncate(f->target, 22));
            workLines(verb, col::CLAY, target, S(utf8Truncate(f->project, 14)) + " · " + mmss(now - f->sinceMs));
            break;
        }
        case Screen::Permission: {
            // forgotten for 5+ min: the title pulses amber / white to catch the eye from across the room
            bool pulse = now - f->sinceMs >= PERMISSION_PULSE_MS && (now / 700) % 2;
            workLines(tr(T_V_ALLOW), pulse ? col::INK : col::AMBER, S(utf8Truncate(f->target, 22)),
                      S(f->tool.empty() ? f->project : f->tool) + " · " + tr(T_WAITING) + mmss(now - f->sinceMs));
            break;
        }
        case Screen::YourTurn:
            workLines(tr(T_V_YOURTURN), col::AMBER, S(utf8Truncate(f->project, 22)), String(tr(T_WAITING)) + mmss(now - f->sinceMs));
            break;
        case Screen::Error:
            workLines(tr(T_V_ERROR), col::PINK, S(utf8Truncate(f->target, 22)), S(utf8Truncate(f->project, 22)));
            break;
        case Screen::Done: {
            const ClaudeSession *n = newest(m);
            workLines(tr(T_DONE), col::GREEN, n ? S(utf8Truncate(n->project, 22)) : String(""),
                      String(tr(T_IN)) + mmss(m.lastDoneDurationMs()));
            break;
        }
        case Screen::Hello: {
            const ClaudeSession *n = newest(m);
            workLines(tr(T_V_HELLO), col::INK, n ? S(utf8Truncate(n->project, 22)) : String(""),
                      n ? S(utf8Truncate(n->branch, 22)) : String(""));
            break;
        }
        default: break;
    }
}

static void drawMulti(const Model &m) {
    // Order: permission -> working -> other; max 3 rows.
    const ClaudeSession *rows[3] = {nullptr, nullptr, nullptr};
    int n = 0;
    for (int pass = 0; pass < 3 && n < 3; pass++)
        for (const auto &s : m.sessions()) {
            bool p0 = s.state == SState::Permission, p1 = !p0 && isWorking(s), p2 = !p0 && !p1;
            if ((pass == 0 && p0) || (pass == 1 && p1) || (pass == 2 && p2))
                if (n < 3) rows[n++] = &s;
        }
    String key;
    for (int i = 0; i < n; i++) key += S(rows[i]->id) + int(rows[i]->state) + ";";
    if (key == lineCache[0]) return;
    lineCache[0] = key;
    tft.fillRect(lay::CL, 30, lay::CR - lay::CL, 170, col::BG);
    for (int i = 0; i < n; i++) {
        const ClaudeSession *s = rows[i];
        int y = 36 + i * 54;
        bool perm = s->state == SState::Permission, work = isWorking(*s);
        uint16_t bg = perm ? col::AMBER_BG : col::CARD;
        uint16_t dot = perm ? col::AMBER : work ? col::CLAY : col::DIM;
        tft.fillSmoothRoundRect(lay::CL, y, lay::CR - lay::CL, 46, 7, bg, col::BG);
        tft.fillSmoothCircle(lay::CL + 13, y + 23, 4, dot, bg);
        text(F_S, S(utf8Truncate(s->project.empty() ? s->id : s->project, 15)), lay::CL + 24, y + 23,
             work || perm ? col::INK : col::DIM, bg, ML_DATUM);
        if (perm) drawBang(tft, lay::CR - 12, y + 18, col::AMBER);
        else if (!work) text(F_S, "—", lay::CR - 8, y + 23, col::DIM, bg, MR_DATUM);
    }
}

// Clock faces (settings "face"): 0 classic (JBMono 60 px), 1 minimal (48 px, blinking colon), 2 pixel (5x7 LED digits).
static const uint8_t PIX_DIGITS[10][7] PROGMEM = {  // 5 bits per row, MSB = left
    {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},   {14, 17, 1, 2, 4, 8, 31},  {31, 2, 4, 2, 1, 17, 14},
    {2, 6, 10, 18, 31, 2, 2},     {31, 16, 30, 1, 1, 17, 14}, {6, 8, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 2, 12}};
static const int PIX = 6, PIX_DOT = 5, PIX_GAP = 6, PIX_COLON = 12;  // LED dot 5 px in a 6 px grid
static const int PIX_W = 4 * 5 * PIX + 3 * PIX_GAP + PIX_COLON;    // 150 px: fits the 184 px column
static_assert(PIX_W <= lay::CR - lay::CL && 7 * PIX <= lay::STAGE_Y - lay::TIME_CLEAR_TOP, "pixel face fits");

static void pixelDigit(int d, int x, int y, uint16_t c) {
    for (int r = 0; r < 7; r++) {
        uint8_t bits = pgm_read_byte(&PIX_DIGITS[d][r]);
        for (int k = 0; k < 5; k++)
            if (bits & (16 >> k)) tft.fillRect(x + k * PIX, y + r * PIX, PIX_DOT, PIX_DOT, c);
    }
}

static void pixelTime(const LocalTime &lt, int y) {
    int x = lay::CX - PIX_W / 2;
    int dg[4] = {lt.hour / 10, lt.hour % 10, lt.min / 10, lt.min % 10};
    for (int i = 0; i < 4; i++) {
        pixelDigit(dg[i], x, y, col::CLAY);
        x += 5 * PIX + (i == 1 ? 0 : PIX_GAP);
        if (i == 1) {  // colon: two dots in the middle of a 12 px slot
            tft.fillRect(x + 3, y + 2 * PIX, PIX_DOT, PIX_DOT, col::CLAY);
            tft.fillRect(x + 3, y + 4 * PIX, PIX_DOT, PIX_DOT, col::CLAY);
            x += PIX_COLON;
        }
    }
}

// Minimal face: "HH" and "MM" right / left of a colon that blinks every second (drawn alone between minutes).
static void minimalColon(uint32_t now, bool force) {
    static int shown = -1;
    int on = (now / 1000) % 2;
    if (on == shown && !force) return;
    shown = on;
    int cw = textWidth(F_BIG, ":");
    tft.fillRect(lay::CX - cw / 2, lay::TIME_Y + 14, cw, lay::STAGE_Y - lay::TIME_Y - 14, col::BG);  // the whole glyph
    if (on) text(F_BIG, ":", lay::CX, lay::TIME_Y + 14, col::MUTED, col::BG, TC_DATUM, false);
}

// 3 segment: LCD digits, unlit segments faintly visible.
static void segDigit(int d, int x, int y, uint16_t on, uint16_t off) {
    static const uint8_t M[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};  // bits a..g
    const int w = 30, h = 46, t = 6;
    const int R[7][4] = {{x + t, y, w - 2 * t, t},         {x + w - t, y + t, t, h / 2 - t}, {x + w - t, y + h / 2, t, h / 2 - t},
                         {x + t, y + h - t, w - 2 * t, t}, {x, y + h / 2, t, h / 2 - t},     {x, y + t, t, h / 2 - t},
                         {x + t, y + h / 2 - t / 2, w - 2 * t, t}};
    for (int k = 0; k < 7; k++) tft.fillRect(R[k][0], R[k][1], R[k][2], R[k][3], (M[d] >> k & 1) ? on : off);
}

static void segmentTime(const LocalTime &lt) {
    const uint16_t on = col::AMBER, off = rgb565(0x1C, 0x14, 0x08);
    int dg[4] = {lt.hour / 10, lt.hour % 10, lt.min / 10, lt.min % 10}, x = lay::CX - 82;
    for (int i = 0; i < 4; i++) { segDigit(dg[i], x, 30, on, off); x += 36 + (i == 1 ? 14 : 0); }
    tft.fillRect(lay::CX - 3, 42, 6, 6, on);
    tft.fillRect(lay::CX - 3, 60, 6, 6, on);
}

// 4 flip: two dark cards with a split line, like a desk flip clock.
static void flipTime(const LocalTime &lt) {
    char b[3];
    for (int i = 0; i < 2; i++) {
        int cx = i ? lay::CX + 42 : lay::CX - 42;
        tft.fillSmoothRoundRect(cx - 40, 28, 80, 48, 8, col::TRACK, col::BG);
        snprintf(b, sizeof b, "%02d", i ? lt.min : lt.hour);
        text(F_BIG, b, cx, 52, col::INK, col::TRACK, MC_DATUM, false);
        tft.fillRect(cx - 40, 51, 80, 2, col::BG);
    }
}

// 6 binary: BCD dots (hour tens 2, hour ones 4, minute tens 3, minute ones 4), the time small underneath.
static void binaryTime(const LocalTime &lt) {
    const int v[4] = {lt.hour / 10, lt.hour % 10, lt.min / 10, lt.min % 10}, bits[4] = {2, 4, 3, 4};
    for (int i = 0; i < 4; i++) {
        int cx = 60 + i * 34 + (i > 1 ? 12 : 0);
        for (int b = 0; b < bits[i]; b++)
            tft.fillSmoothCircle(cx, 66 - b * 11, 4, (v[i] >> b & 1) ? col::CLAY : rgb565(0x2A, 0x20, 0x18), col::BG);
    }
    text(F_S3, hhmm(lt), lay::CX, 78, col::MUTED, col::BG, BC_DATUM, false);
}

// 1 minimal / 5 terminal: something blinks every second between minute redraws (colon / cursor).
static void terminalCursor(uint32_t now, bool force) {
    static int shown = -1;
    int on = (now / 1000) % 2;
    if (on == shown && !force) return;
    shown = on;
    int x = 34 + textWidth(F_BIG, "00:00") + 4;
    tft.fillRect(x, 44, 16, 30, on ? col::CLAY : col::BG);
}

static void terminalTime(const LocalTime &lt, uint32_t now) {
    text(F_LIM, "$ date +%H:%M", 34, 40, col::MUTED, col::BG, BL_DATUM, false);
    text(F_BIG, hhmm(lt), 34, 76, col::INK, col::BG, BL_DATUM, false);
    terminalCursor(now, true);
}

static void drawClockTime(const LocalTime &lt, uint32_t now, bool sleep) {
    int key = lt.synced ? lt.hour * 60 + lt.min : -int(now / 5000);  // unsynced: refresh diag every 5 s
    int face = lt.synced && !sleep ? deviceFace() : 0;
    key = key * 8 + face;  // a new face redraws at once
    if (key == lastMinuteKey) {
        if (face == 1) minimalColon(now, false);
        if (face == 5) terminalCursor(now, false);
        return;
    }
    lastMinuteKey = key;
    if (sleep) {
        tft.fillRect(lay::CL - 2, lay::SLEEP_CLEAR_TOP, lay::CR - lay::CL + 4, lay::SLEEP_CLEAR_BOTTOM - lay::SLEEP_CLEAR_TOP, TFT_BLACK);
        text(F_TIME_S, hhmm(lt), lay::CX, lay::SLEEP_TIME_Y, col::SLEEP_INK, TFT_BLACK, TC_DATUM, false);
        if (lt.synced) text(F_TOP, dateShort(lt), lay::CX, lay::SLEEP_DATE_Y, col::SLEEP_INK, TFT_BLACK, TC_DATUM, false);
    } else {
        tft.fillRect(lay::CL, lay::TIME_CLEAR_TOP, lay::CR - lay::CL, lay::STAGE_Y - lay::TIME_CLEAR_TOP, col::BG);
        if (face == 2) {
            pixelTime(lt, lay::TIME_CLEAR_TOP + 6);
        } else if (face == 3) {
            segmentTime(lt);
        } else if (face == 4) {
            flipTime(lt);
        } else if (face == 5) {
            terminalTime(lt, now);
        } else if (face == 6) {
            binaryTime(lt);
        } else if (face == 1) {
            char hh[3], mm[3];
            snprintf(hh, sizeof hh, "%02d", lt.hour);
            snprintf(mm, sizeof mm, "%02d", lt.min);
            int half = textWidth(F_BIG, ":") / 2 + 2;
            text(F_BIG, hh, lay::CX - half, lay::TIME_Y + 14, col::INK, col::BG, TR_DATUM, false);
            text(F_BIG, mm, lay::CX + half, lay::TIME_Y + 14, col::INK, col::BG, TL_DATUM, false);
            minimalColon(now, true);
        } else {
            text(F_TIME_S, hhmm(lt), lay::CX, lay::TIME_Y, col::INK, col::BG, TC_DATUM, false);
        }
    }
}

static void drawLimitReached(const Model &m, time_t epoch, const LocalTime &lt) {
    const Limit &l = m.limitShown();
    long left = (long)l.resetsAt - (long)epoch;
    if (left < 0) left = 0;
    char b[12];
    snprintf(b, sizeof b, "%ld:%02ld", left / 3600, left / 60 % 60);
    line(0, 80, 32, F_H, tr(T_LIMIT), col::PINK);
    line(1, 112, 56, F_BIG, b, col::INK);
    time_t r = l.resetsAt;
    struct tm tmv;
    localtime_r(&r, &tmv);
    char rb[48];
    snprintf(rb, sizeof rb, "%s%02d:%02d", tr(T_RESETS_AT), tmv.tm_hour, tmv.tm_min);
    line(2, lay::DETAIL_Y, 20, F_S3, l.resetsAt ? String(rb) : String(""), col::INK);
    (void)lt;
}

static bool offlineNow = false;
static uint32_t offlineSinceMs = 0;

static void drawSilentCard(const Model &m, uint32_t now) {
    String mins = String((now - (offlineNow ? offlineSinceMs : m.lastEventMs())) / 60000) + "′";
    String key = String(offlineNow ? "off" : "silent") + mins;
    if (lineCache[1] == key) return;
    lineCache[1] = key;
    int x = lay::CL + 2, w = lay::CR - lay::CL - 4;
    tft.fillSmoothRoundRect(x, lay::CARD_Y, w, lay::CARD_H, 7, col::CARD, col::BG);
    text(F_S, offlineNow ? tr(T_NO_WIFI) : tr(T_SILENT), x + 10, lay::CARD_Y + 17, offlineNow ? col::AMBER : col::INK,
         col::CARD, ML_DATUM);
    text(F_LIM, mins, x + w - 8, lay::CARD_Y + 17, col::MUTED, col::CARD, MR_DATUM);
}

// ---------- boot / connecting ----------
// Steps: Wi-Fi, then time (NTP). Icon: dim ring = waiting, pulsing dot = in progress, green tick = done.
static void stepIcon(int cx, int cy, int state, uint32_t tick) {
    tft.fillRect(cx - 8, cy - 8, 17, 17, col::BG);
    if (state == 0) { tft.drawSmoothCircle(cx, cy, 6, col::DIM, col::BG); return; }
    if (state == 1) { tft.fillSmoothCircle(cx, cy, (tick & 1) ? 6 : 4, (tick & 1) ? col::AMBER : col::CLAY, col::BG); return; }
    tft.fillSmoothCircle(cx, cy, 7, col::GREEN, col::BG);
    tft.drawWideLine(cx - 3, cy, cx - 1, cy + 3, 2, col::BG, col::GREEN);
    tft.drawWideLine(cx - 1, cy + 3, cx + 4, cy - 3, 2, col::BG, col::GREEN);
}

static void animHero(const SpriteFrame &f, int dx, int dy, int accessory, uint32_t tick, bool hat);

static void drawBoot(const NetInfo &net, bool frame, uint32_t tick) {
    static const int X0 = 44, X1 = 196, Y_WIFI = 136, Y_TIME = 164;
    if (lineCache[0].length() == 0) {  // static parts, once per screen entry
        lineCache[0] = "boot";
        text(F_TOP, "CLAUDE CLOCK", lay::CX, lay::TOP_BASE, col::MUTED, col::BG, C_BASELINE);
        text(F_S, "Wi-Fi", X0 + 14, Y_WIFI, col::INK, col::BG, ML_DATUM);
        text(F_S, tr(T_BOOT_TIME), X0 + 14, Y_TIME, col::INK, col::BG, ML_DATUM);
        text(F_S3, fitText(F_S3, net.ssid, 90), X1, Y_WIFI, col::MUTED, col::BG, MR_DATUM);
        text(F_S3, "NTP", X1, Y_TIME, col::MUTED, col::BG, MR_DATUM);
        text(F_S3, "v" FW_VERSION, lay::CX, lay::BOTTOM_BASE, col::DIM, col::BG, C_BASELINE);
    }
    String detail = net.connected ? String(tr(T_SYNCING))
                  : String(tr(T_CONNECTING)) + net.attempts;
    if (lineCache[2] != detail) {
        lineCache[2] = detail;
        tft.fillRect(0, 186, 240, 24, col::BG);
        text(F_S3, detail, lay::CX, 198, col::MUTED, col::BG, MC_DATUM);
    }
    if (!frame) return;
    stepIcon(X0, Y_WIFI, net.connected ? 2 : 1, tick);
    stepIcon(X0, Y_TIME, net.connected ? 1 : 0, tick);
    bool odd = tick & 1;
    if (net.connected) animHero(tick % 7 == 0 ? SPR_BLINK : SPR_UP, 0, 3, 1, tick, false);  // looks up: waiting for time
    else animHero(odd ? SPR_WALK_A : SPR_WALK_B, 0, odd ? 3 : 0, 1, tick, false);           // busy connecting
}

// ---------- Wi-Fi setup mode (AP + captive portal) ----------
static void drawQr(const String &payload, int top) {
    static const int VER = 3, MOD = 5, QUIET = 2;  // version 3 = 29x29 (53 bytes at ECC_LOW), 165 px with quiet zone
    uint8_t buf[qrcode_getBufferSize(VER)];
    QRCode qr;
    if (qrcode_initText(&qr, buf, VER, ECC_LOW, payload.c_str()) != 0) return;
    int side = (qr.size + 2 * QUIET) * MOD, x0 = lay::CX - side / 2;
    tft.fillRect(x0, top, side, side, TFT_WHITE);  // QR needs a light background and quiet zone
    for (int y = 0; y < qr.size; y++)
        for (int x = 0; x < qr.size; x++)
            if (qrcode_getModule(&qr, x, y))
                tft.fillRect(x0 + (QUIET + x) * MOD, top + (QUIET + y) * MOD, MOD, MOD, TFT_BLACK);
}

// Pages: QR (join the AP) <-> steps, every 8 s; checking / result pages while the phone sends a network.
static void drawSetup(const NetInfo &net, uint32_t now, bool frame, uint32_t tick) {
    int page = net.test == TEST_RUNNING ? 2 : net.test == TEST_FAIL ? 3
             : (net.test == TEST_OK || net.test == TEST_OK_UNSAVED) ? 4 : int((now / 8000) % 2);
    String key = String(page) + net.testSsid;
    if (lineCache[0] != key) {
        lineCache[0] = key;
        tft.fillScreen(col::BG);
        text(F_TOP, tr(T_SETUP_TITLE), lay::CX, lay::TOP_BASE, col::MUTED, col::BG, C_BASELINE);
        String ap = String(net.apSsid) + " · " + net.apPass;
        switch (page) {
            case 0:
                drawQr(wifiQr(net.apSsid, net.apPass).c_str(), lay::QR_TOP);
                text(F_S, ap, lay::CX, lay::QR_AP_Y, col::INK, col::BG, MC_DATUM);
                text(F_S3, tr(T_SCAN_HINT), lay::CX, lay::BOTTOM_BASE, col::MUTED, col::BG, C_BASELINE);
                break;
            case 1:
                text(F_S3, fitText(F_S3, net.connected ? String(tr(T_CHANGE_WIFI))
                                       : net.ssid[0] ? String(tr(T_NO_LINK)) + tr(T_Q_OPEN) + net.ssid + tr(T_Q_CLOSE) : String(tr(T_FIRST_SETUP)), 200),
                     lay::CX, 46, col::AMBER, col::BG, MC_DATUM);
                text(F_S, tr(T_STEP1), 24, 84, col::INK, col::BG, ML_DATUM);
                text(F_S, tr(T_STEP2), 24, 114, col::INK, col::BG, ML_DATUM);
                text(F_S, tr(T_STEP3), 24, 144, col::INK, col::BG, ML_DATUM);
                text(F_S, tr(T_STEP3B), 24, 166, col::INK, col::BG, ML_DATUM);
                text(F_S3, ap, lay::CX, 202, col::MUTED, col::BG, MC_DATUM);
                text(F_S3, tr(T_OR_OPEN), lay::CX, lay::BOTTOM_BASE, col::MUTED, col::BG, C_BASELINE);
                break;
            case 2:
                text(F_S, tr(T_CHECKING), lay::CX, 140, col::INK, col::BG, MC_DATUM);
                text(F_S, fitText(F_S, String(tr(T_Q_OPEN)) + net.testSsid + tr(T_Q_CLOSE), 200), lay::CX, 164, col::MUTED, col::BG, MC_DATUM);
                break;
            case 3: {
                static const Txt WHY[] = {T_WHY_FAIL, T_WHY_PASS, T_WHY_NOSSID, T_WHY_FAIL, T_WHY_NOIP};
                text(F_H, tr(WHY[net.testReason < 5 ? net.testReason : 0]), lay::CX, 120, col::PINK, col::BG, MC_DATUM);
                if (net.testReason == 4) text(F_S3, tr(T_DHCP_HINT), lay::CX, 176, col::INK, col::BG, MC_DATUM);
                text(F_S, fitText(F_S, String(tr(T_Q_OPEN)) + net.testSsid + tr(T_Q_CLOSE), 200), lay::CX, 152, col::MUTED, col::BG, MC_DATUM);
                text(F_S3, tr(T_TRY_AGAIN), lay::CX, 196, col::INK, col::BG, MC_DATUM);
                text(F_S3, ap, lay::CX, lay::BOTTOM_BASE, col::MUTED, col::BG, C_BASELINE);
                break;
            }
            default:
                tft.fillSmoothCircle(lay::CX, 90, 26, col::GREEN, col::BG);
                tft.drawWideLine(lay::CX - 12, 90, lay::CX - 3, 100, 5, col::BG, col::GREEN);
                tft.drawWideLine(lay::CX - 3, 100, lay::CX + 13, 80, 5, col::BG, col::GREEN);
                text(F_H, tr(T_CONNECTED), lay::CX, 146, col::INK, col::BG, MC_DATUM);
                text(F_S, fitText(F_S, String(tr(T_Q_OPEN)) + net.testSsid + tr(T_Q_CLOSE), 200), lay::CX, 176, col::MUTED, col::BG, MC_DATUM);
                if (net.test == TEST_OK_UNSAVED) text(F_S3, tr(T_NOT_SAVED), lay::CX, 206, col::AMBER, col::BG, MC_DATUM);
                break;
        }
    }
    if (page == 2 && frame) animHero((tick & 1) ? SPR_WALK_A : SPR_WALK_B, 0, (tick & 1) ? 3 : 0, 1, tick, false);
}

// ---------- pairing a computer (code requested by `claude-clock setup`) ----------
static void drawPair(uint32_t now) {
    String code = devicePairCode();
    if (lineCache[0] == code) return;
    lineCache[0] = code;
    tft.fillScreen(col::BG);
    text(F_TOP, tr(T_PAIR_TITLE), lay::CX, lay::TOP_BASE, col::MUTED, col::BG, C_BASELINE);
    text(F_S, tr(T_PAIR_ENTER), lay::CX, 70, col::INK, col::BG, MC_DATUM);
    tft.fillRoundRect(lay::CX - 90, 92, 180, 48, 8, col::CARD);
    text(F_H, code, lay::CX, 116, col::AMBER, col::CARD, MC_DATUM);
    text(F_S3, tr(T_PAIR_VALID), lay::CX, 162, col::MUTED, col::BG, MC_DATUM);
    text(F_S3, String("claude-clock.local · ") + WiFi.localIP().toString(), lay::CX, lay::BOTTOM_BASE, col::DIM, col::BG,
         C_BASELINE);
    (void)now;
}

// ---------- animation ----------
static void hatOnTft(int x, int y, int px) {
    // first 3 rows of the hat frame, drawn above the body
    SpriteFrame hat{3, SPR_HAT_IDLE.data};
    drawSprite(tft, hat, x, y, px, col::CLAY);
}

// Sprite into a 4-bit palette sprite (colours are palette indices).
static void spriteIdx(TFT_eSprite &g, const SpriteFrame &f, int x, int y, int px, uint8_t body, int rows = 99) {
    for (int r = 0; r < f.rows && r < rows; r++)
        for (int c = 0; c < 14; c++) {
            uint8_t i;
            switch (spx(f, r, c)) {
                case 'o': i = body; break;
                case 'e': i = P_EYE; break;
                case 'b': i = P_LID; break;
                case 'r': i = P_APPLE; break;
                case 'h': i = P_HAT_RED; break;
                case 'w': i = P_HAT_WHITE; break;
                case 'k': i = P_BAND; break;
                default: continue;
            }
            g.fillRect(x + c * px, y + r * px, px, px, i);
        }
}
static void stageSprite(const SpriteFrame &f, int x, int y, uint8_t body, int rows = 99) {
    spriteIdx(stage, f, x, y, lay::SMALL_PX, body, rows);
}

// Pixel spinner in the Claude "✻" style: alternates + and x, 3 px blocks, 21x21.
static void pixelSpinner(TFT_eSprite &g, int cx, int cy, uint32_t tick, uint8_t c) {
    static const char *const PLUS[] = {"...x...", "...x...", "...x...", "xxxxxxx", "...x...", "...x...", "...x..."};
    static const char *const CROSS[] = {"x.....x", ".x...x.", "..x.x..", "...x...", "..x.x..", ".x...x.", "x.....x"};
    const char *const *f = (tick & 1) ? CROSS : PLUS;
    for (int r = 0; r < 7; r++)
        for (int k = 0; k < 7; k++)
            if (f[r][k] == 'x') g.fillRect(cx - 10 + 3 * k, cy - 10 + 3 * r, 3, 3, c);
}

static void animHero(const SpriteFrame &f, int dx, int dy, int accessory, uint32_t tick, bool hat) {
    // accessory: 0 none, 1 spinner, 2 amber badge "!", 3 amber ring "?"
    hero.fillSprite(P_BG);
    int bx = lay::CX - 42 - lay::HERO_X + dx;
    spriteIdx(hero, f, bx, 4 + dy, lay::HERO_PX, P_BODY);
    if (accessory == 1) pixelSpinner(hero, 158, 12, tick, P_BODY);
    if (accessory == 2) { hero.fillCircle(160, 12, 11, P_STAR); drawBang(hero, 160, 7, P_EYE); }
    if (accessory == 3) { hero.drawCircle(160, 12, 10, P_STAR); hero.drawCircle(160, 12, 9, P_STAR); drawBang(hero, 160, 7, P_STAR); }
    pushHero();
    if (hat) hatOnTft(lay::CX - 42, lay::HERO_Y + 4 - 3 * lay::HERO_PX, lay::HERO_PX);
}

// Static small mascot straight on the screen (sleep / limit screens: the frame never changes).
static void animSmall(const SpriteFrame &f, int y, uint16_t body, uint16_t bg, bool hat) {
    (void)bg;
    drawSprite(tft, f, lay::CX - SMALL_W / 2, y, lay::SMALL_PX, body);
    if (hat) {
        SpriteFrame h{3, SPR_HAT_IDLE.data};
        drawSprite(tft, h, lay::CX - SMALL_W / 2, y - 3 * lay::SMALL_PX, lay::SMALL_PX, body);
    }
}

// Tiny pixel glyph, 2 px blocks.
static void stageGlyph(const char *const *rows, int n, int x, int y, uint8_t c) {
    for (int r = 0; r < n; r++)
        for (int k = 0; rows[r][k]; k++)
            if (rows[r][k] == 'x') stage.fillRect(x + 2 * k, y + 2 * r, 2, 2, c);
}

// Canvas over a 4-bit sprite: scene coordinates are screen px, (ox, oy) is where the sprite sits.
struct SpriteCanvas : Canvas {
    TFT_eSprite &s;
    int ox, oy;
    SpriteCanvas(TFT_eSprite &sp, int x, int y) : s(sp), ox(x), oy(y) {}
    void rect(int x, int y, int w, int h, uint8_t slot, int r) override {
        x -= ox;
        y -= oy;
        int m = (w < h ? w : h) / 2;
        if (r > 0 && m > 0) s.fillRoundRect(x, y, w, h, r < m ? r : m, slot);
        else s.fillRect(x, y, w, h, slot);
    }
};

// New Year scenery behind the mascot: snow, a drift at the bottom, a small tree with a blinking garland.
static void drawWinter(uint32_t t, bool resting) {
    uint32_t tl = resting ? t / 12 : t;  // garland: slow twinkle at rest
    Dot d[12];
    int n = snowFlakes(t, d, 12);
    for (int i = 0; i < n; i++) {
        int s = i % 3 == 0 ? 3 : 2;
        stage.fillRect(d[i].x, d[i].y, s, s, i % 3 == 0 ? P_HAT_WHITE : P_GREY);
    }
    for (int x = 0; x < IDLE_STAGE_W; x += 6) stage.fillRect(x, IDLE_STAGE_H - ((x / 6) % 4 == 0 ? 6 : 4), 6, 6, P_GREY);
    static const char *const TREE[] = {"....g....", "...ggg...", "..ggggg..", "...ggg...", "..ggggg..", ".ggggggg.", "ggggggggg", "....t...."};
    static const int LX[] = {4, 3, 6, 2, 5, 1, 7}, LY[] = {1, 2, 4, 5, 5, 6, 6};
    static const uint8_t LC[] = {P_APPLE, P_STAR, P_SWEAT};
    const int tx = 124, ty = 24;  // inside the visible part of the 1.2x stage (columns 16..167)
    for (int r = 0; r < 8; r++)
        for (int k = 0; k < 9; k++)
            if (TREE[r][k] != '.') stage.fillRect(tx + 4 * k, ty + 4 * r, 4, 4, TREE[r][k] == 'g' ? P_LEAF : P_LID);
    stage.fillRect(tx + 16, ty - 5, 4, 4, P_STAR);
    for (int i = 0; i < 7; i++)
        if (treeLightOn(tl, i)) stage.fillRect(tx + 4 * LX[i], ty + 4 * LY[i], 4, 4, LC[i % 3]);
}

static void drawStage(const IdlePose &p, uint8_t body, bool hat, bool winter = false, uint32_t t = 0, bool resting = true,
                      int month = 0) {
    static const char *const STAR[] = {"x.x.x", ".xxx.", "xxxxx", ".xxx.", "x.x.x"};
    static const char *const FLAKE[] = {"..x..", "x.x.x", ".xxx.", "x.x.x", "..x.."};
    static const char *const ZED[] = {"xxxx", "..x.", ".x..", "xxxx"};
    stage.fillSprite(P_BG);
    if (winter) drawWinter(t, resting);
    else if (month) {
        SpriteCanvas sc(stage, lay::CL, lay::STAGE_Y);
        seasonLayer(sc, month, t, true);  // the season keeps moving while the mascot rests
    }
    if (p.fx & FX_FIREWORK) {
        static const uint8_t FC[] = {P_STAR, P_APPLE, P_SWEAT};
        Dot d[30];
        uint8_t c[30];
        int n = fireworkDots(t, d, c, 30);
        for (int i = 0; i < n; i++) stage.fillRect(d[i].x, d[i].y, 3, 3, FC[c[i]]);
    }
    int dy = (hat && !p.band && p.dy < -1) ? -1 : p.dy;  // the hat would leave the stage on high jumps
    if (hat) dy += 4;  // keep the hat clear of the time digits
    int x = SPR_X + p.dx, y = SPR_Y + dy;
    stageSprite(*p.frame, x, y, body);
    if (hat && !p.band) stageSprite(SPR_HAT_IDLE, x, y - 3 * lay::SMALL_PX, body, 3);
    if (p.fx & FX_STAR) stageGlyph(STAR, 5, SPR_X + p.fxX, SPR_Y + p.fxY, P_STAR);
    if (p.fx & FX_FLAKE) stageGlyph(FLAKE, 5, SPR_X + p.fxX, SPR_Y + p.fxY, P_HAT_WHITE);
    if (p.fx & FX_Z) stageGlyph(ZED, 4, SPR_X + p.fxX, SPR_Y + p.fxY, P_Z);
    if (p.fx & FX_SPARK) {  // hourly party: three sparkles blinking in turn
        static const char *const SPARK[] = {".x.", "xxx", ".x."};
        static const int SX[] = {22, 148, 132}, SY[] = {20, 32, 12};
        static const uint8_t SC[] = {P_STAR, P_LEAF, P_BODY};
        for (int i = 0; i < 3; i++)
            if ((i + p.fxX) % 2 == 0 || i == 2) stageGlyph(SPARK, 3, SX[i], SY[i], SC[i]);
    }
    if (p.fx & FX_SWEAT) stage.fillRect(x + p.fxX - p.dx, SPR_Y + p.fxY, 3, 5, P_SWEAT);
    if (p.fx & FX_FOOD) {
        int fx = x + 63, fy = y + 15;
        stage.fillRect(fx, fy, p.foodW, 10, P_APPLE);
        stage.fillRect(fx + 2, fy - 3, 4, 3, P_LEAF);
    }
    if (p.fx & FX_CRUMB) {
        stage.fillRect(x + 64, SPR_Y + p.crumbY, 3, 3, P_APPLE);
        stage.fillRect(x + 70, SPR_Y + p.crumbY + 4, 2, 2, P_CRUMB);
    }
    pushStageScaled();
}

// The clock stage is drawn at 184x70 and shown 1.2x larger (5 px sprite pixels -> 6 px), cropped to the column:
// every 5th stage row and column is doubled. Same-colour runs go out as one rect, so a frame is a few hundred rects.
static void pushStageScaled() {
    const int W = lay::CR - lay::CL;
    static uint8_t colMap[lay::CR - lay::CL];
    static bool mapped = false;
    if (!mapped) {
        for (int ox = 0; ox < W; ox++) colMap[ox] = uint8_t(W / 2 + ((ox - W / 2) * 5 + 600) / 6 - 100);
        mapped = true;
    }
    for (int sy = 0; sy < STAGE_H; sy++) {
        int oy0 = (sy * 6 + 4) / 5, oy1 = ((sy + 1) * 6 + 4) / 5;
        if (oy1 > lay::STAGE_SHOW_H) oy1 = lay::STAGE_SHOW_H;
        if (oy0 >= oy1) continue;
        int run = 0;
        uint16_t c = stage.readPixel(colMap[0], sy);
        for (int ox = 1; ox <= W; ox++) {
            uint16_t n = ox < W ? stage.readPixel(colMap[ox], sy) : uint16_t(~c);
            if (n == c) continue;
            tft.fillRect(lay::CL + run, STAGE_Y + oy0, ox - run, oy1 - oy0, c);
            run = ox;
            c = n;
        }
    }
}

// New Year midnight: the year under the dancing mascot (clock screen, 00:00 on Jan 1).
static void drawYearBanner(int year) {
    static int shown = 0;
    if (year == shown) return;
    tft.fillRect(lay::CL, lay::YEAR_Y - lay::YEAR_CLEAR_H / 2, lay::CR - lay::CL, lay::YEAR_CLEAR_H, col::BG);
    if (year > 0) text(F_H, String(year), lay::CX, lay::YEAR_Y, col::AMBER, col::BG, MC_DATUM);
    shown = year;
}

// Sleep screen: breathing mascot with z's floating up (stage moved down under the date).
static const int SLEEP_STAGE_Y = lay::SLEEP_STAGE_Y;
static void drawSleepStage(uint32_t t, bool hat, bool winter) {
    static const char *const ZBIG[] = {"xxxx", "..x.", ".x..", "xxxx"};
    static const char *const ZSMALL[] = {"xxx", ".x.", "xxx"};
    SleepPose p = sleepPose(t);
    stage.fillSprite(P_BG);
    int x = SPR_X, y = lay::SLEEP_SPRITE_Y - SLEEP_STAGE_Y + p.dy;
    stageSprite(SPR_SLEEP, x, y, P_LID);
    if (hat) stageSprite(SPR_HAT_IDLE, x, y, P_LID, 3);
    int headY = y + 3 * lay::SMALL_PX;  // first visible row of the sleeping sprite
    static const char *const FLAKE[] = {"x.x", ".x.", "x.x"};
    for (int i = 0; i < p.zCount; i++) {
        int zx = x + SMALL_W + p.z[i].x, zy = headY + p.z[i].y;
        if (winter) stageGlyph(FLAKE, 3, zx, zy, P_SLEEP_INK);  // New Year: snowflakes instead of "z"
        else stageGlyph(p.z[i].big ? ZBIG : ZSMALL, p.z[i].big ? 4 : 3, zx, zy, P_SLEEP_INK);
    }
    stage.pushSprite(lay::CL, SLEEP_STAGE_Y);
}

// ---------- scene pack (lib/model/scene*.cpp) ----------

// Slots 0..6 are the fixed colours (same as PALETTE), 7..15 the scene's own.
static void scenePalette(TFT_eSprite &s, const Scene &sc) {
    uint16_t pal[16];
    for (int i = 0; i < 7; i++) pal[i] = PALETTE[i];
    for (int i = 0; i < 9; i++) pal[7 + i] = sc.colours[i] ? sc.colours[i] : col::BG;
    pal[15] = col::GREEN;  // reserved for the season background
    s.createPalette(pal, 16);
}

static void drawSceneStage(const Scene &sc, uint32_t p, uint32_t t, bool hat) {
    scenePalette(stage, sc);
    stage.fillSprite(S_BG);
    if (hat) {  // New Year: snow behind (fixed colours only; the tree needs palette slots the scene uses)
        Dot d[12];
        int n = snowFlakes(t, d, 12);
        for (int i = 0; i < n; i++) stage.fillRect(d[i].x, d[i].y, 2, 2, i % 3 ? S_HATW : S_INK);
    }
    SpriteCanvas c(stage, lay::CL, lay::STAGE_Y);
    MascotAt m = sc.draw(c, p, t);
    if (hat && m.hatOk) frame(c, SPR_HAT_IDLE, m.x, m.y - 3 * lay::SMALL_PX, lay::SMALL_PX, S_BODY, 3);
    pushStageScaled();
    stage.createPalette(PALETTE, 16);
}

// Info card under the mascot (clock screen): air-raid alert, holiday caption, weather, today with Claude.
// 12x10 pixel icons drawn at 2x; letters pick the colour.
static const char ICONS[][10][13] PROGMEM = {
    {},  // None
    {".....y......", ".y.......y..", "....yyy.....", "...yyyyy....", "y.yyyyyyy.y.", "...yyyyy....", "....yyy.....",
     ".y.......y..", ".....y......", "............"},  // Sun
    {"....mmm.....", "..mmm.......", ".mmm........", ".mm.........", ".mm.........", ".mmm........", "..mmmm..m...",
     "...mmmmmm...", "............", "............"},  // Moon
    {"..y.y.......", "...yyy......", "..yyyyy.....", ".yyyywww....", "..yywwwwww..", "..wwwwwwwww.", ".wwwwwwwwwww",
     "..wwwwwwwww.", "............", "............"},  // SunCloud
    {"..mm........", ".mm.........", ".mm.........", ".mmm.www....", "..mmwwwwww..", "..wwwwwwwww.", ".wwwwwwwwwww",
     "..wwwwwwwww.", "............", "............"},  // MoonCloud
    {"............", "............", ".....www....", "...wwwwwww..", "..wwwwwwwww.", ".wwwwwwwwwww", ".wwwwwwwwwww",
     "..wwwwwwwww.", "............", "............"},  // Cloud
    {"....www.....", "..wwwwwww...", ".wwwwwwwww..", "wwwwwwwwwww.", ".wwwwwwwww..", "............", ".b..b..b....",
     "b..b..b.....", "............", "............"},  // Rain
    {"....www.....", "..wwwwwww...", ".wwwwwwwww..", "wwwwwwwwwww.", ".wwwwwwwww..", "............", ".i...i..i...",
     "...i...i....", ".i...i...i..", "............"},  // Snow
    {"....www.....", "..wwwwwww...", ".wwwwwwwww..", "wwwwwwwwwww.", ".wwwwwwwww..", ".....yy.....", "....yy......",
     "...yyyy.....", ".....y......", "............"},  // Storm
    {"............", ".wwwwwwwww..", "............", "..wwwwwwwwww", "............", ".wwwwwwwww..", "............",
     "..wwwwwwww..", "............", "............"},  // Fog
    {".....c......", "..c..c..c...", "...c.c.c....", "....ccc.....", "ccccccccccc.", "....ccc.....", "...c.c.c....",
     "..c..c..c...", ".....c......", "............"},  // Claude
    {"....ff......", "...ffff.....", "..ffffff....", "..ffffff....", "..ffffff....", "..ffffff....", ".ffffffff...",
     "ffffffffff..", "............", "............"},  // Siren (card text colour)
    {"....y.......", "....y.......", "...yyy......", "yyyyyyyyy...", ".yyyyyyy....", "..yyyyy.....", "..yy.yy.....",
     ".yy...yy....", "............", "............"},  // Star
    {"..i.....i...", ".cciccccicc.", ".cccccccccc.", ".iiiiiiiiii.", ".i.ii.ii.ii.", ".iiiiiiiiii.", ".ii.ii.ii.i.",
     ".iiiiiiiiii.", "............", "............"},  // Calendar
    {"............", ".ffffffffff.", "ffffffffffff", "ff........ff", "ff.ffffff.ff", "ff........ff", "ffffffffffff",
     ".ffffffffff.", "..ff........", ".f.........."},  // Message (speech bubble, card text colour)
};

static void drawIcon(InfoIcon ic, int x, int y, uint16_t fg) {
    const char(*rows)[13] = ICONS[int(ic)];
    if (!pgm_read_byte(&rows[0][0])) return;
    for (int r = 0; r < 10; r++)
        for (int k = 0; k < 12; k++) {
            uint16_t c;
            switch (char(pgm_read_byte(&rows[r][k]))) {
                case 'y': c = col::AMBER; break;
                case 'm': c = rgb565(0xF0, 0xE0, 0xA0); break;
                case 'w': c = rgb565(0xF0, 0xE4, 0xC8); break;  // warm: the panel adds blue
                case 'b': c = col::BLUE; break;
                case 'i': c = col::INK; break;
                case 'c': c = col::CLAY; break;
                case 'f': c = fg; break;
                default: continue;
            }
            tft.fillRect(x + 2 * k, y + 2 * r, 2, 2, c);
        }
}

static String captionShown, dotsShown;
// line1 empty: no card. Redraws only when something changed.
static void drawInfoCard(InfoIcon icon, const String &l1, const String &l2, uint16_t fg, uint16_t sub, uint16_t bg) {
    String key = l1.length() ? String(int(icon)) + l1 + "|" + l2 + "|" + fg + "/" + bg : String();
    if (key == captionShown) return;
    captionShown = key;
    const int x = lay::INFO_X, y = lay::INFO_Y, w = lay::INFO_W, h = lay::INFO_H;
    tft.fillRect(x - 2, y, w + 4, h, col::BG);
    if (!l1.length()) return;
    tft.fillSmoothRoundRect(x, y, w, h, 7, bg, col::BG);
    drawIcon(icon, x + 8, y + (h - 20) / 2, fg);
    int tw = w - lay::INFO_TEXT_DX - lay::INFO_PAD_R;
    text(F_S, fitText(F_S, l1, tw), x + lay::INFO_TEXT_DX, y + 14, fg, bg, ML_DATUM);
    if (l2.length()) text(F_S3, fitText(F_S3, l2, tw), x + lay::INFO_TEXT_DX, y + 31, sub, bg, ML_DATUM);
}

// Rotation dots between the two reset timers: which info item is on the card now.
static void drawInfoDots(int items, int index) {
    String key = String(items) + "/" + index;
    if (key == dotsShown) return;
    dotsShown = key;
    tft.fillRect(lay::CX - 30, lay::INFO_DOTS_Y - 5, 60, 10, col::BG);
    if (items < 2) return;
    int x0 = lay::CX - (items - 1) * 5;
    for (int i = 0; i < items; i++)
        tft.fillSmoothCircle(x0 + i * 10, lay::INFO_DOTS_Y, i == index ? 3 : 2, i == index ? col::INK : col::DIM, col::BG);
}

// Work screens: scene by the focus session's state and tool; restarts when either changes.
static void drawWorkScene(const Model &m, const View &v, const LocalTime &lt, uint32_t now, uint32_t it) {
    static String key;
    static int idx = -1, morningDay = -1;
    static uint32_t start = 0, variant = 0;
    static bool greetMorning = false;
    WorkCtx w;
    w.screen = v.screen;
    w.state = v.focus ? v.focus->state : (v.screen == Screen::Done ? SState::Idle : SState::Working);
    w.tool = v.focus ? v.focus->tool.c_str() : "";
    w.toolAgeMs = v.focus ? now - v.focus->toolMs : 0;
    w.edits = v.focus ? v.focus->edits : 0;
    w.agents = v.focus ? v.focus->agents : 0;
    w.reaction = m.reaction();
    w.reactionAgeMs = m.reaction() == Reaction::None ? 0xFFFFFFFF : now - m.reactionAtMs();
    w.doneDurationMs = m.lastDoneDurationMs();
    w.synced = lt.synced;
    w.hour = lt.hour;
    w.greeting = m.greeting(now);
    if (w.greeting || v.screen == Screen::Hello) {  // first greeting of the morning -> "good morning"
        int dayKey = lt.month * 32 + lt.day;
        if (lt.synced && lt.hour >= 5 && lt.hour < 12 && dayKey != morningDay) { morningDay = dayKey; greetMorning = true; }
    } else {
        greetMorning = false;
    }
    w.morning = greetMorning;
    String k = String(int(v.screen)) + "|" + int(w.state) + "|" + w.tool;
    if (k != key) { key = k; variant = ESP.random(); }  // unknown tools: a new random look per step
    w.variant = variant;
    int pick = pickWorkScene(w);
    if (pick != idx) { idx = pick; start = it; }
    if (idx < 0) return;
    const Scene &sc = sceneAt(idx);
    scenePalette(hero, sc);
    hero.fillSprite(S_BG);
    SpriteCanvas c(hero, lay::HERO_X, lay::HERO_Y);
    sc.draw(c, it - start, it);
    pushHero();
    hero.createPalette(PALETTE, 16);
}

static SceneCtx sceneContext(const Model &m, const LocalTime &lt, uint32_t now) {
    static int lastH5 = -1;
    static uint32_t resetUntil = 0;
    int h5 = m.hasLimits() && !m.limitsStale(now) ? m.limit5h().pct : -1;
    if (lastH5 >= 0 && h5 >= 0 && h5 + 30 <= lastH5) resetUntil = (now + 120000) | 1;  // 5h window just reset
    if (h5 >= 0) lastH5 = h5;
    SceneCtx c;
    c.synced = lt.synced;
    c.year = lt.year; c.month = lt.month; c.day = lt.day; c.wday = lt.wday; c.hour = lt.hour; c.min = lt.min;
    c.limitReset = resetUntil && int32_t(resetUntil - now) > 0;
    c.longSession = m.lastDoneDurationMs() >= 3600000 && now - m.lastDoneAtMs() < 1800000;
    deviceBirthday(c.birthMonth, c.birthDay);
    uint32_t quiet = m.quietMs(now);
    bool day = lt.synced && lt.hour >= 7 && lt.hour < 23;
    bool evening = lt.synced && lt.hour >= EVENING_FROM_H;
    c.bored = day && !evening && quiet >= BORED_MS && quiet < AWAY_SLEEP_MS;  // daytime only
    // evening: Claude closed, or 15-30 min of quiet -> say good night, switch off the lamp (then the screen sleeps)
    c.goodnight = evening && m.lastEventMs() &&
                  ((m.count() == 0 && quiet < 10u * 60000u) || (quiet >= 15u * 60000u && quiet < EVENING_SLEEP_MS));
    c.weekClosed = lt.wday == 5;
    const Info &in = infoState();
    c.weather = in.weather(now);
    c.alert = isAlert(in.alert(now));
    c.alertEnded = in.alertEnded(now);
    uint32_t epoch = lt.synced ? uint32_t(time(nullptr)) : 0;
    c.meetingSoon = in.meetingSoonMin(epoch) >= 0;
    c.meetingNow = in.inMeeting(epoch);
    return c;
}

static void animate(const Model &m, const View &v, uint32_t tick, bool hat) {
    bool odd = tick & 1;
    switch (v.screen) {
        case Screen::Permission: {
            uint16_t ring = (tick % 4) < 2 ? col::AMBER : col::BG;
            for (int i = 0; i < 3; i++) tft.drawRect(i, i, 240 - 2 * i, 240 - 2 * i, i == 0 ? col::AMBER : ring);
            break;
        }

        case Screen::Multi: {
            int i = 0;
            for (const auto &s : m.sessions()) {
                if (i >= 3) break;
                (void)s;
                i++;
            }
            break;
        }
        case Screen::MacSilent: {
            IdlePose p{&SPR_IDLE, 0, 0, 0, 0, 0, 0, 0, false};
            drawStage(p, P_GREY, false);
            break;
        }
        case Screen::LimitReached:
            animSmall(SPR_SLEEP, 30, col::CLAY, col::BG, false);
            break;
        default: break;
    }
}

void screensUpdate(const Model &m, uint32_t now, const LocalTime &lt, time_t epoch, const NetInfo &net, bool frame,
                   uint32_t tick) {
    View v = m.view(now, lt);
    bool preview = nyPreviewUntil && int32_t(nyPreviewUntil - now) > 0;
    bool hat = (lt.synced && hatSeason(lt.month, lt.day)) || preview;
    if (v.brightness != lastBright) { setBrightness(v.brightness); lastBright = v.brightness; }

    if (net.apOn) {  // setup mode overrides everything: without Wi-Fi nothing else can update
        if (firstDraw || lastScreen != Screen::FirstSetup) {
            firstDraw = false;
            lastScreen = Screen::FirstSetup;
            for (auto &c : lineCache) c = "";
        }
        drawSetup(net, now, frame, tick);
        return;
    }
    if (devicePairShowing(now)) {  // a computer asked for the pairing code
        if (lastScreen != Screen::FirstSetup || lineCache[1] != "pair") {
            lastScreen = Screen::FirstSetup;
            for (auto &c : lineCache) c = "";
            lineCache[1] = "pair";
        }
        drawPair(now);
        return;
    }
    // Wi-Fi lost after the time is known: Claude's state is stale -> grey clock with a "no Wi-Fi" card.
    static uint32_t lastOnline = 0;
    if (net.connected || !lt.synced) lastOnline = now;
    bool offline = now - lastOnline > 30000;
    if (offline) v.screen = offlineScreen(v.screen);
    offlineNow = offline;
    offlineSinceMs = lastOnline;

    if (firstDraw || v.screen != lastScreen) {
        firstDraw = false;
        lastScreen = v.screen;
        tft.fillScreen(v.screen == Screen::Sleep ? TFT_BLACK : col::BG);
        lastMinuteKey = -1000;
        lastIdleTick = UINT32_MAX;
        captionShown = dotsShown = ctxShown = "";
        lastTopL = lastGauge = "";
        for (auto &c : lineCache) c = "";
        frame = true;
    }
    if (v.screen == Screen::Boot) {
        drawBoot(net, frame, tick);
        return;
    }
    if (v.screen != Screen::Sleep) {
        topBar(m, v, lt, net);
        gauges(m, now, epoch);
    }
    switch (v.screen) {
        case Screen::Clock: drawClockTime(lt, now, false); break;
        case Screen::MacSilent: drawClockTime(lt, now, false); drawSilentCard(m, now); break;
        case Screen::Sleep: drawClockTime(lt, now, true); break;
        case Screen::LimitReached: drawLimitReached(m, epoch, lt); break;
        case Screen::Multi: drawMulti(m); break;
        default:
            if (isWorkScreen(v.screen) && (v.focus || v.screen == Screen::Done || v.screen == Screen::Hello)) {
                drawWorkBody(m, v, now);
                contextBar(v.focus);
            }
            break;
    }
    bool workAnim = isWorkScreen(v.screen);
    if (v.screen == Screen::Clock || v.screen == Screen::Sleep || workAnim) {  // mascots run on the faster idle tick
        uint32_t it = now / IDLE_TICK_MS;
        if (it != lastIdleTick && workAnim) {
            lastIdleTick = it;
            drawWorkScene(m, v, lt, now, it);
            if (hat) hatOnTft(lay::CX - 42, lay::HERO_Y + 4 - 3 * lay::HERO_PX, lay::HERO_PX);
        }
        if (it != lastIdleTick) {
            lastIdleTick = it;
            idle.setContext(sceneContext(m, lt, now));
            {
                static String animShown = "-";
                if (animShown != deviceAnim()) {  // parse only when the setting changed
                    animShown = deviceAnim();
                    AnimPrefs a;
                    a.decode(animShown.c_str());
                    idle.setPrefs(a);
                }
            }
            bool tired = !m.limitsStale(now) && m.limit5h().pct >= 90;
            bool midnight = (lt.synced && lt.month == 1 && lt.day == 1 && lt.hour == 0 && lt.min == 0) ||
                            (preview && nyPreviewMidnight);
            idle.setCalendar(hat, midnight);
            const char *caption = nullptr, *date = nullptr;
            if (v.screen == Screen::Clock) {
                IdlePose pose = idle.pose(it, lt.min, tired);
                static int previewing = -1;  // a scene asked for from the page / CLI: not cut short by the pin
                if (previewing >= 0 && !(idle.act() == IdleAct::Scene && idle.sceneIndex() == previewing)) previewing = -1;
                if (demoOnly >= 0) {  // "play now": at once, even over a playing (or pinned) scene
                    idle.playScene(it, demoOnly);
                    previewing = demoOnly;
                    demoOnly = -1;
                    pose = idle.pose(it, lt.min, tired);
                } else if (idle.act() != IdleAct::Scene && demoNext >= 0) {  // "all in turn"
                    while (demoNext < sceneCount() && sceneAt(demoNext).kind == SceneKind::Work) demoNext++;
                    if (demoNext < sceneCount()) { previewing = demoNext; idle.playScene(it, demoNext++); }
                    else demoNext = -1;
                    pose = idle.pose(it, lt.min, tired);
                } else if (demoNext < 0 && previewing < 0) {  // a pinned scene loops instead of the random pick
                    int pin = pinnedScene(devicePin(), lt.synced ? uint32_t(epoch) : 0);
                    if (pin >= 0 && !(idle.act() == IdleAct::Scene && idle.sceneIndex() == pin)) {
                        idle.playScene(it, pin);
                        pose = idle.pose(it, lt.min, tired);
                    }
                }
                if (idle.act() == IdleAct::Scene) {
                    const Scene &sc = sceneAt(idle.sceneIndex());
                    drawSceneStage(sc, idle.sceneTick(it), it, hat);
                    if (sc.kind == SceneKind::Holiday) { caption = sceneCaption(sc.name, sc.caption); date = sc.date; }
                } else {
                    drawStage(pose, P_BODY, hat, hat, it, idle.act() == IdleAct::Rest, lt.synced ? lt.month : 0);
                }
                Info::Card card = infoState().card(now, lt.month * 32 + lt.day, lt.synced ? uint32_t(time(nullptr)) : 0);
                if (midnight) {  // New Year: the year banner takes the card's place
                    drawInfoCard(InfoIcon::None, "", "", 0, 0, 0);
                    card.items = 0;
                } else if (card.tone == 5) {  // threat (yellow level)
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), col::INK, rgb565(0xF0, 0xD8, 0x90),
                                 rgb565(0x4A, 0x3A, 0x00));
                } else if (card.tone == 6) {  // partial alert
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), col::INK, rgb565(0xFF, 0xD0, 0xC0),
                                 rgb565(0x5A, 0x14, 0x10));
                } else if (card.tone == 4) {  // `claude-clock say`: pinned, green
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), col::INK, rgb565(0xC0, 0xF0, 0xC8),
                                 rgb565(0x14, 0x4C, 0x24));
                } else if (card.tone == 3) {  // meeting soon / now: pinned, blue
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), col::INK, rgb565(0xC8, 0xD8, 0xF8),
                                 rgb565(0x18, 0x34, 0x70));
                } else if (card.tone) {  // alert wins over all
                    bool drone = card.tone == 1;
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), drone ? col::BG : col::INK,
                                 drone ? col::BG : rgb565(0xFF, 0xD0, 0xC0), drone ? col::AMBER : col::RED);
                } else if (caption) {
                    drawInfoCard(InfoIcon::Star, caption, date ? date : "", col::AMBER, col::MUTED, col::CARD);
                    card.items = 0;
                } else {
                    drawInfoCard(card.icon, card.line1.c_str(), card.line2.c_str(), col::INK, col::MUTED, col::CARD);
                }
                drawInfoDots(card.items, card.index);
            } else {
                drawSleepStage(it, hat, hat);
            }
            drawYearBanner(v.screen == Screen::Clock && midnight ? lt.year + (preview ? 1 : 0) : 0);
        }
    }
    if (frame) {
        animate(m, v, tick, hat);
        if (v.screen == Screen::Multi) {
            int i = 0;
            for (int pass = 0; pass < 2; pass++)
                for (const auto &s : m.sessions()) {
                    bool perm = s.state == SState::Permission;
                    if ((pass == 0 && perm) || (pass == 1 && !perm && isWorking(s))) {
                        if (i < 3 && pass == 1) {
                            int y = 36 + i * 54;
                            tft.fillRect(lay::CR - 26, y + 9, 24, 28, col::CARD);
                            drawSpinner(tft, lay::CR - 14, y + 23, tick, col::CLAY, col::CARD);
                        }
                        i++;
                    }
                }
        }
    }
}
