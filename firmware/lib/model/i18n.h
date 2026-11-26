// Clock language: English (default) or Ukrainian. All UI texts live in flash (PROGMEM); tr() copies one into
// a small ring of RAM buffers, so a few results can be combined in one expression.
#pragma once
#include <string>

enum class Lang : unsigned char { En = 0, Uk = 1 };
void setLang(Lang l);
Lang lang();
bool parseLang(const std::string &code, Lang &out);  // "en" / "uk"
const char *langCode(Lang l);

enum Txt : unsigned char {
    T_V_RUN, T_V_EDIT, T_V_READ, T_V_DELEGATE, T_V_SEARCH, T_V_WORK, T_V_THINK, T_V_COMPACT,
    T_AGENT1, T_AGENTS, T_CONTEXT, T_V_ALLOW, T_WAITING, T_V_YOURTURN, T_V_ERROR, T_DONE, T_IN, T_V_HELLO,
    T_LIMIT, T_RESETS_AT, T_NO_WIFI, T_SILENT,
    T_BOOT_TIME, T_SYNCING, T_CONNECTING,
    T_SETUP_TITLE, T_SCAN_HINT, T_CHANGE_WIFI, T_NO_LINK, T_FIRST_SETUP, T_STEP1, T_STEP2, T_STEP3, T_STEP3B, T_OR_OPEN,
    T_CHECKING, T_WHY_FAIL, T_WHY_PASS, T_WHY_NOSSID, T_WHY_NOIP, T_DHCP_HINT, T_TRY_AGAIN, T_CONNECTED, T_NOT_SAVED,
    T_PAIR_TITLE, T_PAIR_ENTER, T_PAIR_VALID,
    T_U_DAY, T_U_HOUR, T_U_MIN, T_Q_OPEN, T_Q_CLOSE,
    T_ALERT, T_ALERT_DRONE, T_ALERT_MISSILE, T_TODAY, T_FEELS, T_MPS, T_ANSWERS, T_COMMIT1, T_COMMIT2, T_COMMIT5,
    T_W_CLEAR, T_W_CLOUDS, T_W_RAIN, T_W_DRIZZLE, T_W_SNOW, T_W_STORM, T_W_FOG,
    T_COUNT
};
const char *tr(Txt t);

// "THU 26 NOV" / "ЧТ 26.11" (wday 0 = Sunday, month 1..12)
std::string dateShort(int wday, int day, int month);
// Caption of a holiday / event scene in the current language (Ukrainian caption is the scene's own).
const char *sceneCaption(const char *sceneName, const char *ukCaption);
