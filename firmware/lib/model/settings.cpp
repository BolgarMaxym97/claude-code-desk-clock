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
