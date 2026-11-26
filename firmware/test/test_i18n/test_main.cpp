// English (default) / Ukrainian texts: every string has both languages, scenes have English captions.
#include <unity.h>
#include <cstring>
#include <string>
#include "i18n.h"
#include "model.h"
#include "scene.h"
#include "settings.h"
#include "text.h"

void setUp() { setLang(Lang::En); }
void tearDown() {}

void test_default_is_english_and_switches() {
    TEST_ASSERT_EQUAL(int(Lang::En), int(lang()));
    TEST_ASSERT_EQUAL_STRING("Running", verbFor("Bash"));
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL_STRING("Виконує", verbFor("Bash"));
    TEST_ASSERT_EQUAL_STRING("Ліміт", tr(T_LIMIT));
}

void test_every_text_has_both_languages() {
    for (int i = 0; i < T_COUNT; i++) {
        setLang(Lang::En);
        std::string en = tr(Txt(i));
        setLang(Lang::Uk);
        std::string uk = tr(Txt(i));
        TEST_ASSERT_FALSE_MESSAGE(en.empty(), "missing English text");
        TEST_ASSERT_FALSE_MESSAGE(uk.empty(), "missing Ukrainian text");
    }
}

void test_several_tr_results_live_at_once() {  // one line often joins 3-4 texts
    std::string s = std::string(tr(T_WAITING)) + tr(T_U_MIN) + tr(T_LIMIT) + tr(T_DONE);
    TEST_ASSERT_EQUAL_STRING("waiting mLimitDone", s.c_str());
}

void test_dates_and_timer_units() {
    TEST_ASSERT_EQUAL_STRING("THU 26 NOV", dateShort(4, 26, 11).c_str());
    TEST_ASSERT_EQUAL_STRING("45m", resetShort(45 * 60, false).c_str());
    TEST_ASSERT_EQUAL_STRING("3d", resetShort(3 * 86400 + 60, true).c_str());
    TEST_ASSERT_EQUAL_STRING("5h", resetShort(5 * 3600 + 60, true).c_str());
    TEST_ASSERT_EQUAL_STRING("1:05", resetShort(65 * 60, false).c_str());
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL_STRING("ЧТ 26.11", dateShort(4, 26, 11).c_str());
    TEST_ASSERT_EQUAL_STRING("45хв", resetShort(45 * 60, false).c_str());
}

void test_every_captioned_scene_has_an_english_caption() {
    for (int i = 0; i < sceneCount(); i++) {
        Scene s = sceneAt(i);
        if (!s.caption || !*s.caption) continue;
        const char *en = sceneCaption(s.name, s.caption);
        TEST_ASSERT_TRUE_MESSAGE(en && *en, s.name);
        for (const char *p = en; *p; p++) TEST_ASSERT_TRUE_MESSAGE((unsigned char)*p < 0x80, s.name);  // English = ASCII
    }
    setLang(Lang::Uk);
    TEST_ASSERT_EQUAL_STRING("Гелловін", sceneCaption("halloween", "Гелловін"));
}

void test_lang_codes_and_settings_record() {
    Lang l;
    TEST_ASSERT_TRUE(parseLang("uk", l));
    TEST_ASSERT_EQUAL(int(Lang::Uk), int(l));
    TEST_ASSERT_TRUE(parseLang("en", l));
    TEST_ASSERT_FALSE(parseLang("de", l));
    Settings s;
    s.token = "abcdefghijklmnopqrstuvwxyz012345";
    s.lang = "uk";
    std::vector<uint8_t> b = encodeSettingsRec(1, s);
    Settings back;
    uint32_t seq;
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_EQUAL_STRING("uk", back.lang.c_str());
    s.lang = "xx";  // unknown -> dropped (English)
    b = encodeSettingsRec(2, s);
    TEST_ASSERT_TRUE(decodeSettingsRec(b.data(), b.size(), seq, back));
    TEST_ASSERT_TRUE(back.lang.empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_is_english_and_switches);
    RUN_TEST(test_every_text_has_both_languages);
    RUN_TEST(test_several_tr_results_live_at_once);
    RUN_TEST(test_dates_and_timer_units);
    RUN_TEST(test_every_captioned_scene_has_an_english_caption);
    RUN_TEST(test_lang_codes_and_settings_record);
    return UNITY_END();
}
