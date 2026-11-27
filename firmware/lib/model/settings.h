// Per-device settings kept in their own raw A/B flash sectors (record magic "CCS1", payload "key=value\n"),
// and pairing a computer with the code shown on the clock screen.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

extern const char SETTINGS_MAGIC[5];  // "CCS1"
constexpr size_t SETTINGS_REC_MAX = 768;  // older records (<= 512) still load: each one carries its length
constexpr const char *PAIR_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";  // no 0/O, 1/I/L

struct Settings {
    std::string token;           // API token (32 chars), made on first boot
    std::string tz;              // POSIX TZ string; empty = default (Kyiv)
    int birthMonth = 0, birthDay = 0;  // 0 = no birthday scene
    std::string lang;            // "en" / "uk"; empty = English
    std::string owm;             // OpenWeather API key (32 hex); empty = no weather
    std::string loc;             // "lat,lon" for the weather
    std::string region;          // air-raid alert region (ubilling name, e.g. "м. Київ"); empty = alerts off
    std::string today;           // "day,busyMs,answers,commits": today-with-Claude counters (survive power loss)
    std::string night;           // sleep hours "23-7", "off" = never; empty = default 23-7
    std::string bright;          // brightness % "day/night", e.g. "80/20"; empty = default
    std::string face;            // big clock style "0".."6"; empty = 0
    std::string pin;             // pinned idle scene "name,untilEpoch" (0 = always); empty = random scenes
    std::string week;            // last 8 days of today-counters (WeekLog::encode)
    std::string anim;            // animation settings (AnimPrefs::encode); empty = defaults
    std::string atok;            // alerts.in.ua token (alert levels, fetched by the computer); empty = none
};

std::vector<uint8_t> encodeSettingsRec(uint32_t seq, const Settings &s);
// Invalid values inside a valid record are dropped (field left empty), the record itself still loads.
bool decodeSettingsRec(const uint8_t *buf, size_t n, uint32_t &seq, Settings &out);

bool validTz(const std::string &tz);        // 1..48 chars of [A-Za-z0-9,.:/+<>-]
bool validToken(const std::string &t);      // 16..64 chars of [A-Za-z0-9]
bool validOwmKey(const std::string &k);   // 32 chars of [0-9a-f]
bool validLoc(const std::string &l);      // "lat,lon", decimals, in range
bool validRegion(const std::string &r);   // 1..80 bytes, no control chars, quotes or backslashes
bool parseToday(const std::string &s, int &day, uint32_t &busyMs, int &answers, int &commits);
bool parseBirthday(const std::string &s, int &month, int &day);  // "MM-DD" or "" (off)
bool parseNight(const std::string &s, int &from, int &to);        // "" -> 23-7, "off" -> from == to (never)
bool parseBright(const std::string &s, int &day, int &night);     // "" -> 80/20; each 5..100
bool validFace(const std::string &s);  // "" or "0".."6": classic, minimal, pixel, segment, flip, terminal, binary
bool parsePin(const std::string &s, std::string &name, uint32_t &until);  // "" (none) or "name,untilEpoch"
bool validWeek(const std::string &s);
bool validAnim(const std::string &s);       // "" or "0|1,groups 0..31,freq 0..2,hex" (hex <= 32 chars, even)
bool validAlertsToken(const std::string &s);  // "" or 16..64 of [A-Za-z0-9_-]

std::string makeToken(uint32_t (*rnd)());
std::string makePairCode(uint32_t (*rnd)());

enum class PairResult { Ok, Wrong, Closed, Locked };

// The code is valid for a while after boot and for one successful pairing; too many wrong tries lock it
// until the next reboot (6 chars of a 31-letter alphabet: guessing is hopeless with 5 tries).
class PairWindow {
  public:
    static constexpr int MAX_TRIES = 5;
    void open(const std::string &code, uint32_t nowMs, uint32_t forMs);
    void close() { open_ = false; }
    bool isOpen(uint32_t nowMs) const;
    bool locked() const { return wrong_ >= MAX_TRIES; }
    const std::string &code() const { return code_; }
    PairResult check(const std::string &code, uint32_t nowMs);

  private:
    std::string code_;
    uint32_t start_ = 0, len_ = 0;
    int wrong_ = 0;
    bool open_ = false;
};
