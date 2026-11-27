#include "i18n.h"
#include <cstdio>
#include <cstring>
#include "pgm.h"

static Lang cur = Lang::En;
void setLang(Lang l) { cur = l; }
Lang lang() { return cur; }
bool parseLang(const std::string &c, Lang &out) {
    if (c == "en") { out = Lang::En; return true; }
    if (c == "uk") { out = Lang::Uk; return true; }
    return false;
}
const char *langCode(Lang l) { return l == Lang::Uk ? "uk" : "en"; }

// X-macro: id, English, Ukrainian
#define TEXTS(X) \
    X(T_V_RUN, "Running", "Виконує") X(T_V_CALL, "Calling", "Викликає") X(T_V_EDIT, "Editing", "Редагує") X(T_V_READ, "Reading", "Читає") \
    X(T_V_DELEGATE, "Delegating", "Делегує") X(T_V_SEARCH, "Searching", "Шукає") X(T_V_WORK, "Working", "Працює") \
    X(T_V_THINK, "Thinking", "Думає") X(T_V_COMPACT, "Compacting", "Стискає") \
    X(T_AGENT1, " agent", " агент") X(T_AGENTS, " agents", " агенти") X(T_CONTEXT, "context", "контекст") X(T_COMPACT_SOON, "compact soon", "скоро стискання") X(T_FC_RAIN, "rain ", "дощ о ") X(T_FC_SNOW, "snow ", "сніг о ") X(T_FC_STORM, "storm ", "гроза о ") \
    X(T_V_ALLOW, "Allow?", "Дозвіл?") X(T_WAITING, "waiting ", "чекає ") X(T_V_YOURTURN, "Your turn", "Твоя черга") \
    X(T_V_ERROR, "Error", "Помилка") X(T_DONE, "Done", "Готово") X(T_IN, "in ", "за ") X(T_V_HELLO, "Hi!", "Привіт!") \
    X(T_LIMIT, "Limit", "Ліміт") X(T_RESETS_AT, "resets at ", "скидання о ") X(T_NO_WIFI, "No Wi-Fi", "Немає Wi-Fi") \
    X(T_SILENT, "Claude is quiet", "Claude мовчить") \
    X(T_BOOT_TIME, "Time", "Час") X(T_SYNCING, "Syncing time…", "Синхронізую час…") \
    X(T_CONNECTING, "Connecting · try ", "Підключаюсь · спроба ") \
    X(T_SETUP_TITLE, "WI-FI SETUP", "НАЛАШТУВАННЯ WI-FI") X(T_SCAN_HINT, "Scan with your phone camera", "Скануй камерою телефона") \
    X(T_CHANGE_WIFI, "Changing Wi-Fi", "Зміна мережі Wi-Fi") X(T_NO_LINK, "No connection to ", "Немає зв’язку з ") \
    X(T_FIRST_SETUP, "First setup", "Перше налаштування") \
    X(T_STEP1, "1  Scan the QR code", "1  Скануй QR камерою") X(T_STEP2, "2  Join the network", "2  Підключись до точки") \
    X(T_STEP3, "3  Pick your Wi-Fi", "3  Обери свою мережу") X(T_STEP3B, "    and enter the password", "    і введи пароль") \
    X(T_OR_OPEN, "or open 192.168.4.1", "або відкрий 192.168.4.1") X(T_CHECKING, "Checking network", "Перевіряю мережу") \
    X(T_WHY_FAIL, "Can't connect", "Не підключився") X(T_WHY_PASS, "Wrong password", "Невірний пароль") \
    X(T_WHY_NOSSID, "Network not found", "Мережі не видно") X(T_WHY_NOIP, "No IP address", "Немає адреси") \
    X(T_DHCP_HINT, "Router gave no IP address (DHCP)", "Роутер не видав IP-адресу (DHCP)") \
    X(T_TRY_AGAIN, "Try again on the phone", "Спробуй ще раз на телефоні") X(T_CONNECTED, "Connected", "Підключено") \
    X(T_NOT_SAVED, "Not saved: forgotten after restart", "Діє лише до перезапуску") \
    X(T_PAIR_TITLE, "PAIR A COMPUTER", "ПІДКЛЮЧЕННЯ КОМП’ЮТЕРА") X(T_PAIR_ENTER, "Type this code on your PC:", "Введи код на комп’ютері:") \
    X(T_PAIR_VALID, "The code works for 5 minutes", "Код діє 5 хвилин") \
    X(T_U_DAY, "d", "д") X(T_U_HOUR, "h", "г") X(T_U_MIN, "m", "хв") X(T_Q_OPEN, "\"", "«") X(T_Q_CLOSE, "\"", "»") \
    X(T_ALERT, "Air raid alert", "Тривога") X(T_ALERT_DRONE, "Drones", "Дрони") X(T_ALERT_MISSILE, "Missiles", "Ракети") X(T_ALERT_THREAT, "Threat", "Загроза") X(T_ALERT_PARTIAL, "Partial", "Частково") \
    X(T_TODAY, "Today", "Сьогодні") X(T_LAST_WEEK, "Week", "Тиждень") X(T_HOURS_SHORT, "h", " год") X(T_FEELS, "feels", "відчув.") X(T_FEELS_SHORT, "feels", "відч.") X(T_MPS, "m/s", "м/с") \
    X(T_ANSWERS, "ans.", "відп.") X(T_COMMIT1, "commit", "коміт") X(T_COMMIT2, "commits", "коміти") \
    X(T_COMMIT5, "commits", "комітів") X(T_MEETING, "Meeting", "Зустріч") X(T_NOW, "Now", "Зараз") \
    X(T_IN_TIME, "In", "Через") X(T_AT, "At", "О") X(T_LEFT, "left", "ще") X(T_W_CLEAR, "clear", "ясно") X(T_W_CLOUDS, "cloudy", "хмарно") \
    X(T_W_RAIN, "rain", "дощ") X(T_W_DRIZZLE, "drizzle", "мряка") X(T_W_SNOW, "snow", "сніг") \
    X(T_W_STORM, "storm", "гроза") X(T_W_FOG, "fog", "туман")

