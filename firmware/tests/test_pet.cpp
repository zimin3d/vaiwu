// Host-side unit tests for the game core (no ESP-IDF needed).
// Build & run: python3 tools/run_host_checks.py

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/Assistant.hpp"
#include "vaiwu/game/Shop.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/OfflinePhrases.hpp"
#include "vaiwu/game/SoundSynth.hpp"
#include "vaiwu/game/VaiwuPet.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {
int failures = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++failures;                                                             \
        }                                                                           \
    } while (0)

using namespace vaiwu::game;

constexpr std::uint64_t kMinute = 60ULL * 1000ULL;

void testDecayRates()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Hina, "Test", 0);
    pet.tick(2 * kMinute); // 2 minutes
    // hunger: 1%/2min -> -1; energy: 1%/min * 1.3 (Hina) -> -2.6
    CHECK(std::fabs(pet.stats().hunger - 99.0f) < 0.01f);
    CHECK(std::fabs(pet.stats().energy - (100.0f - 2.6f)) < 0.05f);
    // happiness: 1%/1.5min -> -1.333
    CHECK(std::fabs(pet.stats().happiness - (80.0f - 2.0f / 1.5f)) < 0.05f);
    // discipline: softened 1%/10min -> -0.2 (spec 1%/5min made the only stat
    // without a recovery mechanic hit caprice in a single day).
    CHECK(std::fabs(pet.stats().discipline - (60.0f - 0.2f)) < 0.01f);
    CHECK(pet.stats().ageSeconds == 120);
}

void testAoiSlowHappinessDecay()
{
    VaiwuPet aoi;
    VaiwuPet hina;
    aoi.startNew(CharacterId::Aoi, "", 0);
    hina.startNew(CharacterId::Hina, "", 0);
    aoi.tick(30 * kMinute);
    hina.tick(30 * kMinute);
    CHECK(aoi.stats().happiness > hina.stats().happiness);
    CHECK(aoi.name() == std::string("Aoi"));
}

void testFeeding()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Aoi, "A", 0);
    pet.tick(60 * kMinute); // hunger 100 -> 70
    const float before = pet.stats().hunger;
    CHECK(pet.feed() == ActionResult::Ok);
    CHECK(std::fabs(pet.stats().hunger - (before + 30.0f)) < 0.01f);
    CHECK(std::fabs(pet.stats().weightKg - 16.0f) < 0.01f);
    // full pet refuses
    CHECK(pet.feed() == ActionResult::NotHungry);
}

void testForcedSleepAndWake()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Aoi, "A", 0);
    std::uint64_t t = 0;
    // Energy 1%/min -> after 86 min energy <= 15 -> forced sleep.
    t += 90 * kMinute;
    pet.tick(t);
    CHECK(pet.life() == LifeState::Sleeping);
    CHECK(pet.emotion() == Emotion::Sleepy);
    CHECK(pet.feed() == ActionResult::RefusedSleeping);
    // Aoi sleep regen: 2%/min * 1.5 = 3%/min -> full in ~30 min.
    t += 40 * kMinute;
    pet.tick(t);
    CHECK(pet.life() == LifeState::Awake);
    CHECK(pet.stats().energy >= 99.0f);
}

void testWake()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Aoi, "A", 0);
    CHECK(!pet.wake()); // awake: nothing happens
    pet.tick(90 * kMinute);
    CHECK(pet.life() == LifeState::Sleeping);
    const float moodBefore = pet.stats().happiness;
    CHECK(pet.wake());
    CHECK(pet.life() == LifeState::Awake);
    CHECK(pet.stats().happiness < moodBefore); // grumpy about it
    // New phrase contexts exist in both languages.
    for (int c = 0; c < kCharacterCount; ++c) {
        for (auto ctx : {PhraseContext::Poked, PhraseContext::Woken}) {
            CHECK(offlinePhrase(static_cast<CharacterId>(c), ctx, 1, Lang::En) != nullptr);
            CHECK(std::string(offlinePhrase(static_cast<CharacterId>(c), ctx, 1, Lang::Ru)) != "?");
        }
    }
}

