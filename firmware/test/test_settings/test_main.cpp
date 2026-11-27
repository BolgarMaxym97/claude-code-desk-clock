// Device settings (token, timezone, birthday) and pairing a computer by the code shown on the screen.
#include <unity.h>
#include <vector>
#include "rawstore.h"
#include "settings.h"

void setUp() {}
void tearDown() {}

static uint32_t seed = 1;
static uint32_t fakeRnd() { seed = seed * 1103515245u + 12345u; return seed >> 3; }

void test_settings_roundtrip_through_flash_record() {
    Settings s;
    s.token = "abcdefghijklmnopqrstuvwxyz012345";
    s.tz = "EET-2EEST,M3.5.0/3,M10.5.0/4";
    s.birthMonth = 11;
    s.birthDay = 20;
    std::vector<uint8_t> b = encodeSettingsRec(9, s);
    TEST_ASSERT_EQUAL(0, int(b.size() % 4));
    TEST_ASSERT_TRUE(b.size() <= SETTINGS_REC_MAX);
    Settings back;
    uint32_t seq = 0;
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_EQUAL(9, int(seq));
    TEST_ASSERT_EQUAL_STRING(s.token.c_str(), back.token.c_str());
    TEST_ASSERT_EQUAL_STRING(s.tz.c_str(), back.tz.c_str());
    TEST_ASSERT_EQUAL(11, back.birthMonth);
    TEST_ASSERT_EQUAL(20, back.birthDay);
}

void test_settings_record_is_not_a_wifi_record_and_vice_versa() {
    Settings s;
    s.token = "abcdefghijklmnopqrstuvwxyz012345";
    std::vector<uint8_t> b = encodeSettingsRec(1, s);
    StoreRec w;
    TEST_ASSERT_FALSE(decodeRec(b.data(), b.size(), w));
    std::vector<uint8_t> wifi = encodeRec(StoreRec{1, "Home", "12345678"});
    Settings back;
    uint32_t seq;
    TEST_ASSERT_FALSE(decodeSettingsRec(wifi.data(), wifi.size(), seq, back));
    b[14] ^= 1;  // corrupt payload -> CRC fails
    TEST_ASSERT_FALSE(decodeSettingsRec(b.data(), b.size(), seq, back));
    // our settings sector is not "foreign" for the settings magic
    std::vector<uint8_t> sec = encodeSettingsRec(1, s);
    sec.resize(4096, 0xFF);
    TEST_ASSERT_FALSE(sectorForeign(sec.data(), sec.size(), SETTINGS_MAGIC));
    TEST_ASSERT_TRUE(sectorForeign(sec.data(), sec.size()));  // but it is foreign for the Wi-Fi sectors
}

void test_invalid_values_are_dropped_on_decode() {
    Settings s;
    s.token = "short";                  // too short -> no token
    s.tz = "EET-2;rm -rf";              // bad characters -> no tz
    s.birthMonth = 13;
    s.birthDay = 1;                     // bad date -> no birthday
    std::vector<uint8_t> b = encodeSettingsRec(1, s);
    Settings back;
    uint32_t seq;
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_TRUE(back.token.empty());
    TEST_ASSERT_TRUE(back.tz.empty());
    TEST_ASSERT_EQUAL(0, back.birthMonth);
}

void test_validators() {
    TEST_ASSERT_TRUE(validTz("EET-2EEST,M3.5.0/3,M10.5.0/4"));
    TEST_ASSERT_TRUE(validTz("<+0530>-5:30"));
    TEST_ASSERT_TRUE(validTz("PST8PDT,M3.2.0,M11.1.0"));
    TEST_ASSERT_FALSE(validTz(""));
    TEST_ASSERT_FALSE(validTz("EET-2 EEST"));
    TEST_ASSERT_FALSE(validTz(std::string(60, 'A')));
    TEST_ASSERT_TRUE(validToken("abcdefghijklmnopqrstuvwxyz012345"));
    TEST_ASSERT_FALSE(validToken("abc"));
    TEST_ASSERT_FALSE(validToken("abcdefghijklmnop\"rstuvwxyz012345"));
    int m, d;
    TEST_ASSERT_TRUE(parseBirthday("11-20", m, d));
    TEST_ASSERT_EQUAL(11, m);
    TEST_ASSERT_EQUAL(20, d);
    TEST_ASSERT_TRUE(parseBirthday("", m, d));  // empty = off
    TEST_ASSERT_EQUAL(0, m);
    TEST_ASSERT_FALSE(parseBirthday("02-30", m, d));
    TEST_ASSERT_FALSE(parseBirthday("1120", m, d));
}

