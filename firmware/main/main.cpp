// Tamagotchi Vaiwu - firmware entry point.
//
// Target: Freenove ESP32-S3 2.8" capacitive-touch display board
// (board profile freenove-esp32s3-display-2.8-lcd from xiaozhi-esp32;
// pins below come from its board config).
//
// Composition root: creates every driver and service, loads the save,
// then hands control to the LVGL task (VaiwuUi timers drive the game).

#include "AlarmService.hpp"
#include "SoundService.hpp"
#include "VoiceService.hpp"
#include "WeatherService.hpp"
#include "WifiService.hpp"
#include "vaiwu/chat/NeuralChat.hpp"
#include "vaiwu/chat/SpeechToText.hpp"
#include "vaiwu/chat/TextToSpeech.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/VaiwuPet.hpp"
#include "vaiwu/hal/esp/Es8311Audio.hpp"
#include "vaiwu/hal/esp/EspSntpClock.hpp"
#include "vaiwu/hal/esp/EspWifiNetwork.hpp"
#include "vaiwu/hal/esp/OpenMeteoWeatherProvider.hpp"
#include "vaiwu/hal/esp/Ft6336Touch.hpp"
#include "vaiwu/hal/esp/NvsStorage.hpp"
#include "vaiwu/hal/esp/SharedI2cBus.hpp"
#include "vaiwu/services/ConsoleLogger.hpp"
#include "vaiwu/ui/DisplayDriver.hpp"
#include "vaiwu/ui/VaiwuUi.hpp"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Board pins (freenove-esp32s3-display-2.8-lcd, see the xiaozhi-esp32 board config).
constexpr int kI2cSda = 16; // shared: FT6336 touch + ES8311 codec control
constexpr int kI2cScl = 15;
constexpr int kI2sMclk = 4;
constexpr int kI2sBclk = 5;
constexpr int kI2sWs = 7;
constexpr int kI2sDout = 8;
constexpr int kI2sDin = 6;
constexpr int kAudioPaPin = 1; // active-low power amp enable
constexpr int kVolumePercent = 70; // default; Settings slider overrides via NVS "vol"

std::uint64_t nowMs()
{
#ifdef ESP_PLATFORM
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL);
#else
    return 0;
#endif
}

} // namespace

