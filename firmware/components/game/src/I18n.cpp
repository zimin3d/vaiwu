#include "vaiwu/game/I18n.hpp"

namespace vaiwu::game {

namespace {
constexpr int kCount = static_cast<int>(Str::Count_);

constexpr const char* kEn[kCount] = {
    "CHOOSE YOUR VAIWU",
    "NAME:",
    "her name...",
    "Empty name = her own name",
    "START",
    "likes",
    "FEED", "PLAY", "WASH", "WC", "TALK",
    "HUG", "PAT", "LEAVE",
    "PICK A GAME", "GUESS EMOTION", "CATCH HEARTS", "REPEAT MELODY", "CANCEL",
    "GUESS MY EMOTION!",
    "HAPPY", "SAD", "ANGRY", "SLEEPY",
    "TAP 5 HEARTS!",
    "WATCH CLOSELY...", "YOUR TURN!",
    "CORRECT! SHE IS DELIGHTED!", "WRONG... SHE POUTS A LITTLE.", "TOO SLOW...",
    "Too tired to play...", "She is full already.",
    "She has left this world...", "RESTART",
    "%d kg  %s\nDay %u%s",
    "   SICK!",
    "SETTINGS", "Language", "English", "Wi-Fi networks", "New game", "(hold to confirm)", "BACK",
    "WI-FI NETWORKS", "RESCAN", "Scanning...", "No networks found",
    "Password for %s",
    "CONNECT", "Connecting...", "Failed. Check password?", "Connected!",
    "...",
    "FOOD", "MOOD", "ENGY", "DISC",
    "CHAT", "CHAT WITH HER", "ask me anything...", "SEND",
    "Alarm set for %02d:%02d!", "I will remind you in %d min!",
    "Tell me the time, e.g. 7:30",
    "All alarms cancelled.", "Too many alarms already (max 4).",
    "It is %02d:%02d now.", "The clock is not synced yet (need Wi-Fi).",
    "ALARM!", "OK", "I am offline... ask me when Wi-Fi is back.",
    "Listening...", "Recognizing...", "Did not catch that. Try again?",
    "Hold the mic and speak", "VOICE",
    "Checking the weather...", "%s: %d C, %s", "Could not get the weather.",
    "Focus timer started - 25 minutes!",
    "Coins: %u", "+%u coins!",
    "SHOP", "BUY", "Not enough coins!", "Enjoy your gift!",
    "Tasty food", "Cake", "Toy", "Energy drink", "Medicine", "Ribbon", "Sushi",
    "Vitamins", "Super energy", "HP potion", "MP potion",
    "CHOOSE FOOD", "Rice", "free",
    "ROCK-PAPER-SCISSORS", "QUICK MATH", "ROCK", "PAPER", "SCISSORS",
    "You win! +coin", "You lose...", "Draw!",
    "SOLVE IT!",
    "CHOOSE DIFFICULTY", "EASY (4)", "MEDIUM (6)", "HARD (8)",
    "FOOD", "MEDICINE", "GIFTS",
    "Help / About",
    "ABOUT",
    "Tamagotchi Vaiwu - your pocket companion.\n\n"
    "Care for her: feed, play, wash, take her to the toilet and chat. "
    "Watch the four bars - food, mood, energy, discipline. If they stay "
    "low she gets sick and may even die.\n\n"
    "Win mini-games (PLAY button) to earn gold coins. Spend them in the "
    "SHOP (tap the coin badge) on food, medicine and gifts.\n\n"
    "The heart button opens the chat: talk by voice or keyboard, ask for "
    "the weather, time, jokes, timers and alarms. Works offline too - "
    "with Wi-Fi she becomes much smarter.\n\n"
    "She sleeps at night; tap her to wake her (she will grumble). "
    "Settings: language, Wi-Fi, volume, brightness, new game.",
    "Volume", "Brightness",
    "ADVENTURE",
    "%s Lv.%d blocks the path!", "Slime", "Bat", "Shroom",
    "Fox", "Ghost", "Golem",
    "ATTACK", "SKILL", "POTION x%d", "RUN",
    "You hit: -%d HP", " CRIT!", "Foe hits you: -%d HP",
    "Foe misses!", "Not enough MP!", "No potions left!", "Healed +%d HP",
    "You escaped!", "Can't escape!",
    "VICTORY!  +%d XP  +%d gold", "You faint... and lose %d gold",
    "LEVEL %d!", "Points: %d",
    "Strength", "Vitality", "Agility", "Magic", "Luck",
    "DONE",
    "WEAPONS", "ARMOR", "TRINKETS",
    "Wooden sword", "Steel sword", "Magic staff", "Wind dagger",
    "Leather vest", "Iron armor", "Lucky charm", "Star amulet",
    "Moon blade", "Scale mail", "Silk cloak", "Sage ring",
    "Weapon", "Armor", "Amulet",
    "- none -", "Already owned!", "GEAR", "EQUIPMENT",
    "STR", "VIT", "AGI", "MAG", "LCK",
    "%d kg", "Day %u",
    "BLAST", "HEAL", "DRAIN", "FOCUS", "FLURRY", "GUARD", "TRICK",
    " +%d more", "  drained +%d HP", "  guard up!", "  snagged %d gold!",
    "A %s is prowling near the house!", "FIGHT", "IGNORE",
    "Her voice: %s", "off", "babble", "speech",
    "Screen dims: %s", "never", "30 s", "1 min", "5 min",
    "DUNGEON", "Room %d/%d", "BOSS ROOM!",
    "A breather: +%d HP, +%d MP", "DEEPER", "GO HOME",
    "The chest holds %d gold!", "Dungeon cleared: %d rooms!",
    "MANA x%d", "  mana +%d", "Long press a slot = sell for half price",
    "Sold for %d gold", "Nothing to sell here",
};

constexpr const char* kRu[kCount] = {
    "ВЫБЕРИ СВОЮ ВАЙФУ",
    "ИМЯ:",
    "её имя...",
    "Пустое имя = её собственное",
    "СТАРТ",
    "любит",
    "ЕДА", "ИГРА", "МЫТЬЁ", "ТУАЛЕТ", "ЧАТ",
    "ОБНЯТЬ", "ГЛАДИТЬ", "УЙТИ",
    "ВЫБЕРИ ИГРУ", "УГАДАЙ ЭМОЦИЮ", "ЛОВИ СЕРДЕЧКИ", "ПОВТОРИ МЕЛОДИЮ", "ОТМЕНА",
    "УГАДАЙ МОЮ ЭМОЦИЮ!",
    "РАДОСТЬ", "ГРУСТЬ", "ЗЛОСТЬ", "СОН",
    "ПОЙМАЙ 5 СЕРДЕЧЕК!",
    "СМОТРИ ВНИМАТЕЛЬНО...", "ТВОЙ ХОД!",
    "ВЕРНО! ОНА В ВОСТОРГЕ!", "МИМО... ОНА НАДУЛАСЬ.", "СЛИШКОМ МЕДЛЕННО...",
    "Слишком устала для игр...", "Она уже сыта.",
    "Она покинула этот мир...", "ЗАНОВО",
    "%d кг  %s\nДень %u%s",
    "   БОЛЕЕТ!",
    "НАСТРОЙКИ", "Язык", "Русский", "Сети Wi-Fi", "Новая игра", "(удерживай для сброса)", "НАЗАД",
    "СЕТИ WI-FI", "ОБНОВИТЬ", "Сканирую...", "Сети не найдены",
    "Пароль для %s",
    "ПОДКЛЮЧИТЬ", "Подключаюсь...", "Не вышло. Верный пароль?", "Подключено!",
    "...",
    "ЕДА", "НАСТР", "ЭНЕРГ", "ДИСЦ",
    "ЧАТ", "ЧАТ С НЕЙ", "спроси меня о чём-нибудь...", "ОТПР",
    "Будильник поставлен на %02d:%02d!", "Напомню через %d мин!",
    "Скажи время, например 7:30",
    "Все будильники отменены.", "Слишком много будильников (макс. 4).",
    "Сейчас %02d:%02d.", "Часы ещё не синхронизированы (нужен Wi-Fi).",
    "БУДИЛЬНИК!", "ОК", "Я оффлайн... спроси, когда появится Wi-Fi.",
    "Слушаю...", "Распознаю...", "Не расслышала. Ещё раз?",
    "Зажми микрофон и говори", "ГОЛОС",
    "Узнаю погоду...", "%s: %d C, %s", "Не удалось узнать погоду.",
    "Помодоро запущен - 25 минут!",
    "Золото: %u", "+%u золота!",
    "МАГАЗИН", "КУПИТЬ", "Не хватает золота!", "Приятного подарка!",
    "Вкусняшка", "Торт", "Игрушка", "Энергетик", "Лекарство", "Бантик", "Суши",
    "Витамины", "Супер-энергетик", "Зелье здоровья", "Зелье маны",
    "ВЫБЕРИ ЕДУ", "Рис", "беспл.",
    "КАМЕНЬ-НОЖНИЦЫ-БУМАГА", "БЫСТРЫЙ СЧЁТ", "КАМЕНЬ", "БУМАГА", "НОЖНИЦЫ",
    "Ты выиграл! +монета", "Ты проиграл...", "Ничья!",
    "РЕШИ!",
    "ВЫБЕРИ СЛОЖНОСТЬ", "ЛЕГКО (4)", "СРЕДНЕ (6)", "СЛОЖНО (8)",
    "ЕДА", "АПТЕКА", "ПОДАРКИ",
    "Помощь / О программе",
    "О ПРОГРАММЕ",
    "Тамагочи Вайфу - твой карманный компаньон.\n\n"
    "Заботься о ней: корми, играй, мой, води в туалет и болтай. "
    "Следи за четырьмя шкалами - еда, настроение, энергия, дисциплина. "
    "Если они на нуле, она заболеет и может даже умереть.\n\n"
    "Выигрывай мини-игры (кнопка ИГРА) и получай золото. Трать его в "
    "МАГАЗИНЕ (нажми на значок с монетой): еда, лекарства, подарки.\n\n"
    "Кнопка с сердцем открывает чат: говори голосом или с клавиатуры, "
    "спрашивай погоду, время, шутки, ставь таймеры и будильники. "
    "Работает и без сети - с Wi-Fi она заметно умнее.\n\n"
    "Ночью она спит; коснись её, чтобы разбудить (будет ворчать). "
    "Настройки: язык, Wi-Fi, громкость, яркость, новая игра.",
    "Громкость", "Яркость",
    "ПОХОД",
    "%s ур.%d преграждает путь!", "Слизень", "Нетопырь", "Гриб-ворчун",
    "Лиса-плутовка", "Призрак", "Голем",
    "УДАР", "НАВЫК", "ЗЕЛЬЕ x%d", "БЕГСТВО",
    "Ты бьёшь: -%d HP", " КРИТ!", "Враг бьёт: -%d HP",
    "Враг промахнулся!", "Не хватает маны!", "Зелий больше нет!", "Лечение +%d HP",
    "Ты сбежала!", "Не удалось сбежать!",
    "ПОБЕДА!  +%d опыта  +%d золота", "Ты без сил... потеряно %d золота",
    "УРОВЕНЬ %d!", "Очки: %d",
    "Сила", "Живучесть", "Ловкость", "Магия", "Удача",
    "ГОТОВО",
    "ОРУЖИЕ", "БРОНЯ", "АМУЛЕТЫ",
    "Деревянный меч", "Стальной меч", "Волшебный посох", "Кинжал ветра",
    "Кожаный жилет", "Железная броня", "Счастливый оберег", "Звёздный амулет",
    "Лунный клинок", "Чешуйчатая броня", "Шёлковый плащ", "Кольцо мудреца",
    "Оружие", "Броня", "Амулет",
    "- пусто -", "Уже куплено!", "ВЕЩИ", "СНАРЯЖЕНИЕ",
    "СИЛ", "ЖИВ", "ЛОВ", "МАГ", "УДЧ",
    "%d кг", "День %u",
    "ВСПЫШКА", "ЛЕЧЕНИЕ", "ВЫТЯЖКА", "ТОЧНЫЙ", "СЕРИЯ", "ЗАЩИТА", "ПРОКАЗА",
    " +%d ещё", "  вытянуто +%d HP", "  защита!", "  стянуто %d золота!",
    "%s бродит у дома!", "В БОЙ", "НЕ ВЫХОДИТЬ",
    "Её голос: %s", "выкл", "лепет", "речь",
    "Экран гаснет: %s", "никогда", "30 с", "1 мин", "5 мин",
    "ПОДЗЕМЕЛЬЕ", "Зал %d/%d", "ЗАЛ БОССА!",
    "Передышка: +%d HP, +%d MP", "ГЛУБЖЕ", "ДОМОЙ",
    "В сундуке %d золота!", "Подземелье пройдено: залов %d!",
    "МАНА x%d", "  мана +%d", "Долгий тап по слоту = продать за полцены",
    "Продано за %d золота", "Тут нечего продать",
};
} // namespace

const char* tr(Lang lang, Str id)
{
    const auto index = static_cast<int>(id);
    if (index < 0 || index >= kCount) {
        return "?";
    }
    return lang == Lang::Ru ? kRu[index] : kEn[index];
}

} // namespace vaiwu::game