void testDeathByHunger()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Yuki, "Y", 0);
    // Hunger 0.5%/min awake; sleep halves it. Force plenty of time.
    pet.tick(1000 * kMinute);
    CHECK(pet.life() == LifeState::Dead);
    CHECK(pet.emotion() == Emotion::Dead);
    CHECK(pet.feed() == ActionResult::RefusedDead);
    CHECK(!pet.alive());
}

void testSicknessAndYukiResistance()
{
    VaiwuPet normal;
    normal.startNew(CharacterId::Aoi, "", 0);
    VaiwuPet resistant;
    resistant.startNew(CharacterId::Yuki, "", 0);
    // 185 min -> hunger ~7.5% (awake decay only while awake; both sleep the
    // same way, so relative comparison is fair).
    normal.tick(185 * kMinute);
    resistant.tick(185 * kMinute);
    CHECK(normal.isSick());
    CHECK(!resistant.isSick());
}

void testPlayFlow()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Hina, "H", 0);
    const float weightBefore = pet.stats().weightKg;
    const float energyBefore = pet.stats().energy;
    CHECK(pet.startPlay() == ActionResult::Ok);
    CHECK(std::fabs(pet.stats().weightKg - (weightBefore - 2.0f)) < 0.01f);
    CHECK(std::fabs(pet.stats().energy - (energyBefore - 8.0f)) < 0.01f); // cheap games
    const float happinessBefore = pet.stats().happiness;
    const float disciplineBefore = pet.stats().discipline;
    pet.applyPlayResult(true);
    // Hina: +20 * 1.3 = +26 happiness (clamped at 100).
    CHECK(pet.stats().happiness >= happinessBefore);
    CHECK(pet.stats().happiness <= 100.0f);
    // Symmetric discipline stakes: win +3, loss -3 (the old +2/-5 made every
    // round a net drain, so active play pushed discipline to caprice).
    CHECK(std::fabs(pet.stats().discipline - (disciplineBefore + 3.0f)) < 0.01f);
    pet.applyPlayResult(false);
    CHECK(std::fabs(pet.stats().discipline - disciplineBefore) < 0.01f);
}

void testSleepRestoresDiscipline()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Rin, "R", 0);
    // Drive Rin to forced sleep (energy 100 -> 15 at 1%/min = 85 minutes).
    std::uint64_t now = 0;
    while (pet.life() != LifeState::Sleeping && now < 400ULL * kMinute) {
        now += kMinute;
        pet.tick(now);
    }
    CHECK(pet.life() == LifeState::Sleeping);
    const float disciplineBefore = pet.stats().discipline;
    CHECK(disciplineBefore < 60.0f); // it decayed while awake
    // 10 minutes of sleep restore exactly +10 discipline (never erode it).
    for (int minute = 0; minute < 10; ++minute) {
        now += kMinute;
        pet.tick(now);
    }
    CHECK(pet.life() == LifeState::Sleeping);
    CHECK(std::fabs(pet.stats().discipline - (disciplineBefore + 10.0f)) < 0.01f);
    CHECK(pet.stats().discipline <= 100.0f);
}

void testCapriceRefusesFood()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Rin, "R", 0);
    // Rin loses discipline 3x faster when hungry. Run until discipline < 20.
    pet.tick(400 * kMinute);
    if (pet.alive() && pet.life() == LifeState::Awake && pet.stats().discipline < 20.0f) {
        CHECK(pet.feed() == ActionResult::RefusedCaprice);
        CHECK(pet.emotion() == Emotion::Angry);
    }
    // Treat bypasses caprice.
    if (pet.alive() && pet.life() == LifeState::Awake) {
        CHECK(pet.treat() == ActionResult::Ok);
    }
}