extern "C" void app_main(void)
{
    static vaiwu::services::ConsoleLogger logger;
    logger.info("Tamagotchi Vaiwu starting...");

    // --- persistent storage (NVS) -----------------------------------------
    static vaiwu::hal::esp::NvsStorage storage(logger, "vaiwu_cfg");
    const bool storageReady = storage.init();
    if (!storageReady) {
        logger.error("NVS init failed; progress will not be saved");
    }

    // Settings persisted by the UI: speaker volume / backlight brightness.
    const auto readPercent = [&](const char* key, int fallback) {
        if (storageReady) {
            if (const auto stored = storage.readText(key); stored.has_value() && !stored->empty()) {
                const int value = std::atoi(stored->c_str());
                return value < 0 ? 0 : (value > 100 ? 100 : value);
            }
        }
        return fallback;
    };
    const int volumePercent = readPercent("vol", kVolumePercent);
    const int brightnessPercent = readPercent("bri", 100);

    // --- display + LVGL -----------------------------------------------------
    static vaiwu::ui::DisplayDriver display(logger, vaiwu::ui::DisplayPins{});
    if (!display.init()) {
        logger.error("Display init failed; halting");
        return;
    }
    display.setBrightness(static_cast<std::uint8_t>(brightnessPercent));

    // --- shared I2C bus: touch + audio codec --------------------------------
    static vaiwu::hal::esp::SharedI2cBus i2cBus(logger, kI2cSda, kI2cScl);
    const bool i2cReady = i2cBus.init();

    // mirrorY = true: confirmed on real hardware - the
    // FT6336 on this board reports Y flipped relative to the post-swap frame.
    static vaiwu::hal::esp::Ft6336Touch touch(logger, i2cReady ? i2cBus.handle() : nullptr,
        /* i2cAddr = */ 0x38, /* mirrorX = */ false, /* mirrorY = */ true);
    if (!i2cReady || !touch.init()) {
        logger.error("Touch init failed; the game will not be controllable!");
    }

    static vaiwu::hal::esp::Es8311Audio audio(logger,
        i2cReady ? i2cBus.handle() : nullptr,
        kI2sMclk, kI2sBclk, kI2sWs, kI2sDout, kI2sDin, kAudioPaPin);
    if (audio.init()) {
        audio.setVolume(static_cast<std::uint8_t>(volumePercent));
    } else {
        logger.warn("Audio init failed; continuing without sound");
    }

    static vaiwu::SoundService sound(logger, audio);
    static_cast<void>(sound.init());

    // --- network: stored credentials (on-screen setup) beat Kconfig ----------
    std::string ssid;
    std::string password;
#ifdef ESP_PLATFORM
    ssid = CONFIG_VAIWU_WIFI_SSID;
    password = CONFIG_VAIWU_WIFI_PASSWORD;
#endif
    if (storageReady) {
        if (const auto stored = storage.readText("wifi_ssid"); stored.has_value() && !stored->empty()) {
            ssid = *stored;
            password = storage.readText("wifi_pass").value_or("");
        }
    }

    // --- wall clock (SNTP): initialized lazily on the Wi-Fi task right
    // after the first successful connect, so boot never blocks on NTP.
    static vaiwu::hal::esp::EspSntpClock clock(logger);

    static vaiwu::hal::esp::EspWifiNetwork network(ssid, password);
    static vaiwu::WifiService wifi(logger, network, storage,
        []() { static_cast<void>(clock.init()); });
    static_cast<void>(wifi.init());
    if (!ssid.empty()) {
        wifi.requestAutoConnect(); // background; game runs offline meanwhile
    } else {
        logger.info("No Wi-Fi credentials; offline phrases only (set up in Settings)");
    }

    static vaiwu::chat::NeuralChat chat(logger, network);
    if (!chat.init()) {
        logger.warn("Chat worker init failed; offline phrases only");
    }

    // --- voice: microphone -> Whisper STT -> intent parser / chat ------------
    static vaiwu::chat::SpeechToText stt(logger);
    static vaiwu::VoiceService voice(logger, audio, stt);
    static_cast<void>(voice.init());
    if (!voice.available()) {
        logger.info("No STT key configured; voice input disabled (typing still works)");
    }

    // --- weather skill (Open-Meteo, free, no key) ---------------------------
#ifdef ESP_PLATFORM
    static vaiwu::hal::esp::OpenMeteoWeatherProvider weatherProvider(logger,
        CONFIG_VAIWU_WEATHER_CITY,
        std::atof(CONFIG_VAIWU_WEATHER_LAT), std::atof(CONFIG_VAIWU_WEATHER_LON));
#else
    static vaiwu::hal::esp::OpenMeteoWeatherProvider weatherProvider(logger);
#endif
    static_cast<void>(weatherProvider.init());
    static vaiwu::WeatherService weather(logger, weatherProvider);
    static_cast<void>(weather.init());

    // --- text-to-speech (spoken replies; off unless a TTS URL is set) -------
    static vaiwu::chat::TextToSpeech tts(logger, audio);
    static_cast<void>(tts.init());
    if (!vaiwu::chat::TextToSpeech::configured()) {
        logger.info("No TTS URL configured; replies are shown as text only");
    }

    // --- the pet itself ------------------------------------------------------
    static vaiwu::game::VaiwuPet pet;
    bool haveSave = false;
    if (storageReady) {
        if (const auto saved = storage.readText("pet"); saved.has_value() && !saved->empty()) {
            haveSave = pet.deserialize(*saved, nowMs());
            if (!haveSave) {
                logger.warn("Save data corrupt; starting fresh");
            }
        }
    }

    // Russian is the default UI language; "0" in NVS switches to English.
    vaiwu::game::Lang lang = vaiwu::game::Lang::Ru;
    if (storageReady) {
        if (const auto stored = storage.readText("lang"); stored.has_value() && *stored == "0") {
            lang = vaiwu::game::Lang::En;
        }
    }

    // --- UI -------------------------------------------------------------------
    vaiwu::ui::VaiwuUi::Services services;
    services.pet = &pet;
    services.chat = &chat;
    services.storage = &storage;
    services.network = &network;
    services.clock = &clock;
    services.logger = &logger;
    services.initialLang = lang;
    services.playSfx = [](vaiwu::game::Sfx effect) { sound.play(effect); };
    services.readTouch = [](bool& touched, std::int16_t& x, std::int16_t& y) {
        return touch.readLogical(touched, x, y);
    };
    services.nowMs = []() { return nowMs(); };
    services.wifiRequestScan = []() { wifi.requestScan(); };
    services.wifiRequestConnect = [](const char* s, const char* p) { wifi.requestConnect(s, p); };
    services.wifiState = []() { return static_cast<int>(wifi.state()); };
    services.wifiScanResults = []() { return wifi.scanResults(); };
    services.voiceAvailable = []() { return voice.available(); };
    services.voiceStart = [](int lang) {
        voice.startRecording(lang == 1 ? vaiwu::game::Lang::Ru : vaiwu::game::Lang::En);
    };
    services.voiceStop = []() { voice.stopRecording(); };
    services.voiceState = []() { return static_cast<int>(voice.state()); };
    services.voiceTake = []() { return voice.takeTranscript(); };
    services.weatherRequest = []() { weather.request(); };
    services.weatherState = []() { return static_cast<int>(weather.state()); };
    services.weatherTake = []() { return weather.take(); };
    services.speak = [](const char* text, int lang) {
        tts.speak(text, lang == 1 ? vaiwu::game::Lang::Ru : vaiwu::game::Lang::En);
    };
    services.babble = [](const char* text) { sound.babble(text, pet.character()); };
    services.setVolume = [](int percent) {
        audio.setVolume(static_cast<std::uint8_t>(percent < 0 ? 0 : (percent > 100 ? 100 : percent)));
    };
    services.setBrightness = [](int percent) {
        display.setBrightness(static_cast<std::uint8_t>(percent < 0 ? 0 : (percent > 100 ? 100 : percent)));
    };

    // Assistant alarms/reminders. Polled from the UI's 1 s timer via dueAlarm.
    static vaiwu::AlarmService alarms;
    services.setAlarmAt = [](int hour, int minute) { return alarms.addAt(hour, minute); };
    services.setReminderAfter = [](int minutes) { return alarms.addAfter(minutes, nowMs()); };
    services.cancelAlarms = []() { alarms.cancelAll(); };
    services.dueAlarm = []() {
        const vaiwu::ClockReading reading = clock.now();
        return alarms.due(reading, nowMs());
    };

    static vaiwu::ui::VaiwuUi ui(services);
#ifdef ESP_PLATFORM
    {
        char heap[96];
        std::snprintf(heap, sizeof(heap), "Free heap before UI: %u internal, %u total",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
        logger.info(heap);
    }
#endif
    if (!ui.init(haveSave)) {
        logger.error("UI init failed; halting");
        return;
    }
#ifdef ESP_PLATFORM
    {
        char heap[96];
        std::snprintf(heap, sizeof(heap), "Free heap after UI: %u internal, %u total",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
        logger.info(heap);
    }
#endif

    logger.info("Tamagotchi Vaiwu is running");
    // Everything from here on is event-driven on the LVGL task.
}
