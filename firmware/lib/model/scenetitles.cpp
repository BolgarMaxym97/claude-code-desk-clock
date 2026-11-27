#include "scenetitles.h"
#include <cstring>
#include "i18n.h"
#include "pgm.h"
#include "scene.h"
#include "settings.h"

// Readable names of the idle scenes for the settings page: "name|English|Ukrainian\n" (flash only, no RAM).
static const char TITLES[] PROGMEM =
    "coffee|Coffee|Кава\nbook|Reading a book|Читає книгу\nlaptop|Laptop|Ноутбук\nmeditation|Meditation|Медитація\n"
    "bubbles|Soap bubbles|Мильні бульбашки\nplane|Paper plane|Паперовий літачок\nrope|Jump rope|Скакалка\n"
    "juggle|Juggling|Жонглює\nwatering|Watering a plant|Поливає квітку\ncat|Cat|Кіт\nballoon|Balloon|Повітряна кулька\n"
    "shades|Sunglasses|Сонцезахисні окуляри\nguitar|Guitar|Гітара\nbutterfly|Butterfly|Метелик\nidea|Idea|Ідея\n"
    "ball|Ball|М'яч\nhide|Hide and seek|Хованки\nyoyo|Yo-yo|Йо-йо\ntetris|Falling blocks|Блоки, що падають\n"
    "fishing|Fishing|Риболовля\ntelescope|Telescope|Телескоп\neasel|Painting|Малює\ncastle|Sand castle|Замок з піску\n"
    "rubik|Puzzle cube|Кубик-головоломка\ndumbbells|Dumbbells|Гантелі\nheadphones|Headphones|Навушники\n"
    "tophat|Magic hat|Капелюх фокусника\nnewspaper|Newspaper|Газета\naquarium|Aquarium|Акваріум\n"
    "handheld|Handheld game|Портативна гра\nrocket|Rocket|Ракета\nselfie|Selfie|Селфі\ncatch-drop|Catching drops|Ловить краплі\n"
    "hammock|Hammock|Гамак\npicnic|Picnic|Пікнік\nsunbath|Sunbathing|Засмагає\nchess|Chess|Шахи\n"
    "ufo|UFO|НЛО\nasterisk|Asterisk|Зірочка\nguest|A guest|Гість\ndragon|Dragon|Дракон\nknock|Knock knock|Тук-тук\n"
    "glitch|Glitch|Глюк\nshooting-star|Shooting star|Падаюча зірка\n"
    "valentine|Valentine's Day|День закоханих\nmarch8|March 8|8 Березня\neaster|Easter|Великдень\n"
    "programmers|Programmers' Day|День програміста\nindependence|Independence Day|День Незалежності\n"
    "defenders|Defenders' Day|День захисників\nhalloween|Halloween|Гелловін\nbirthday|Birthday|День народження\n"
    "kyiv|Kyiv Day|День Києва\nunity|Unity Day|День Соборності\nflag-day|Flag Day|День Прапора\ncat-day|Cat Day|День кота\n"
    "clock-birthday|Clock's birthday|День народження годинника\napril1|April Fools' Day|1 квітня\n"
    "cosmos|Cosmonautics Day|День космонавтики\npi|Pi Day|День числа пі\nmay4|May the 4th|4 травня\n"
    "vyshyvanka|Vyshyvanka Day|День вишиванки\nconstitution|Constitution Day|День Конституції\n"
    "sept1|September 1|1 вересня\nnicholas|St Nicholas Day|День святого Миколая\nmothers|Mother's Day|День матері\n"
    "bored|Bored|Нудьгує\ngoodnight|Good night|На добраніч\nclosed|Week closed|Тиждень закрито\n"
    "all-clear|All clear|Відбій тривоги\nstorm|Thunderstorm|Гроза\nfog|Fog|Туман\nmeeting-now|In a meeting|На зустрічі\n"
    "rain|Rain|Дощ\nsnow|Snow|Сніг\nhot|Heat|Спека\nwind|Wind|Вітер\nmorning|Morning|Ранок\nlunch|Lunch|Обід\n"
    "evening|Evening|Вечір\nfriday|Friday|П'ятниця\nmonday|Monday|Понеділок\nlimit-reset|Limits reset|Ліміти оновились\n"
    "long-session|Long session|Довга сесія\nmeeting|Meeting soon|Скоро зустріч\nalert|Air-raid alert|Тривога\n";

std::string sceneTitle(const char *name) {
    size_t n = strlen(name);
    const char *p = TITLES;
    char line[96];
    for (;;) {
        size_t i = 0;
        char c;
        while ((c = char(pgm_read_byte(p))) && c != '\n' && i < sizeof line - 1) { line[i++] = c; p++; }
        line[i] = 0;
        if (!c) return name;
        p++;
        char *a = strchr(line, '|'), *b = a ? strchr(a + 1, '|') : nullptr;
        if (!a || !b || size_t(a - line) != n || strncmp(line, name, n)) continue;
        *b = 0;
        return lang() == Lang::Uk ? std::string(b + 1) : std::string(a + 1);
    }
}

int pinnedScene(const char *pin, uint32_t epoch) {
    std::string name;
    uint32_t until;
    if (!parsePin(pin, name, until) || name.empty()) return -1;
    if (until && (!epoch || epoch >= until)) return -1;
    int i = sceneIndexByName(name.c_str());
    return i >= 0 && sceneAt(i).kind != SceneKind::Work ? i : -1;
}
