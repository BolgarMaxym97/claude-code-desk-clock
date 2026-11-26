// Per-device identity and user settings: API token, timezone, birthday, pairing a computer.
// Release builds have no secrets compiled in: the token is made on first boot and kept in flash (store.h);
// a computer gets it once by sending the code shown on the clock screen (POST /api/pair).
#pragma once
#include <Arduino.h>

void deviceBegin();
const char *deviceToken();
const char *deviceTz();                 // POSIX TZ (default: Kyiv)
void deviceBirthday(int &month, int &day);
bool deviceSetTz(const String &tz);     // false: invalid
bool deviceSetBirthday(const String &mmdd);  // "MM-DD" or "" (off); false: invalid
bool deviceSetLang(const String &code);      // "en" / "uk"; false: unknown
const char *deviceLang();
// Weather / air-raid alert sources ("" = off). The OpenWeather key never leaves the device (/status says only "set").
bool deviceSetOwm(const String &key);     // 32 hex or ""
bool deviceSetLoc(const String &latLon);  // "lat,lon" or ""
bool deviceSetRegion(const String &name); // ubilling region name or "" (alerts off)
const char *deviceOwm();
const char *deviceLoc();
const char *deviceRegion();
// Today-with-Claude counters in the settings record ("day,busyMs,answers,commits"), written now and then.
const char *deviceToday();
void deviceSaveToday(const String &v);
bool deviceSettingsSaved();             // false: kept in RAM only (flash area unusable)

// Pairing: start shows a fresh code on the screen for a few minutes.
bool devicePairStart(uint32_t nowMs);   // false: locked (too many wrong codes) until reboot
bool devicePairShowing(uint32_t nowMs);
const char *devicePairCode();
int devicePairCheck(const String &code, uint32_t nowMs);  // 0 ok, 1 wrong, 2 closed, 3 locked
