// Raw A/B flash records at the erased tail of the stock data area:
//   Wi-Fi (setup page)          0x3F8000 / 0x3F9000, magic "CCK1"
//   settings (token, tz, birth) 0x3F6000 / 0x3F7000, magic "CCS1"
// Safety: a pair is used only if both sectors are erased or hold our record (checked at boot); a new record
// goes to the other sector and is verified by read-back, so a power cut keeps the previous one. On any error
// the clock runs without saving. Read-only mode (crash-loop safe boot) never erases or writes.
#pragma once
#include <Arduino.h>
#include "settings.h"

bool storeBegin(bool readOnly);
bool storeOk();          // Wi-Fi pair usable
bool storeSettingsOk();  // settings pair usable
bool storeLoad(String &ssid, String &pass);
bool storeSave(const String &ssid, const String &pass);
bool storeLoadSettings(Settings &s);
bool storeSaveSettings(const Settings &s);
