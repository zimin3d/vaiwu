#include "vaiwu/game/Assistant.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

namespace vaiwu::game {

namespace {

// Lowercase ASCII and Cyrillic (UTF-8: А-Я 0xD090-0xD0AF -> а-я, Ё -> ё).
std::string toLowerUtf8(const char* text)
{
    std::string out;
    const std::size_t len = std::strlen(text);
    out.reserve(len);
    for (std::size_t i = 0; i < len; ++i) {
        const auto b0 = static_cast<unsigned char>(text[i]);
        if (b0 < 0x80) {
            out.push_back(static_cast<char>(std::tolower(b0)));
            continue;
        }
        if (b0 == 0xD0 && i + 1 < len) {
            const auto b1 = static_cast<unsigned char>(text[i + 1]);
            if (b1 >= 0x90 && b1 <= 0x9F) { // А-П -> а-п (0xD0B0..)
                out.push_back(static_cast<char>(0xD0));
                out.push_back(static_cast<char>(b1 + 0x20));
                ++i;
                continue;
            }
            if (b1 >= 0xA0 && b1 <= 0xAF) { // Р-Я -> р-я (0xD180..)
                out.push_back(static_cast<char>(0xD1));
                out.push_back(static_cast<char>(b1 - 0x20));
                ++i;
                continue;
            }
            if (b1 == 0x81) { // Ё -> ё
                out.push_back(static_cast<char>(0xD1));
                out.push_back(static_cast<char>(0x91));
                ++i;
                continue;
            }
        }
        out.push_back(static_cast<char>(b0));
    }
    return out;
}

bool contains(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

// First "H", "H:MM", "H.MM" or "H MM" in the text; returns false if none.
bool findClockTime(const std::string& text, int& hour, int& minute)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            continue;
        }
        int h = 0;
        std::size_t j = i;
        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j])) && j - i < 2) {
            h = h * 10 + (text[j] - '0');
            ++j;
        }
        int m = 0;
        if (j + 2 < text.size() + 1 && j < text.size()
            && (text[j] == ':' || text[j] == '.' || text[j] == '-')
            && j + 2 <= text.size() + 1
            && j + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[j + 1]))) {
            std::size_t k = j + 1;
            m = 0;
            int digits = 0;
            while (k < text.size() && std::isdigit(static_cast<unsigned char>(text[k])) && digits < 2) {
                m = m * 10 + (text[k] - '0');
                ++k;
                ++digits;
            }
        }
        if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            hour = h;
            minute = m;
            return true;
        }
        i = j;
    }
    return false;
}

// First plain number in the text (for "in N minutes"); -1 if none.
double findDecimal(const std::string& text)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (std::isdigit(static_cast<unsigned char>(ch))
            || (ch == '-' && i + 1 < text.size()
                && std::isdigit(static_cast<unsigned char>(text[i + 1])))) {
            return std::strtod(text.c_str() + i, nullptr);
        }
    }
    return 0.0;
}

// Earliest position where any of the given unit words appears.
std::size_t unitPos(const std::string& text, std::initializer_list<const char*> keys)
{
    std::size_t best = std::string::npos;
    for (const char* k : keys) {
        const std::size_t p = text.find(k);
        if (p < best) {
            best = p;
        }
    }
    return best;
}

// Simple offline unit conversion (temperature / distance / weight), both
// directions. Writes "5 km = 3.11 mi" into out; returns true if recognized.
bool tryConvert(const std::string& text, char* out, std::size_t outSize)
{
    if (text.find_first_of("0123456789") == std::string::npos) {
        return false;
    }
    const double v = findDecimal(text);
    const std::size_t npos = std::string::npos;

    const std::size_t c = unitPos(text, {"\u0446\u0435\u043B\u044C\u0441", "celsius"});
    const std::size_t f = unitPos(text, {"\u0444\u0430\u0440\u0435\u043D", "fahrenheit"});
    if (c != npos && f != npos) {
        if (c <= f) {
            std::snprintf(out, outSize, "%.1f C = %.1f F", v, v * 9.0 / 5.0 + 32.0);
        } else {
            std::snprintf(out, outSize, "%.1f F = %.1f C", v, (v - 32.0) * 5.0 / 9.0);
        }
        return true;
    }

    const std::size_t km = unitPos(text, {"\u043A\u0438\u043B\u043E\u043C\u0435\u0442", "\u043A\u043C", "km", "kilomet"});
    const std::size_t mi = unitPos(text, {"\u043C\u0438\u043B", "mile", " mi"});
    if (km != npos && mi != npos) {
        if (km <= mi) {
            std::snprintf(out, outSize, "%.2f km = %.2f mi", v, v * 0.621371);
        } else {
            std::snprintf(out, outSize, "%.2f mi = %.2f km", v, v / 0.621371);
        }
        return true;
    }

    const std::size_t kg = unitPos(text, {"\u043A\u0438\u043B\u043E\u0433\u0440", "\u043A\u0433", "kg", "kilogram"});
    const std::size_t lb = unitPos(text, {"\u0444\u0443\u043D\u0442", "pound", "lb"});
    if (kg != npos && lb != npos) {
        if (kg <= lb) {
            std::snprintf(out, outSize, "%.2f kg = %.2f lb", v, v * 2.2046226);
        } else {
            std::snprintf(out, outSize, "%.2f lb = %.2f kg", v, v / 2.2046226);
        }
        return true;
    }
    return false;
}

