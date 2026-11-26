#include "rawstore.h"
#include <algorithm>
#include "wifisetup.h"

const char WIFI_MAGIC[5] = "CCK1";

static uint32_t crc32(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void put32(std::vector<uint8_t> &b, uint32_t v) { for (int i = 0; i < 4; i++) b.push_back(uint8_t(v >> (8 * i))); }
static uint32_t get32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

std::vector<uint8_t> encodeBlob(const char *magic, uint32_t seq, const std::string &payload) {
    std::vector<uint8_t> b(magic, magic + 4);
    put32(b, seq);
    uint16_t len = uint16_t(payload.size());
    b.push_back(uint8_t(len));
    b.push_back(uint8_t(len >> 8));
    b.insert(b.end(), payload.begin(), payload.end());
    put32(b, crc32(b.data() + 4, b.size() - 4));
    while (b.size() % 4) b.push_back(0xFF);
    return b;
}

bool decodeBlob(const char *magic, const uint8_t *buf, size_t n, size_t maxLen, uint32_t &seq, std::string &payload) {
    if (n < 14 || !std::equal(magic, magic + 4, reinterpret_cast<const char *>(buf))) return false;
    size_t len = buf[8] | size_t(buf[9]) << 8;
    if (len > maxLen || 10 + len + 4 > n) return false;
    if (crc32(buf + 4, 6 + len) != get32(buf + 10 + len)) return false;
    seq = get32(buf + 4);
    payload.assign(reinterpret_cast<const char *>(buf + 10), len);
    return true;
}

std::vector<uint8_t> encodeRec(const StoreRec &r) {
    std::string payload = r.ssid;
    payload += '\0';
    payload += r.pass;
    return encodeBlob(WIFI_MAGIC, r.seq, payload);
}

bool decodeRec(const uint8_t *buf, size_t n, StoreRec &out) {
    uint32_t seq;
    std::string payload;
    if (!decodeBlob(WIFI_MAGIC, buf, n, 32 + 1 + 63, seq, payload) || payload.size() < 2) return false;
    size_t z = payload.find('\0');
    if (z == std::string::npos) return false;
    StoreRec r{seq, payload.substr(0, z), payload.substr(z + 1)};
    if (!validCreds(r.ssid, r.pass)) return false;
    out = r;
    return true;
}

bool sectorForeign(const uint8_t *sector, size_t n, const char *magic) {
    if (n >= 4 && std::equal(magic, magic + 4, reinterpret_cast<const char *>(sector))) return false;  // ours (even if the CRC is broken)
    for (size_t i = 0; i < n; i++) if (sector[i] != 0xFF) return true;
    return false;
}

int pickNewest(bool validA, uint32_t seqA, bool validB, uint32_t seqB) {
    if (validA && validB) return seqB > seqA ? 1 : 0;
    if (validA) return 0;
    if (validB) return 1;
    return -1;
}
