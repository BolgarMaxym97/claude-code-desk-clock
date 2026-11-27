#include "store.h"
#include "rawstore.h"

// Sectors at the very end of the stock data area (0x200000..0x3FA000). The stock firmware keeps raw images at
// its start; 0x3F0000..0x3F9FFF were verified erased on the device. Bootloader, firmware, EEPROM (0x3FA000) and
// SDK sectors are never touched.
extern "C" uint32_t _FS_start, _FS_end;
static const uint32_t SEC_SIZE = 0x1000;

struct Pair {
    uint32_t a, b;
    const char *magic;
    size_t recMax;
    bool usable = false;
    int newest = -1;
    uint32_t seq = 0;
};
static Pair wifi{0x3F8000, 0x3F9000, WIFI_MAGIC, 128};
static Pair conf{0x3F6000, 0x3F7000, SETTINGS_MAGIC, SETTINGS_REC_MAX};
static bool readOnly = false;

static bool readBlob(const Pair &p, uint32_t addr, uint8_t *buf) {
    return ESP.flashRead(addr, reinterpret_cast<uint32_t *>(buf), p.recMax);
}

static bool sectorOk(uint32_t addr, const char *magic) {  // erased or ours; read in chunks to keep the stack small
    uint32_t buf[64];
    for (uint32_t off = 0; off < SEC_SIZE; off += sizeof buf) {
        if (!ESP.flashRead(addr + off, buf, sizeof buf)) return false;
        const uint8_t *b = reinterpret_cast<uint8_t *>(buf);
        if (off == 0) { if (sectorForeign(b, sizeof buf, magic)) return false; if (b[0] == 'C') return true; }
        else for (size_t i = 0; i < sizeof buf; i++) if (b[i] != 0xFF) return false;
    }
    return true;
}

static bool seqOf(const Pair &p, uint32_t addr, uint32_t &seq) {
    alignas(4) uint8_t buf[SETTINGS_REC_MAX];
    std::string payload;
    return readBlob(p, addr, buf) && decodeBlob(p.magic, buf, p.recMax, p.recMax - 14, seq, payload);
}

static void beginPair(Pair &p, uint32_t fsStart, uint32_t fsEnd) {
    p.usable = p.a >= fsStart && p.b + SEC_SIZE <= fsEnd && sectorOk(p.a, p.magic) && sectorOk(p.b, p.magic);
    if (!p.usable) return;
    uint32_t sa = 0, sb = 0;
    bool va = seqOf(p, p.a, sa), vb = seqOf(p, p.b, sb);
    p.newest = pickNewest(va, sa, vb, sb);
    p.seq = p.newest == 0 ? sa : p.newest == 1 ? sb : 0;
}

// Writes bytes into the older sector, verifies by read-back, then makes it the newest.
static bool writePair(Pair &p, const std::vector<uint8_t> &bytes) {
    if (!p.usable || readOnly || bytes.empty() || bytes.size() > p.recMax) return false;
    int target = writeTarget(p.newest);
    uint32_t addr = target == 0 ? p.a : p.b;  // the other sector keeps the previous record
    if (!ESP.flashEraseSector(addr / SEC_SIZE)) return false;
    if (!ESP.flashWrite(addr, reinterpret_cast<const uint32_t *>(bytes.data()), bytes.size())) return false;
    uint32_t back[16];  // read-back in 64-byte chunks: no record-sized buffer on the stack
    for (size_t off = 0; off < bytes.size(); off += sizeof back) {
        size_t n = std::min(sizeof back, bytes.size() - off);
        if (!ESP.flashRead(addr + off, back, sizeof back) || memcmp(back, bytes.data() + off, n) != 0) return false;
    }
    p.newest = target;
    p.seq++;
    return true;
}

bool storeBegin(bool ro) {
    readOnly = ro;
    uint32_t fsStart = uint32_t(&_FS_start) - 0x40200000, fsEnd = uint32_t(&_FS_end) - 0x40200000;
    beginPair(wifi, fsStart, fsEnd);
    beginPair(conf, fsStart, fsEnd);
    return wifi.usable;
}

bool storeOk() { return wifi.usable; }
bool storeSettingsOk() { return conf.usable; }

bool storeLoad(String &ssid, String &pass) {
    if (!wifi.usable || wifi.newest < 0) return false;
    alignas(4) uint8_t buf[SETTINGS_REC_MAX];
    StoreRec r;
    if (!readBlob(wifi, wifi.newest == 0 ? wifi.a : wifi.b, buf) || !decodeRec(buf, wifi.recMax, r)) return false;
    ssid = r.ssid.c_str();
    pass = r.pass.c_str();
    return true;
}

bool storeSave(const String &ssid, const String &pass) {
    return writePair(wifi, encodeRec(StoreRec{wifi.seq + 1, ssid.c_str(), pass.c_str()}));
}

bool storeLoadSettings(Settings &s) {
    if (!conf.usable || conf.newest < 0) return false;
    alignas(4) uint8_t buf[SETTINGS_REC_MAX];
    uint32_t seq;
    return readBlob(conf, conf.newest == 0 ? conf.a : conf.b, buf) && decodeSettingsRec(buf, conf.recMax, seq, s);
}

bool storeSaveSettings(const Settings &s) { return writePair(conf, encodeSettingsRec(conf.seq + 1, s)); }