void test_token_and_code_generation() {
    std::string t = makeToken(fakeRnd), t2 = makeToken(fakeRnd);
    TEST_ASSERT_EQUAL(32, int(t.size()));
    TEST_ASSERT_TRUE(validToken(t));
    TEST_ASSERT_TRUE(t != t2);
    std::string c = makePairCode(fakeRnd);
    TEST_ASSERT_EQUAL(6, int(c.size()));
    for (char ch : c) TEST_ASSERT_TRUE(std::string(PAIR_ALPHABET).find(ch) != std::string::npos);
}

void test_pair_window_accepts_the_code_once_in_time() {
    PairWindow w;
    TEST_ASSERT_FALSE(w.isOpen(0));
    w.open("K7Q2MX", 1000, 600000);
    TEST_ASSERT_TRUE(w.isOpen(1000));
    TEST_ASSERT_EQUAL(int(PairResult::Wrong), int(w.check("AAAAAA", 2000)));
    TEST_ASSERT_EQUAL(int(PairResult::Ok), int(w.check("k7q2mx", 3000)));  // case-insensitive
    TEST_ASSERT_FALSE(w.isOpen(3000));                                    // one use
    TEST_ASSERT_EQUAL(int(PairResult::Closed), int(w.check("K7Q2MX", 3001)));
}

void test_pair_window_expires_and_locks_after_wrong_tries() {
    PairWindow w;
    w.open("K7Q2MX", 0, 600000);
    TEST_ASSERT_EQUAL(int(PairResult::Closed), int(w.check("K7Q2MX", 600001)));
    w.open("K7Q2MX", 0, 600000);
    for (int i = 0; i < PairWindow::MAX_TRIES; i++) TEST_ASSERT_EQUAL(int(PairResult::Wrong), int(w.check("BBBBBB", 10)));
    TEST_ASSERT_TRUE(w.locked());
    TEST_ASSERT_EQUAL(int(PairResult::Locked), int(w.check("K7Q2MX", 11)));  // even the right code
    TEST_ASSERT_FALSE(w.isOpen(12));
    w.open("NEWCDE", 20, 600000);  // a new code does not give new tries: locked until reboot
    TEST_ASSERT_EQUAL(int(PairResult::Locked), int(w.check("NEWCDE", 21)));
}

void test_info_settings_roundtrip_at_max_length() {
    Settings s;
    s.token = std::string(64, 'a');
    s.tz = std::string(48, 'E');
    s.lang = "uk";
    s.birthMonth = 2; s.birthDay = 29;
    s.owm = "0123456789abcdef0123456789abcdef";
    s.loc = "-33.87,151.21";
    s.region = "Дніпропетровська область";  // the longest ubilling region name
    s.today = "380,86399999,9999,999";
    s.night = "23-7";
    s.bright = "100/100";
    s.face = "2";
    s.pin = "abcdefghijklmnopqrstuvwx,4000000000";
    s.week = "99999:1440,9999,999;1440,9999,999;1440,9999,999;1440,9999,999;1440,9999,999;1440,9999,999;1440,9999,999;1440,9999,999";
    s.anim = "1,31,2,ffffffffffffffffffffffffffffffff";
    s.atok = std::string(64, 'x');
    std::vector<uint8_t> b = encodeSettingsRec(3, s);
    TEST_ASSERT_TRUE(b.size() <= SETTINGS_REC_MAX);
    Settings back;
    uint32_t seq;
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_EQUAL_STRING(s.owm.c_str(), back.owm.c_str());
    TEST_ASSERT_EQUAL_STRING(s.loc.c_str(), back.loc.c_str());
    TEST_ASSERT_EQUAL_STRING(s.region.c_str(), back.region.c_str());
    TEST_ASSERT_EQUAL_STRING("uk", back.lang.c_str());
    TEST_ASSERT_EQUAL_STRING(s.week.c_str(), back.week.c_str());
    TEST_ASSERT_EQUAL_STRING(s.pin.c_str(), back.pin.c_str());
    TEST_ASSERT_EQUAL_STRING("100/100", back.bright.c_str());
    TEST_ASSERT_EQUAL_STRING("23-7", back.night.c_str());
    TEST_ASSERT_EQUAL_STRING("2", back.face.c_str());
    TEST_ASSERT_EQUAL_STRING(s.anim.c_str(), back.anim.c_str());
    TEST_ASSERT_EQUAL_STRING(s.atok.c_str(), back.atok.c_str());
}