void testBodyShape()
{
    // bodyShape() is a pure function of weight - drive it via crafted saves.
    struct Case { const char* save; BodyShape expected; };
    const Case cases[] = {
        {"V1 1 80 50 90 50 5 0 0 0\nH", BodyShape::Thin},
        {"V1 1 80 50 90 50 15 0 0 0\nH", BodyShape::Normal},
        {"V1 1 80 50 90 50 45 0 0 0\nH", BodyShape::Chubby},
        {"V1 1 80 50 90 50 70 0 0 0\nH", BodyShape::Fat},
    };
    for (const Case& testCase : cases) {
        VaiwuPet pet;
        CHECK(pet.deserialize(testCase.save, 0));
        CHECK(pet.bodyShape() == testCase.expected);
    }
    // Feeding gains weight, playing burns it.
    VaiwuPet pet;
    pet.startNew(CharacterId::Hina, "H", 0);
    pet.tick(120 * 60 * 1000ULL); // make room for food
    static_cast<void>(pet.wake()); // 120 min drains Hina's energy to zero
    const float before = pet.stats().weightKg;
    CHECK(pet.feed() == ActionResult::Ok);
    CHECK(pet.stats().weightKg > before);
    VaiwuPet slim;
    slim.startNew(CharacterId::Aoi, "A", 0);
    for (int i = 0; i < 3; ++i) {
        static_cast<void>(slim.startPlay());
        slim.applyPlayResult(true);
    }
    CHECK(slim.stats().weightKg <= 9.01f);
    CHECK(slim.bodyShape() == BodyShape::Thin);
}

void testEconomy()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Hina, "H", 0);
    CHECK(pet.coins() == 0);
    pet.addCoins(10);
    CHECK(pet.coins() == 10);
    CHECK(!pet.spendCoins(20));      // too poor
    CHECK(pet.coins() == 10);
    CHECK(pet.spendCoins(6));
    CHECK(pet.coins() == 4);

    // Shop: affordable item applies its effect and deducts coins.
    pet.addCoins(20); // 24 total
    const float hungerBefore = pet.stats().hunger;
    const auto& tasty = shopItem(ShopItem::TastyFood);
    CHECK(buyShopItem(pet, ShopItem::TastyFood));
    CHECK(pet.coins() == 24 - tasty.price);
    CHECK(pet.stats().hunger >= hungerBefore); // gift raised hunger (or capped)

    // Every shop item has a sane price and a localized name.
    for (int i = 0; i < kShopItemCount; ++i) {
        const auto& info = shopItem(static_cast<ShopItem>(i));
        CHECK(info.price > 0 && info.price < 1000);
        CHECK(std::string(tr(Lang::Ru, info.nameId)) != "?");
    }
    // Broke pet cannot buy.
    VaiwuPet poor;
    poor.startNew(CharacterId::Aoi, "A", 0);
    CHECK(!buyShopItem(poor, ShopItem::Ribbon));

    // Medicine cures illness. Starve a pet until sick, then heal it.
    VaiwuPet ill;
    ill.startNew(CharacterId::Yuki, "Y", 0);
    CHECK(ill.deserialize("V2 0 8 50 90 50 15 0 0 1 30\nSick", 0)); // hunger 8 -> sick
    CHECK(ill.isSick());
    CHECK(ill.coins() == 30);
    CHECK(buyShopItem(ill, ShopItem::Medicine));
    CHECK(!ill.isSick());                     // cured
    // cure() directly is a no-op when healthy.
    VaiwuPet well;
    well.startNew(CharacterId::Hina, "H", 0);
    CHECK(!well.isSick());
    well.cure();
    CHECK(!well.isSick());
}

void testSerializeRoundTrip()
{
    VaiwuPet pet;
    pet.startNew(CharacterId::Rin, "My Rin-chan", 0);
    pet.tick(10 * kMinute);
    CHECK(pet.feed() == ActionResult::Ok);
    pet.addCoins(42);
    const std::string data = pet.serialize();
    CHECK(!data.empty());

    VaiwuPet restored;
    CHECK(restored.deserialize(data, 12345));
    CHECK(restored.character() == CharacterId::Rin);
    CHECK(restored.name() == std::string("My Rin-chan"));
    CHECK(std::fabs(restored.stats().hunger - pet.stats().hunger) < 0.02f);
    CHECK(std::fabs(restored.stats().weightKg - pet.stats().weightKg) < 0.02f);
    CHECK(restored.stats().ageSeconds == pet.stats().ageSeconds);
    CHECK(restored.coins() == pet.coins());
    CHECK(restored.life() == pet.life());

    // Old V1 saves still load (coins default to 0).
    VaiwuPet legacy;
    CHECK(legacy.deserialize("V1 1 80 50 90 50 15 0 0 0\nOld", 0));
    CHECK(legacy.coins() == 0);
    CHECK(legacy.name() == std::string("Old"));

    VaiwuPet bad;
    CHECK(!bad.deserialize("garbage", 0));
    CHECK(!bad.deserialize("", 0));
    CHECK(!bad.deserialize("V1 99 1 2 3 4 5 6 0 0\nX", 0)); // bad char id
}

