#include "settings.h"
#include <cstdlib>
#include "rawstore.h"

const char SETTINGS_MAGIC[5] = "CCS1";

std::vector<uint8_t> encodeSettingsRec(uint32_t seq, const Settings &s) {
    std::string p = "token=" + s.token + "\ntz=" + s.tz + "\n";
    if (!s.lang.empty()) p += "lang=" + s.lang + "\n";
    if (!s.owm.empty()) p += "owm=" + s.owm + "\n";
    if (!s.loc.empty()) p += "loc=" + s.loc + "\n";
    if (!s.region.empty()) p += "region=" + s.region + "\n";
    int d, a, c;
    uint32_t b;
    if (parseToday(s.today, d, b, a, c)) p += "today=" + s.today + "\n";
    int x, y;
    std::string name;
    uint32_t until;
    if (!s.night.empty() && parseNight(s.night, x, y)) p += "night=" + s.night + "\n";
    if (!s.bright.empty() && parseBright(s.bright, x, y)) p += "bright=" + s.bright + "\n";
    if (!s.face.empty() && validFace(s.face)) p += "face=" + s.face + "\n";
    if (!s.pin.empty() && parsePin(s.pin, name, until)) p += "pin=" + s.pin + "\n";
    if (!s.week.empty() && validWeek(s.week)) p += "week=" + s.week + "\n";
    if (!s.anim.empty() && validAnim(s.anim)) p += "anim=" + s.anim + "\n";
    if (!s.atok.empty() && validAlertsToken(s.atok)) p += "atok=" + s.atok + "\n";
    if (s.birthMonth) {
        char b[16];
        snprintf(b, sizeof b, "birth=%02d-%02d\n", s.birthMonth, s.birthDay);
        p += b;
    }
    return encodeBlob(SETTINGS_MAGIC, seq, p);
}

bool decodeSettingsRec(const uint8_t *buf, size_t n, uint32_t &seq, Settings &out) {
    std::string p;
    if (!decodeBlob(SETTINGS_MAGIC, buf, n, SETTINGS_REC_MAX - 14, seq, p)) return false;
    Settings s;
    size_t i = 0;
    while (i < p.size()) {
        size_t e = p.find('\n', i);
        if (e == std::string::npos) e = p.size();
        std::string line = p.substr(i, e - i);
        i = e + 1;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "token" && validToken(v)) s.token = v;
        else if (k == "tz" && validTz(v)) s.tz = v;
        else if (k == "lang" && (v == "en" || v == "uk")) s.lang = v;
        else if (k == "owm" && validOwmKey(v)) s.owm = v;
        else if (k == "loc" && validLoc(v)) s.loc = v;
        else if (k == "region" && validRegion(v)) s.region = v;
        else if (k == "today") { int d, a, c; uint32_t b; if (parseToday(v, d, b, a, c)) s.today = v; }
        else if (k == "night") { int a, b; if (parseNight(v, a, b)) s.night = v; }
        else if (k == "bright") { int a, b; if (parseBright(v, a, b)) s.bright = v; }
        else if (k == "face" && validFace(v)) s.face = v;
        else if (k == "pin") { std::string nm; uint32_t u; if (parsePin(v, nm, u)) s.pin = v; }
        else if (k == "week" && validWeek(v)) s.week = v;
        else if (k == "anim" && validAnim(v)) s.anim = v;
        else if (k == "atok" && validAlertsToken(v)) s.atok = v;
        else if (k == "birth") { int m, d; if (parseBirthday(v, m, d)) { s.birthMonth = m; s.birthDay = d; } }
    }
    out = s;
    return true;
}

bool validTz(const std::string &tz) {
    if (tz.empty() || tz.size() > 48) return false;
    for (char c : tz) {
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ',' || c == '.' ||
                  c == ':' || c == '/' || c == '+' || c == '-' || c == '<' || c == '>';
        if (!ok) return false;
    }
    return true;
}

bool validToken(const std::string &t) {
    if (t.size() < 16 || t.size() > 64) return false;
    for (char c : t)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
    return true;
}

