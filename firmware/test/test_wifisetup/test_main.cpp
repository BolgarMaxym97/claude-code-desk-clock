// Wi-Fi setup mode: when to show it, QR payload, credential checks, AP password, crash-loop guard.
#include <unity.h>
#include <cstring>
#include "wifisetup.h"

void setUp() {}
void tearDown() {}

void test_five_failed_attempts_enter_setup_and_connect_leaves_it() {
    NetPolicy p;
    for (int i = 0; i < NetPolicy::FAILS_TO_SETUP - 1; i++) p.onAttemptFailed();
    TEST_ASSERT_FALSE(p.setupMode());
    p.onAttemptFailed();
    TEST_ASSERT_TRUE(p.setupMode());
    p.onConnected();
    TEST_ASSERT_FALSE(p.setupMode());
    TEST_ASSERT_EQUAL(0, p.fails());
}

void test_setup_retries_old_network_only_without_phone_and_every_2_min() {
    NetPolicy p;
    for (int i = 0; i < NetPolicy::FAILS_TO_SETUP; i++) p.onAttemptFailed();
    TEST_ASSERT_FALSE(p.shouldRetry(100000, 0, 1));                  // phone on the AP: never hop channels
    TEST_ASSERT_FALSE(p.shouldRetry(100000, 0, 0));                  // < 2 min since last try
    TEST_ASSERT_TRUE(p.shouldRetry(NetPolicy::SETUP_RETRY_MS, 0, 0));
    NetPolicy n;                                                     // normal mode: every 30 s
    TEST_ASSERT_FALSE(n.shouldRetry(NetPolicy::ATTEMPT_MS - 1, 0, 0));
    TEST_ASSERT_TRUE(n.shouldRetry(NetPolicy::ATTEMPT_MS, 0, 0));
}

void test_no_network_known_at_all_opens_setup_at_once() {
    NetPolicy p;
    p.forceSetup();  // fresh clock: nothing saved, nothing built in
    TEST_ASSERT_TRUE(p.setupMode());
    p.onConnected();
    TEST_ASSERT_FALSE(p.setupMode());
}

void test_valid_creds() {
    TEST_ASSERT_TRUE(validCreds("Home", "12345678"));
    TEST_ASSERT_TRUE(validCreds("Open cafe", ""));                    // open network
    TEST_ASSERT_TRUE(validCreds("Дім", std::string(63, 'x')));
    TEST_ASSERT_FALSE(validCreds("", "12345678"));
    TEST_ASSERT_FALSE(validCreds(std::string(33, 'a'), "12345678"));  // SSID max 32 bytes
    TEST_ASSERT_FALSE(validCreds("Home", "1234567"));                 // WPA: 8..63
    TEST_ASSERT_FALSE(validCreds("Home", std::string(64, 'x')));
    TEST_ASSERT_FALSE(validCreds("Ho\nme", "12345678"));              // control chars
}

void test_wifi_qr_escapes_special_chars() {
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:Claude-Clock;P:abc;;", wifiQr("Claude-Clock", "abc").c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b\\,c\\:d\\\\e\\\"f;P:p\\;;;", wifiQr("a;b,c:d\\e\"f", "p;").c_str());
    TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:x;;", wifiQr("x", "").c_str());
}

void test_ap_password_is_stable_readable_and_per_device() {
    std::string a = apPassword(0x1BE1FB, "tok"), b = apPassword(0x1BE1FB, "tok"), c = apPassword(0x1BE1FC, "tok");
    TEST_ASSERT_EQUAL(8, int(a.size()));
    TEST_ASSERT_EQUAL_STRING(a.c_str(), b.c_str());
    TEST_ASSERT_TRUE(a != c);
    for (char ch : a) TEST_ASSERT_NOT_NULL_MESSAGE(strchr("abcdefghjkmnpqrstuvwxyz23456789", ch), "ambiguous char");
}

void test_crash_loop_guard() {
    // reset reasons (ESP8266 rst_info): 0 power-on, 1 hw wdt, 2 exception, 3 soft wdt, 4 soft restart, 6 ext reset
    uint8_t n = nextCrashCount(0, false, 0);
    TEST_ASSERT_EQUAL(0, n);
    n = nextCrashCount(n, true, 2);
    n = nextCrashCount(n, true, 3);
    TEST_ASSERT_FALSE(safeMode(n));
    n = nextCrashCount(n, true, 1);
    TEST_ASSERT_TRUE(safeMode(n));                         // 3 crashes in a row -> safe mode
    TEST_ASSERT_EQUAL(0, nextCrashCount(n, true, 6));      // user pressed reset / replugged -> normal again
    TEST_ASSERT_EQUAL(0, nextCrashCount(n, true, 4));      // OTA / deliberate restart
    TEST_ASSERT_EQUAL(1, nextCrashCount(200, false, 2));   // garbage RTC memory -> start counting afresh
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_five_failed_attempts_enter_setup_and_connect_leaves_it);
    RUN_TEST(test_setup_retries_old_network_only_without_phone_and_every_2_min);
    RUN_TEST(test_no_network_known_at_all_opens_setup_at_once);
    RUN_TEST(test_valid_creds);
    RUN_TEST(test_wifi_qr_escapes_special_chars);
    RUN_TEST(test_ap_password_is_stable_readable_and_per_device);
    RUN_TEST(test_crash_loop_guard);
    return UNITY_END();
}
