#include "device.h"
#include "config.h"
#include "settings.h"
#include "store.h"

static const char *TZ_DEFAULT = "EET-2EEST,M3.5.0/3,M10.5.0/4";  // Europe/Kyiv
static const uint32_t PAIR_MS = 300000;
static Settings cur;
static bool saved = false;
static PairWindow pair;

static uint32_t hwRandom() { return ESP.random() ^ (micros() * 2654435761u); }  // hardware RNG (+ timing jitter)

void deviceBegin() {
    storeLoadSettings(cur);
#ifdef CLOCK_TOKEN
    cur.token = CLOCK_TOKEN;  // developer build with secrets.h: keep the token the computer already has
    saved = true;
#else
    if (cur.token.empty()) {
        cur.token = makeToken(hwRandom);
        saved = storeSaveSettings(cur);
    } else saved = true;
#endif
}

const char *deviceToken() { return cur.token.c_str(); }
const char *deviceTz() { return cur.tz.empty() ? TZ_DEFAULT : cur.tz.c_str(); }

void deviceBirthday(int &month, int &day) {
    if (cur.birthMonth) { month = cur.birthMonth; day = cur.birthDay; }
    else { month = BIRTH_MONTH; day = BIRTH_DAY; }
}

bool deviceSetTz(const String &tz) {
    if (!validTz(tz.c_str())) return false;
    cur.tz = tz.c_str();
    saved = storeSaveSettings(cur);
    return true;
}

bool deviceSetBirthday(const String &mmdd) {
    int m, d;
    if (!parseBirthday(mmdd.c_str(), m, d)) return false;
    cur.birthMonth = m;
    cur.birthDay = d;
    saved = storeSaveSettings(cur);
    return true;
}

bool deviceSettingsSaved() { return saved; }

bool devicePairStart(uint32_t now) {
    if (pair.locked()) return false;
    pair.open(makePairCode(hwRandom), now, PAIR_MS);
    return true;
}

bool devicePairShowing(uint32_t now) { return pair.isOpen(now); }
const char *devicePairCode() { return pair.code().c_str(); }

int devicePairCheck(const String &code, uint32_t now) {
    switch (pair.check(code.c_str(), now)) {
        case PairResult::Ok: return 0;
        case PairResult::Wrong: return 1;
        case PairResult::Closed: return 2;
        default: return 3;
    }
}
