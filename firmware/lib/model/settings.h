// Per-device settings kept in their own raw A/B flash sectors (record magic "CCS1", payload "key=value\n"),
// and pairing a computer with the code shown on the clock screen.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

extern const char SETTINGS_MAGIC[5];  // "CCS1"
constexpr size_t SETTINGS_REC_MAX = 256;
constexpr const char *PAIR_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";  // no 0/O, 1/I/L

struct Settings {
    std::string token;           // API token (32 chars), made on first boot
    std::string tz;              // POSIX TZ string; empty = default (Kyiv)
    int birthMonth = 0, birthDay = 0;  // 0 = no birthday scene
    std::string lang;            // "en" / "uk"; empty = English
};

std::vector<uint8_t> encodeSettingsRec(uint32_t seq, const Settings &s);
// Invalid values inside a valid record are dropped (field left empty), the record itself still loads.
bool decodeSettingsRec(const uint8_t *buf, size_t n, uint32_t &seq, Settings &out);

bool validTz(const std::string &tz);        // 1..48 chars of [A-Za-z0-9,.:/+<>-]
bool validToken(const std::string &t);      // 16..64 chars of [A-Za-z0-9]
bool parseBirthday(const std::string &s, int &month, int &day);  // "MM-DD" or "" (off)

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