#define DEF(id, en, uk) static const char id##_en[] PROGMEM = en; static const char id##_uk[] PROGMEM = uk;
TEXTS(DEF)
#define ROW(id, en, uk) {id##_en, id##_uk},
static const char *const TABLE[][2] PROGMEM = {TEXTS(ROW)};

static char *ring() {  // 8 buffers: enough for any one line built from several texts
    static char buf[8][72];
    static unsigned n = 0;
    return buf[n++ % 8];
}

static const char *copyP(const char *p) {
    char *b = ring();
    strncpy_P(b, p, 71);
    b[71] = 0;
    return b;
}

const char *tr(Txt t) {
    if (t >= T_COUNT) return "";
    return copyP(reinterpret_cast<const char *>(pgm_read_ptr(&TABLE[t][cur == Lang::Uk ? 1 : 0])));
}

std::string dateShort(int wday, int day, int month) {
    static const char *const DEN[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *const DUK[] = {"НД", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"};
    static const char *const MEN[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char b[24];
    if (cur == Lang::Uk) snprintf(b, sizeof b, "%s %02d.%02d", DUK[wday % 7], day, month);
    else snprintf(b, sizeof b, "%s %d %s", DEN[wday % 7], day, MEN[(month + 11) % 12]);
    return b;
}

struct CapEn { const char *name, *en; };
static const CapEn CAPS[] = {
    {"valentine", "Valentine's Day"}, {"march8", "Women's Day"}, {"easter", "Easter"},
    {"programmers", "Programmers' Day"}, {"independence", "Independence Day"}, {"defenders", "Defenders' Day"},
    {"halloween", "Halloween"}, {"birthday", "Happy birthday!"}, {"april1", "April Fools' Day"},
    {"cosmos", "Cosmonautics Day"}, {"pi", "Pi Day"}, {"may4", "May the 4th"}, {"vyshyvanka", "Vyshyvanka Day"},
    {"constitution", "Constitution Day"}, {"sept1", "Back to school"}, {"nicholas", "St Nicholas Day"},
    {"mothers", "Mother's Day"}, {"closed", "See you Monday!"}, {"kyiv", "Kyiv Day"}, {"unity", "Unity Day"},
    {"flag-day", "Flag Day"}, {"cat-day", "Cat Day"}, {"clock-birthday", "Clock's birthday"},
};

const char *sceneCaption(const char *name, const char *ukCaption) {
    if (cur == Lang::Uk || !name) return ukCaption;
    for (const CapEn &c : CAPS)
        if (strcmp(c.name, name) == 0) return c.en;
    return ukCaption && ukCaption[0] && (unsigned char)ukCaption[0] < 0x80 ? ukCaption : "";
}
