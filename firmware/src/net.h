// Wi-Fi manager + NTP + mDNS + setup mode.
// After NetPolicy::FAILS_TO_SETUP failed joins the clock opens AP "Claude-Clock" (captive portal, QR on screen).
#pragma once
#include <ESP8266WiFi.h>
#include <stdint.h>
#include "screens.h"

void netBegin();
void netApplyTz();                                          // after the timezone setting changed
void netTick(uint32_t nowMs);
NetInfo netInfo();
bool netSetupMode();
void netSimulateBadWifi();                                 // test only: wrong built-in password until reboot
void netOpenSetup(uint32_t forMs);                         // open setup AP on request (stays online)
bool netIsApClient(const IPAddress &ip);                    // request came from a phone on the setup AP
void netTryCreds(const String &ssid, const String &pass);   // from the setup page; result in NetInfo.test

constexpr int NET_SCAN_MAX = 15;
struct ScanItem { String ssid; int rssi; bool lock; int channel; };
int netScan(const ScanItem *&items);  // networks found when setup mode started (or last refresh)
bool netScanBusy();
void netScanRefresh();
