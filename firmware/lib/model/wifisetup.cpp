#include "wifisetup.h"

bool validCreds(const std::string &ssid, const std::string &pass) {
    if (ssid.empty() || ssid.size() > 32) return false;
    if (!pass.empty() && (pass.size() < 8 || pass.size() > 63)) return false;
    for (unsigned char c : ssid) if (c < 0x20 || c == 0x7F) return false;
    for (unsigned char c : pass) if (c < 0x20 || c == 0x7F) return false;
    return true;
}

static std::string qrEscape(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') o += '\\';
        o += c;
    }
    return o;
}

std::string wifiQr(const std::string &ssid, const std::string &pass) {
    if (pass.empty()) return "WIFI:T:nopass;S:" + qrEscape(ssid) + ";;";
    return "WIFI:T:WPA;S:" + qrEscape(ssid) + ";P:" + qrEscape(pass) + ";;";
}

std::string apPassword(uint32_t chipId, const char *secret) {
    static const char ALPHA[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o, 1/l/i
    uint32_t h = 2166136261u;                                        // FNV-1a
    auto mix = [&h](uint8_t b) { h ^= b; h *= 16777619u; };
    for (int i = 0; i < 4; i++) mix(uint8_t(chipId >> (8 * i)));
    for (const char *p = secret; p && *p; p++) mix(uint8_t(*p));
    std::string o;
    for (int i = 0; i < 8; i++) {
        mix(uint8_t(i));
        o += ALPHA[h % (sizeof ALPHA - 1)];
    }
    return o;
}

uint8_t nextCrashCount(uint8_t prev, bool prevValid, int resetReason) {
    bool crash = resetReason == 1 || resetReason == 2 || resetReason == 3;  // hw wdt, exception, soft wdt
    if (!crash) return 0;
    if (!prevValid) return 1;
    return prev < 250 ? prev + 1 : prev;
}