int findNumber(const std::string& text)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            int value = 0;
            while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) && value < 100000) {
                value = value * 10 + (text[i] - '0');
                ++i;
            }
            return value;
        }
    }
    return -1;
}

constexpr int kJokes = 10;

constexpr const char* kJokesEn[kJokes] = {
    "Why do programmers prefer dark mode? Because light attracts bugs!",
    "I told my Wi-Fi a joke... no connection.",
    "Why did the tomato blush? It saw the salad dressing!",
    "I would tell a UDP joke, but you might not get it.",
    "My bed and I are perfect for each other, but my alarm keeps trying to break us up.",
    "Why don't skeletons fight? They don't have the guts.",
    "I'm on a seafood diet. I see food and I eat it.",
    "Parallel lines have so much in common... a pity they'll never meet.",
    "Why did the scarecrow win an award? He was outstanding in his field!",
    "There are 10 kinds of people: those who know binary and those who don't.",
};

constexpr const char* kJokesRu[kJokes] = {
    "Почему программисты любят тёмную тему? На свет слетаются баги!",
    "Рассказала анекдот про Wi-Fi... но он до тебя не дошёл.",
    "Заходит нейросеть в бар, а бармен: «Вам как обычно, с галлюцинациями?»",
    "Мой будильник и моя кровать в разводе. Я - причина.",
    "Параллельные прямые так похожи... жаль, им не суждено встретиться.",
    "- Ты выспалась? - Куда высылать?",
    "Почему скелеты такие спокойные? Их ничто не трогает за живое.",
    "Я на морской диете: вижу еду - и ем.",
    "Есть 10 типов людей: те, кто понимает двоичную систему, и остальные.",
    "Чем кончается лето? Мягким знаком!",
};

} // namespace

Intent parseIntent(const char* utf8Text, Lang lang)
{
    static_cast<void>(lang); // both keyword sets are always checked
    Intent intent{};
    if (utf8Text == nullptr || utf8Text[0] == '\0') {
        return intent;
    }
    const std::string text = toLowerUtf8(utf8Text);

    const bool mentionsAlarm = contains(text, "будильник") || contains(text, "разбуди")
        || contains(text, "alarm") || contains(text, "wake me");
    const bool mentionsRemind = contains(text, "напомни") || contains(text, "remind");
    const bool mentionsCancel = contains(text, "отмени") || contains(text, "убери")
        || contains(text, "cancel") || contains(text, "delete");

    if (mentionsCancel && (mentionsAlarm || mentionsRemind)) {
        intent.kind = IntentKind::CancelAlarms;
        return intent;
    }

    const bool relative = contains(text, "через") || contains(text, " in ");
    if (mentionsRemind && relative) {
        const int number = findNumber(text);
        const bool hours = contains(text, "час") || contains(text, "hour");
        if (number > 0) {
            intent.kind = IntentKind::SetReminderIn;
            intent.afterMinutes = hours ? number * 60 : number;
            if (intent.afterMinutes > 24 * 60) {
                intent.afterMinutes = 24 * 60;
            }
        } else if (hours) {
            intent.kind = IntentKind::SetReminderIn;
            intent.afterMinutes = 60;
        } else {
            intent.kind = IntentKind::AlarmMissingTime;
        }
        return intent;
    }

    if (mentionsAlarm || mentionsRemind) {
        int hour = -1;
        int minute = 0;
        if (findClockTime(text, hour, minute)) {
            intent.kind = IntentKind::SetAlarm;
            intent.hour = hour;
            intent.minute = minute;
        } else {
            intent.kind = IntentKind::AlarmMissingTime;
        }
        return intent;
    }

    if (contains(text, "\u0442\u0430\u0439\u043C\u0435\u0440") || contains(text, "timer")) {
        const int number = findNumber(text);
        const bool hours = contains(text, "\u0447\u0430\u0441") || contains(text, "hour");
        if (number > 0) {
            intent.kind = IntentKind::SetReminderIn;
            intent.afterMinutes = hours ? number * 60 : number;
            if (intent.afterMinutes > 24 * 60) {
                intent.afterMinutes = 24 * 60;
            }
        } else {
            intent.kind = IntentKind::AlarmMissingTime;
        }
        return intent;
    }

    if (contains(text, "\u043F\u043E\u043C\u043E\u0434\u043E\u0440") || contains(text, "pomodoro")) {
        intent.kind = IntentKind::Pomodoro;
        intent.afterMinutes = 25;
        return intent;
    }

    {
        char conv[64];
        if (tryConvert(text, conv, sizeof(conv))) {
            intent.kind = IntentKind::Convert;
            std::snprintf(intent.convertResult, sizeof(intent.convertResult), "%s", conv);
            return intent;
        }
    }

    if (contains(text, "анекдот") || contains(text, "шутк") || contains(text, "пошути")
        || contains(text, "joke")) {
        intent.kind = IntentKind::Joke;
        return intent;
    }

    if (contains(text, "погод") || contains(text, "weather") || contains(text, "температур")
        || contains(text, "на улице") || contains(text, "temperature")) {
        intent.kind = IntentKind::AskWeather;
        return intent;
    }

    if (contains(text, "который час") || contains(text, "сколько времени")
        || contains(text, "what time") || contains(text, "time is it")) {
        intent.kind = IntentKind::AskTime;
        return intent;
    }

    return intent;
}

const char* jokeText(Lang lang, std::uint32_t salt)
{
    const auto index = salt % kJokes;
    return lang == Lang::Ru ? kJokesRu[index] : kJokesEn[index];
}

} // namespace vaiwu::game