bool validOwmKey(const std::string &k) {
    if (k.size() != 32) return false;
    for (char c : k)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

static bool coord(const std::string &s, double lim) {
    if (s.empty() || s.size() > 12) return false;
    for (size_t i = 0; i < s.size(); i++)
        if (!((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || (s[i] == '-' && i == 0))) return false;
    char *end = nullptr;
    double v = strtod(s.c_str(), &end);
    return *end == 0 && v >= -lim && v <= lim;
}

bool validLoc(const std::string &l) {
    size_t c = l.find(',');
    return c != std::string::npos && coord(l.substr(0, c), 90) && coord(l.substr(c + 1), 180);
}

bool validRegion(const std::string &r) {
    if (r.empty() || r.size() > 80) return false;
    for (unsigned char c : r)
        if (c < 0x20 || c == '"' || c == '\\' || c == 0x7F) return false;
    return true;
}

bool parseToday(const std::string &s, int &day, uint32_t &busyMs, int &answers, int &commits) {
    if (s.empty() || s.size() > 40) return false;
    unsigned long v[4];
    const char *p = s.c_str();
    for (int i = 0; i < 4; i++) {
        if (*p < '0' || *p > '9') return false;
        char *end;
        v[i] = strtoul(p, &end, 10);
        p = end;
        if (i < 3 && *p++ != ',') return false;
    }
    if (*p) return false;
    day = int(v[0]); busyMs = uint32_t(v[1]); answers = int(v[2]); commits = int(v[3]);
    return true;
}

bool parseBirthday(const std::string &s, int &month, int &day) {
    if (s.empty()) { month = day = 0; return true; }
    if (s.size() != 5 || s[2] != '-') return false;
    for (int i : {0, 1, 3, 4}) if (s[i] < '0' || s[i] > '9') return false;
    int m = (s[0] - '0') * 10 + (s[1] - '0'), d = (s[3] - '0') * 10 + (s[4] - '0');
    static const int DAYS[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12 || d < 1 || d > DAYS[m - 1]) return false;
    month = m;
    day = d;
    return true;
}

std::string makeToken(uint32_t (*rnd)()) {
    static const char A[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string t;
    for (int i = 0; i < 32; i++) t += A[rnd() % (sizeof A - 1)];
    return t;
}

std::string makePairCode(uint32_t (*rnd)()) {
    std::string a(PAIR_ALPHABET), c;
    for (int i = 0; i < 6; i++) c += a[rnd() % a.size()];
    return c;
}

void PairWindow::open(const std::string &code, uint32_t nowMs, uint32_t forMs) {
    code_ = code;
    start_ = nowMs;
    len_ = forMs;
    open_ = true;  // wrong tries are NOT reset: a new code gives no new guesses until reboot
}

bool PairWindow::isOpen(uint32_t nowMs) const { return open_ && wrong_ < MAX_TRIES && nowMs - start_ <= len_; }

PairResult PairWindow::check(const std::string &code, uint32_t nowMs) {
    if (open_ && wrong_ >= MAX_TRIES) return PairResult::Locked;
    if (!isOpen(nowMs)) return PairResult::Closed;
    std::string up;
    for (char c : code) up += (c >= 'a' && c <= 'z') ? char(c - 32) : c;
    if (up != code_) { wrong_++; return PairResult::Wrong; }
    open_ = false;
    return PairResult::Ok;
}

// Small strict number reader: digits only, at most maxDigits; returns false on anything else.
static bool num(const std::string &s, size_t &i, int maxDigits, long &out) {
    size_t st = i;
    out = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9' && int(i - st) < maxDigits) out = out * 10 + (s[i++] - '0');
    return i > st && (i == s.size() || s[i] < '0' || s[i] > '9');
}

bool parseNight(const std::string &s, int &from, int &to) {
    if (s.empty()) { from = 23; to = 7; return true; }
    if (s == "off") { from = to = 0; return true; }
    size_t i = 0;
    long a, b;
    if (!num(s, i, 2, a) || i >= s.size() || s[i++] != '-' || !num(s, i, 2, b) || i != s.size()) return false;
    if (a > 23 || b > 23 || a == b) return false;
    from = int(a);
    to = int(b);
    return true;
}

bool parseBright(const std::string &s, int &day, int &night) {
    if (s.empty()) { day = 80; night = 20; return true; }
    size_t i = 0;
    long a, b;
    if (!num(s, i, 3, a) || i >= s.size() || s[i++] != '/' || !num(s, i, 3, b) || i != s.size()) return false;
    if (a < 5 || a > 100 || b < 5 || b > 100) return false;
    day = int(a);
    night = int(b);
    return true;
}

bool validFace(const std::string &s) { return s.empty() || (s.size() == 1 && s[0] >= '0' && s[0] <= '6'); }

bool parsePin(const std::string &s, std::string &name, uint32_t &until) {
    if (s.empty()) { name.clear(); until = 0; return true; }
    size_t c = s.find(',');
    if (c == std::string::npos || c == 0 || c > 24) return false;
    for (size_t i = 0; i < c; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '-')) return false;
    size_t i = c + 1;
    long u;
    if (!num(s, i, 10, u) || i != s.size() || u > 4294967295L) return false;
    name = s.substr(0, c);
    until = uint32_t(u);
    return true;
}

// "ord:b,a,c;b,a,c;..." 1..8 days ending at day number ord (days since 1970): busy minutes <= 1440,
// answers <= 9999, commits <= 999.
bool validWeek(const std::string &s) {
    size_t i = 0;
    long v;
    if (!num(s, i, 5, v) || i >= s.size() || s[i++] != ':') return false;
    for (int d = 0; d < 8; d++) {
        long b, a, c;
        if (!num(s, i, 4, b) || b > 1440 || i >= s.size() || s[i++] != ',') return false;
        if (!num(s, i, 4, a) || i >= s.size() || s[i++] != ',') return false;
        if (!num(s, i, 3, c)) return false;
        if (i == s.size()) return true;
        if (s[i++] != ';') return false;
    }
    return false;
}

bool validAnim(const std::string &s) {
    if (s.empty()) return true;
    size_t i = 0;
    long g, f;
    if (s.size() < 6 || (s[0] != '0' && s[0] != '1') || s[1] != ',') return false;
    i = 2;
    if (!num(s, i, 2, g) || g > 31 || i >= s.size() || s[i++] != ',') return false;
    if (!num(s, i, 1, f) || f > 2 || i >= s.size() || s[i++] != ',') return false;
    size_t n = s.size() - i;
    if (n % 2 || n > 32) return false;
    for (; i < s.size(); i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}

bool validAlertsToken(const std::string &s) {
    if (s.empty()) return true;
    if (s.size() < 16 || s.size() > 64) return false;
    for (char c : s)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
