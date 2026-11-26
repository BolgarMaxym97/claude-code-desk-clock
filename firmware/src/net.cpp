#include "net.h"
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <time.h>
#include "config.h"
#include "device.h"
#include "store.h"
#include "wifisetup.h"

const char *AP_SSID = "Claude-Clock";
static const IPAddress AP_IP(192, 168, 4, 1);
static const uint32_t STA_GIVE_UP_MS = 20000;  // setup mode: stop the join attempt (and channel hopping) after this
static const uint32_t TEST_MS = 30000;         // checking new credentials from the setup page
static const uint32_t OK_LINGER_MS = 15000;    // keep the AP after success so the phone can read the result

static NetPolicy policy;
static DNSServer dns;
static String savedSsid, savedPass, apPass, testSsid, testPass;
static bool hasSaved = false, apOn = false, wasConnected = false, mdnsUp = false, staActive = false;
static uint32_t lastBegin = 0, attempts = 0, cand = 0, testStart = 0, okAt = 0;
static uint8_t testState = TEST_IDLE, testReason = 0;
static bool usingSaved = false;
static uint32_t forcedUntil = 0;  // setup opened on request while still online
static bool simulateBad = false;
static ScanItem scanList[NET_SCAN_MAX];
static int scanCount = 0;
static bool scanBusy = false;

static void copyScan(int n) {  // keep strongest unique names
    scanCount = 0;
    for (int i = 0; i < n && scanCount < NET_SCAN_MAX; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;
        bool dup = false;
        for (int k = 0; k < scanCount; k++)
            if (scanList[k].ssid == s) {
                dup = true;
                if (WiFi.RSSI(i) > scanList[k].rssi) { scanList[k].rssi = WiFi.RSSI(i); scanList[k].channel = WiFi.channel(i); }
            }
        if (!dup) scanList[scanCount++] = ScanItem{s, WiFi.RSSI(i), WiFi.encryptionType(i) != ENC_TYPE_NONE, WiFi.channel(i)};
    }
    WiFi.scanDelete();
}   // test: built-in network gets a wrong password (RAM only, until reboot)

static void beginStatic() {
#ifdef CLOCK_IP
    WiFi.config(IPAddress(CLOCK_IP), IPAddress(CLOCK_GW), IPAddress(CLOCK_MASK), IPAddress(CLOCK_DNS));
#endif
}

static void beginDhcp() { WiFi.config(0U, 0U, 0U); }

static bool hasBuiltin() { return WIFI_SSID[0] != 0; }

// Alternate between the saved network (setup page, DHCP) and the built-in one from secrets.h (static IP).
// Release builds have no built-in network: only the saved one; nothing saved -> setup mode right away.
static void begin() {
    if (!hasBuiltin() && !hasSaved) { policy.forceSetup(); attempts++; return; }
    usingSaved = hasSaved && (!hasBuiltin() || (savedSsid != WIFI_SSID && (cand++ % 2 == 0)));
    if (usingSaved) { beginDhcp(); WiFi.begin(savedSsid.c_str(), savedPass.c_str()); }
    else if (hasSaved && savedSsid == WIFI_SSID) { beginStatic(); WiFi.begin(WIFI_SSID, savedPass.c_str()); }
    else { beginStatic(); WiFi.begin(WIFI_SSID, simulateBad ? "wrong-password-test" : WIFI_PASS); }
    attempts++;
    staActive = true;
}

static void stopSta() {
    if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();  // RAM config only (persistent=false)
    staActive = false;
}

// ESP8266 has one radio: with the AP up, the station can only join a network on the AP's channel.
static int channelOf(const String &ssid) {
    for (int k = 0; k < scanCount; k++) if (scanList[k].ssid == ssid) return scanList[k].channel;
    return 0;
}
static int apChannel = 1;

static void enterSetup() {
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
    copyScan(WiFi.scanNetworks(false));  // ~2 s, before the AP exists: nobody gets dropped
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    int ch = channelOf(hasSaved ? savedSsid : String(WIFI_SSID));
    if (ch < 1 && hasSaved && hasBuiltin()) ch = channelOf(WIFI_SSID);
    apChannel = ch >= 1 && ch <= 13 ? ch : 1;  // same channel as the home network -> retries can work
    WiFi.softAP(AP_SSID, apPass.c_str(), apChannel);
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", AP_IP);  // captive portal: every name -> the clock
    apOn = true;
}

static void leaveSetup() {
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    apOn = false;
    testState = TEST_IDLE;
}

void netBegin() {
    apPass = apPassword(ESP.getChipId(), deviceToken()).c_str();
    hasSaved = storeLoad(savedSsid, savedPass);
    WiFi.persistent(false);            // never write SDK config sectors
    WiFi.mode(WIFI_OFF);               // clean radio state after OTA / soft reset
    delay(200);
    WiFi.mode(WIFI_STA);               // STA only: a permanent softAP breaks reconnects
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.setAutoReconnect(true);
    WiFi.hostname("claude-clock");
    begin();
    lastBegin = millis();
    if (policy.setupMode()) enterSetup();  // fresh clock: no network known
}