void testOfflinePhrases()
{
    for (int langIndex = 0; langIndex < 2; ++langIndex) {
        for (int c = 0; c < kCharacterCount; ++c) {
            for (int ctx = 0; ctx <= static_cast<int>(PhraseContext::Fainted); ++ctx) {
                for (std::uint32_t salt = 0; salt < 4; ++salt) {
                    const char* phrase = offlinePhrase(static_cast<CharacterId>(c),
                        static_cast<PhraseContext>(ctx), salt, static_cast<Lang>(langIndex));
                    CHECK(phrase != nullptr);
                    CHECK(std::string(phrase).size() <= 110);
                    CHECK(!std::string(phrase).empty());
                }
            }
        }
    }
}

void testI18n()
{
    for (int langIndex = 0; langIndex < 2; ++langIndex) {
        const auto lang = static_cast<Lang>(langIndex);
        for (int i = 0; i < static_cast<int>(Str::Count_); ++i) {
            const char* text = tr(lang, static_cast<Str>(i));
            CHECK(text != nullptr);
            CHECK(std::string(text) != "?");
        }
    }
    // Russian phrases exist and differ from English ones.
    const char* en = offlinePhrase(CharacterId::Rin, PhraseContext::Fed, 0, Lang::En);
    const char* ru = offlinePhrase(CharacterId::Rin, PhraseContext::Fed, 0, Lang::Ru);
    CHECK(std::string(en) != std::string(ru));
}

void testShopCategories()
{
    int seen[kShopItemCount] = {};
    for (int c = 0; c < kShopCategoryCount; ++c) {
        const auto category = static_cast<ShopCategory>(c);
        CHECK(tr(Lang::En, shopCategoryName(category)) != nullptr);
        CHECK(tr(Lang::Ru, shopCategoryName(category)) != nullptr);
        int count = 0;
        const ShopItem* items = shopCategoryItems(category, count);
        CHECK(items != nullptr);
        CHECK(count > 0 && count <= kShopItemCount);
        for (int i = 0; i < count; ++i) {
            ++seen[static_cast<int>(items[i])];
        }
    }
    for (int i = 0; i < kShopItemCount; ++i) {
        CHECK(seen[i] == 1); // every item lives in exactly one category
    }
}

void testAssistantIntents()
{
    using vaiwu::game::IntentKind;
    // Alarm with explicit time, both languages.
    auto a = parseIntent("поставь будильник на 7:30", Lang::Ru);
    CHECK(a.kind == IntentKind::SetAlarm);
    CHECK(a.hour == 7 && a.minute == 30);
    auto b = parseIntent("set an alarm for 21:05", Lang::En);
    CHECK(b.kind == IntentKind::SetAlarm);
    CHECK(b.hour == 21 && b.minute == 5);
    // Alarm without time.
    CHECK(parseIntent("поставь будильник", Lang::Ru).kind == IntentKind::AlarmMissingTime);
    // Relative reminders.
    auto r = parseIntent("напомни через 10 минут позвонить маме", Lang::Ru);
    CHECK(r.kind == IntentKind::SetReminderIn);
    CHECK(r.afterMinutes == 10);
    auto r2 = parseIntent("remind me in 2 hours to drink water", Lang::En);
    CHECK(r2.kind == IntentKind::SetReminderIn);
    CHECK(r2.afterMinutes == 120);
    // Cancel, joke, time, free-form.
    CHECK(parseIntent("отмени все будильники", Lang::Ru).kind == IntentKind::CancelAlarms);
    CHECK(parseIntent("расскажи анекдот", Lang::Ru).kind == IntentKind::Joke);
    CHECK(parseIntent("tell me a joke", Lang::En).kind == IntentKind::Joke);
    CHECK(parseIntent("который час?", Lang::Ru).kind == IntentKind::AskTime);
    CHECK(parseIntent("what time is it", Lang::En).kind == IntentKind::AskTime);
    // weather / pomodoro / timer / convert
    CHECK(parseIntent("какая погода?", Lang::Ru).kind == IntentKind::AskWeather);
    CHECK(parseIntent("what's the weather", Lang::En).kind == IntentKind::AskWeather);
    CHECK(parseIntent("запусти помодоро", Lang::Ru).kind == IntentKind::Pomodoro);
    CHECK(parseIntent("поставь таймер на 5 минут", Lang::Ru).kind == IntentKind::SetReminderIn);
    {
        auto cv = parseIntent("переведи 10 км в мили", Lang::Ru);
        CHECK(cv.kind == IntentKind::Convert);
        CHECK(std::string(cv.convertResult).find("km") != std::string::npos);
    }
    {
        auto cv = parseIntent("convert 100 celsius to fahrenheit", Lang::En);
        CHECK(cv.kind == IntentKind::Convert);
        CHECK(std::string(cv.convertResult).find("212") != std::string::npos);
    }
    CHECK(parseIntent("какая столица Франции?", Lang::Ru).kind == IntentKind::None);
    // Jokes localized and non-empty.
    CHECK(std::string(jokeText(Lang::Ru, 3)).size() > 0);
    CHECK(std::string(jokeText(Lang::En, 3)).size() > 0);
}

