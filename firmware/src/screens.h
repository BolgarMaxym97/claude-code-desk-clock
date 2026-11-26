// Renders the Model's View to the display. Full redraw on screen change, then only
// changed text lines and the animated sprite regions (no full-screen buffer: ~16 KB heap).
#pragma once
#include <time.h>
#include "model.h"

enum : uint8_t { TEST_IDLE, TEST_RUNNING, TEST_OK, TEST_OK_UNSAVED, TEST_FAIL };

struct NetInfo {
    bool connected;
    int wifiStatus;       // WiFi.status() code, shown while time is unknown
    uint32_t attempts;    // Wi-Fi connect attempts
    bool apOn;            // setup mode: access point + captive portal are up
    const char *ssid;     // network we are joining
    const char *apSsid;   // setup access point name
    const char *apPass;   // its password (derived per device)
    int fails;            // failed joins in a row
    uint8_t test;         // TEST_* state of credentials sent from the setup page
    uint8_t testReason;   // TEST_FAIL: 1 wrong password, 2 network not found, 3 other
    const char *testSsid;
};

void screensBegin();
// Preview the New Year look on the device for a while (season; with midnight=true also fireworks + year).
void screensPreviewNewYear(uint32_t forMs, bool midnight);
// Preview scenes on the clock screen: one by name, or all idle scenes one after another (name == nullptr).
bool screensPreviewScene(const char *name);
// frame=true on every animation step (config.h FRAME_MS).
void screensUpdate(const Model &m, uint32_t nowMs, const LocalTime &lt, time_t epoch, const NetInfo &net, bool frame,
                   uint32_t tick);