void test_display_prefs_parse() {
    int a, b;
    TEST_ASSERT_TRUE(parseNight("", a, b)); TEST_ASSERT_EQUAL(23, a); TEST_ASSERT_EQUAL(7, b);  // default
    TEST_ASSERT_TRUE(parseNight("off", a, b)); TEST_ASSERT_EQUAL(a, b);                          // never sleeps
    TEST_ASSERT_TRUE(parseNight("0-6", a, b)); TEST_ASSERT_EQUAL(0, a); TEST_ASSERT_EQUAL(6, b);
    TEST_ASSERT_FALSE(parseNight("24-7", a, b));
    TEST_ASSERT_FALSE(parseNight("5-5", a, b));
    TEST_ASSERT_FALSE(parseNight("x", a, b));
    TEST_ASSERT_TRUE(parseBright("", a, b)); TEST_ASSERT_EQUAL(80, a); TEST_ASSERT_EQUAL(20, b);
    TEST_ASSERT_TRUE(parseBright("100/5", a, b)); TEST_ASSERT_EQUAL(100, a); TEST_ASSERT_EQUAL(5, b);
    TEST_ASSERT_FALSE(parseBright("101/5", a, b));
    TEST_ASSERT_FALSE(parseBright("50/2", a, b));
    TEST_ASSERT_TRUE(validFace("") && validFace("0") && validFace("6"));
    TEST_ASSERT_FALSE(validFace("7"));
    std::string name;
    uint32_t until;
    TEST_ASSERT_TRUE(parsePin("", name, until)); TEST_ASSERT_TRUE(name.empty());
    TEST_ASSERT_TRUE(parsePin("coffee,0", name, until)); TEST_ASSERT_EQUAL_STRING("coffee", name.c_str()); TEST_ASSERT_EQUAL(0, until);
    TEST_ASSERT_TRUE(parsePin("new-year,1764000000", name, until)); TEST_ASSERT_EQUAL(1764000000u, until);
    TEST_ASSERT_FALSE(parsePin("Bad Name,0", name, until));
    TEST_ASSERT_FALSE(parsePin("coffee", name, until));
}

void test_info_validators() {
    TEST_ASSERT_TRUE(validOwmKey("f0123456789abcdef0123456789abcde"));
    TEST_ASSERT_FALSE(validOwmKey("F0123456789ABCDEF0123456789ABCDE!"));
    TEST_ASSERT_FALSE(validOwmKey("short"));
    TEST_ASSERT_TRUE(validLoc("50.45,30.52"));
    TEST_ASSERT_TRUE(validLoc("-33.87,151.21"));
    TEST_ASSERT_FALSE(validLoc("50.45"));
    TEST_ASSERT_FALSE(validLoc("95,30"));
    TEST_ASSERT_FALSE(validLoc("50,30&appid=x"));
    TEST_ASSERT_TRUE(validRegion("м. Київ"));
    TEST_ASSERT_FALSE(validRegion("a\nb"));
    TEST_ASSERT_FALSE(validRegion("x\"y"));
    TEST_ASSERT_FALSE(validRegion(std::string(81, 'x')));
}

void test_today_counters_in_the_settings_record() {
    Settings s;
    s.token = std::string(64, 'a');
    s.tz = std::string(48, 'E');
    s.owm = "0123456789abcdef0123456789abcdef";
    s.loc = "-33.87,151.21";
    s.region = "Дніпропетровська область";
    s.today = "378,86399000,999,999";
    std::vector<uint8_t> b = encodeSettingsRec(4, s);
    TEST_ASSERT_TRUE(b.size() <= SETTINGS_REC_MAX);
    Settings back;
    uint32_t seq;
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_EQUAL_STRING("378,86399000,999,999", back.today.c_str());
    int day; uint32_t busy; int ans, com;
    TEST_ASSERT_TRUE(parseToday(back.today, day, busy, ans, com));
    TEST_ASSERT_EQUAL(378, day);
    TEST_ASSERT_EQUAL(86399000u, busy);
    TEST_ASSERT_FALSE(parseToday("1,2,3", day, busy, ans, com));
    TEST_ASSERT_FALSE(parseToday("x,2,3,4", day, busy, ans, com));
    s.today = "1,2,3\nregion=x";  // junk is dropped on decode
    b = encodeSettingsRec(5, s);
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_TRUE(back.today.empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_settings_roundtrip_through_flash_record);
    RUN_TEST(test_settings_record_is_not_a_wifi_record_and_vice_versa);
    RUN_TEST(test_invalid_values_are_dropped_on_decode);
    RUN_TEST(test_validators);
    RUN_TEST(test_info_settings_roundtrip_at_max_length);
    RUN_TEST(test_info_validators);
    RUN_TEST(test_display_prefs_parse);
    RUN_TEST(test_today_counters_in_the_settings_record);
    RUN_TEST(test_token_and_code_generation);
    RUN_TEST(test_pair_window_accepts_the_code_once_in_time);
    RUN_TEST(test_pair_window_expires_and_locks_after_wrong_tries);
    return UNITY_END();
}
