// Wi-Fi setup mode (pure logic): when to open the "Claude-Clock" access point with a QR code,
// credential validation, QR payload, per-device AP password, crash-loop guard.
#pragma once
#include <stdint.h>
#include <string>

class NetPolicy {
  public:
    static constexpr int FAILS_TO_SETUP = 5;             // failed join attempts in a row -> setup mode
    static constexpr uint32_t ATTEMPT_MS = 30000;        // one join attempt
    static constexpr uint32_t SETUP_RETRY_MS = 120000;   // setup mode: retry the old network this often

    void onAttemptFailed() { if (fails_ < 1000) fails_++; }
    void onConnected() { fails_ = 0; }
    void forceSetup() { if (fails_ < FAILS_TO_SETUP) fails_ = FAILS_TO_SETUP; }  // no network known at all
    bool setupMode() const { return fails_ >= FAILS_TO_SETUP; }
    int fails() const { return fails_; }
    // In setup mode a join attempt makes the radio hop channels and drops the phone, so only retry
    // the old network when nobody is connected to the access point.
    bool shouldRetry(uint32_t nowMs, uint32_t lastBeginMs, int apClients) const {
        if (setupMode()) return apClients == 0 && nowMs - lastBeginMs >= SETUP_RETRY_MS;
        return nowMs - lastBeginMs >= ATTEMPT_MS;
    }

  private:
    int fails_ = 0;
};

// SSID 1..32 bytes, password empty (open) or 8..63, no control characters.
bool validCreds(const std::string &ssid, const std::string &pass);
// Standard "join Wi-Fi" QR payload understood by iOS / Android cameras.
std::string wifiQr(const std::string &ssid, const std::string &pass);
// 8 readable chars derived from chip id + secret token: stable per device, nothing stored in flash.
std::string apPassword(uint32_t chipId, const char *secret);

// Crash-loop guard: count consecutive crash resets (kept in RTC memory across resets).
uint8_t nextCrashCount(uint8_t prev, bool prevValid, int resetReason);
inline bool safeMode(uint8_t crashCount) { return crashCount >= 3; }