void testBabbleVoice()
{
    // Every character has her own voice profile.
    const VoiceProfile aoi = voiceProfileFor(CharacterId::Aoi);
    const VoiceProfile hina = voiceProfileFor(CharacterId::Hina);
    CHECK(hina.basePitchHz > aoi.basePitchHz);        // Hina chirps higher
    CHECK(voiceProfileFor(CharacterId::Kolobok).basePitchHz < aoi.basePitchHz);

    // Latin and Cyrillic both produce audio; longer text = more blips.
    const auto shortEn = renderVoice("Hi!", aoi, 24000);
    const auto longEn = renderVoice("Hello, the clouds are pretty today", aoi, 24000);
    CHECK(!shortEn.empty() && shortEn.size() % 2 == 0);
    CHECK(longEn.size() > shortEn.size());
    const auto ru = renderVoice("\u041f\u0440\u0438\u0432\u0435\u0442, \u043a\u0430\u043a \u0434\u0435\u043b\u0430?", aoi, 24000);
    CHECK(!ru.empty());

    // Deterministic: the same phrase always sounds the same.
    CHECK(renderVoice("Hello", aoi, 24000) == renderVoice("Hello", aoi, 24000));
    // ...but different characters sound different.
    CHECK(renderVoice("Hello", aoi, 24000) != renderVoice("Hello", hina, 24000));

    // Bounded: even a wall of text stays under ~1.5 s at 24 kHz.
    const auto wall = renderVoice(
        "aaaa eeee iiii oooo uuuu aaaa eeee iiii oooo uuuu aaaa eeee", aoi, 24000);
    CHECK(wall.size() <= 24000 * 2 * 2);
    // Punctuation-only lines still get a small blip instead of silence.
    CHECK(!renderVoice("...", aoi, 24000).empty());
    CHECK(renderVoice(nullptr, aoi, 24000).empty());
}

void testSoundSynth()
{
    for (int i = 0; i <= static_cast<int>(Sfx::Note4); ++i) {
        const auto pcm = renderSfx(static_cast<Sfx>(i), 24000);
        CHECK(!pcm.empty());
        CHECK(pcm.size() % 2 == 0);
        CHECK(pcm.size() < 24000 * 2 * 2); // every effect under 2 seconds
    }
}

} // namespace

int main()
{
    testDecayRates();
    testAoiSlowHappinessDecay();
    testFeeding();
    testForcedSleepAndWake();
    testWake();
    testDeathByHunger();
    testSicknessAndYukiResistance();
    testPlayFlow();
    testSleepRestoresDiscipline();
    testCapriceRefusesFood();
    testBodyShape();
    testEconomy();
    testSerializeRoundTrip();
    testOfflinePhrases();
    testI18n();
    testShopCategories();
    testAssistantIntents();
    testSoundSynth();
    testBabbleVoice();

    if (failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