void netTryCreds(const String &ssid, const String &pass) {
    testSsid = ssid;
    testPass = pass;
    testState = TEST_RUNNING;
    testReason = 0;
    testStart = millis();
    int ch = channelOf(ssid);
    if (ch >= 1 && ch <= 13 && ch != apChannel) {  // move the AP to the network's channel (phone rejoins)
        apChannel = ch;
        WiFi.softAP(AP_SSID, apPass.c_str(), apChannel);
    }
    if (hasBuiltin() && ssid == WIFI_SSID) beginStatic();  // same network as built in: static IP (this router's DHCP is flaky)
    else beginDhcp();
    WiFi.begin(ssid.c_str(), pass.c_str());
    staActive = true;
    attempts++;
}

int netScan(const ScanItem *&items) { items = scanList; return scanCount; }
bool netScanBusy() { return scanBusy; }
void netScanRefresh() {
    if (scanBusy || testState == TEST_RUNNING) return;
    WiFi.scanNetworks(true);  // async; the phone may blink off the AP for a moment
    scanBusy = true;
}

static void tickTest(uint32_t now) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
        bool saved = storeSave(testSsid, testPass);
        savedSsid = testSsid;
        savedPass = testPass;
        hasSaved = true;
        usingSaved = true;
        testState = saved ? TEST_OK : TEST_OK_UNSAVED;
        okAt = now;
        testPass = "";
        return;
    }
    bool early = now - testStart > 6000 && (st == WL_WRONG_PASSWORD || st == WL_NO_SSID_AVAIL);
    if (early || now - testStart > TEST_MS) {
        testReason = st == WL_WRONG_PASSWORD ? 1 : st == WL_NO_SSID_AVAIL ? 2 : 3;
        testState = TEST_FAIL;
        testPass = "";
        stopSta();
        lastBegin = now;
    }
}

void netTick(uint32_t now) {
    if (mdnsUp) MDNS.update();
    if (apOn) dns.processNextRequest();
    if (scanBusy) { int n = WiFi.scanComplete(); if (n >= 0) { copyScan(n); scanBusy = false; } else if (n == WIFI_SCAN_FAILED) scanBusy = false; }
    if (testState == TEST_RUNNING) { tickTest(now); return; }
    if (testState == TEST_FAIL && now - lastBegin > 30000) testState = TEST_IDLE;  // back to the QR pages

    if (WiFi.status() == WL_CONNECTED) {
        if (!wasConnected) {
            wasConnected = true;
            policy.onConnected();
            configTime(deviceTz(), "pool.ntp.org", "time.google.com");  // resync on every (re)connect
            if (!mdnsUp && MDNS.begin("claude-clock")) { MDNS.addService("http", "tcp", 80); mdnsUp = true; }
        }
        bool linger = (testState == TEST_OK || testState == TEST_OK_UNSAVED) && now - okAt < OK_LINGER_MS;
        bool forced = forcedUntil && int32_t(forcedUntil - now) > 0 && testState != TEST_OK && testState != TEST_OK_UNSAVED;
        if (apOn && !linger && !forced) { forcedUntil = 0; leaveSetup(); }
        return;
    }
    wasConnected = false;
    int clients = apOn ? WiFi.softAPgetStationNum() : 0;
    if (apOn && staActive && (clients > 0 || now - lastBegin > STA_GIVE_UP_MS)) stopSta();  // keep the AP stable
    if (policy.shouldRetry(now, lastBegin, clients)) {
        policy.onAttemptFailed();  // the previous attempt did not connect
        if (policy.setupMode() && !apOn) enterSetup();
        begin();
        lastBegin = now;
    }
}

bool netSetupMode() { return apOn; }

void netSimulateBadWifi() {
    simulateBad = true;
    WiFi.disconnect();
    begin();
    lastBegin = millis();
}

void netOpenSetup(uint32_t forMs) {
    forcedUntil = (millis() + forMs) | 1;
    if (!apOn) enterSetup();
}
bool netIsApClient(const IPAddress &ip) { return apOn && ip[0] == 192 && ip[1] == 168 && ip[2] == 4; }

NetInfo netInfo() {
    NetInfo n{};
    n.connected = WiFi.status() == WL_CONNECTED;
    n.wifiStatus = int(WiFi.status());
    n.attempts = attempts;
    n.apOn = apOn;
    n.ssid = (usingSaved || !hasBuiltin()) && hasSaved ? savedSsid.c_str() : WIFI_SSID;
    n.apSsid = AP_SSID;
    n.apPass = apPass.c_str();
    n.fails = policy.fails();
    n.test = testState;
    n.testReason = testReason;
    n.testSsid = testSsid.c_str();
    return n;
}

void netApplyTz() {
    setenv("TZ", deviceTz(), 1);
    tzset();
}
