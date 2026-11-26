// Records in raw flash sectors (A/B pairs) at the erased tail of the stock data area.
// Record: magic[4] | seq u32 | len u16 | payload | crc32 (over seq..payload) | 0xFF pad to 4 bytes.
// Wi-Fi record: magic "CCK1", payload ssid \0 pass. Settings record: see settings.h.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

struct StoreRec {
    uint32_t seq;
    std::string ssid, pass;
};

extern const char WIFI_MAGIC[5];  // "CCK1"
std::vector<uint8_t> encodeBlob(const char *magic, uint32_t seq, const std::string &payload);
bool decodeBlob(const char *magic, const uint8_t *buf, size_t n, size_t maxLen, uint32_t &seq, std::string &payload);
std::vector<uint8_t> encodeRec(const StoreRec &r);
bool decodeRec(const uint8_t *buf, size_t n, StoreRec &out);  // false: erased / corrupt / invalid creds
// A sector we may use is fully erased or starts with our record; anything else is someone else's data.
bool sectorForeign(const uint8_t *sector, size_t n, const char *magic = WIFI_MAGIC);
int pickNewest(bool validA, uint32_t seqA, bool validB, uint32_t seqB);  // -1 none, 0 A, 1 B
inline int writeTarget(int newest) { return newest == 0 ? 1 : 0; }
