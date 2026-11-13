#pragma once

#include "vaiwu/chat/NeuralChat.hpp"
#include "vaiwu/game/Assistant.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/SoundSynth.hpp"
#include "vaiwu/game/VaiwuPet.hpp"
#include "vaiwu/interfaces/IClock.hpp"
#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/INetwork.hpp"
#include "vaiwu/interfaces/IStorage.hpp"
#include "vaiwu/interfaces/IWeatherProvider.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace vaiwu::ui {

/**
 * Purpose: the whole game UI on one 320x240 LVGL display: character select
 *          (with on-screen keyboard), main game screen (stat bars, clock,
 *          sprite, 5 actions), dialog overlay, a game-select menu with three
 *          mini-games (guess emotion / catch hearts / repeat melody), death
 *          screen, and a settings area (language RU/EN, Wi-Fi setup with
 *          scan + password keyboard, full reset).
 * Responsibilities: build every widget once at init, register the touch
 *                    panel as an LVGL pointer indev, and drive everything
 *                    from two LVGL timers (1 s simulation/autosave/chat/Wi-Fi
 *                    polling; 100 ms game countdowns and animations). All
 *                    LVGL work stays on the LVGL task; blocking work (chat
 *                    HTTP, Wi-Fi ops, audio) lives behind the injected
 *                    service handles.
 * Public API: init().
 *
 * Only one instance may exist (LVGL callbacks route through it).
 */
class VaiwuUi {
public:
    struct Services {
        game::VaiwuPet* pet{nullptr};
        chat::NeuralChat* chat{nullptr};
        IStorage* storage{nullptr};
        INetwork* network{nullptr};
        IClock* clock{nullptr};
        ILogger* logger{nullptr};
        game::Lang initialLang{game::Lang::En};
        // Fire-and-forget sound effect trigger (SoundService queue).
        std::function<void(game::Sfx)> playSfx{};
        // Ft6336Touch::readLogical wrapper for the LVGL pointer indev.
        std::function<bool(bool&, std::int16_t&, std::int16_t&)> readTouch{};
        // Monotonic milliseconds (esp_timer based).
        std::function<std::uint64_t()> nowMs{};
        // WifiService bridge (values of wifiState mirror WifiService::State:
        // 0 Idle, 1 Scanning, 2 ScanDone, 3 Connecting, 4 Connected, 5 Failed).
        std::function<void()> wifiRequestScan{};
        std::function<void(const char*, const char*)> wifiRequestConnect{};
        std::function<int()> wifiState{};
        std::function<std::vector<WifiNetworkInfo>()> wifiScanResults{};
        // Assistant alarms/reminders (AlarmService bridge). setAlarmAt returns
        // false if the alarm store is full; setReminderAfter likewise. dueAlarm
        // is polled once per second and returns true when one just fired.
        std::function<bool(int, int)> setAlarmAt{};
        std::function<bool(int)> setReminderAfter{};
        std::function<void()> cancelAlarms{};
        std::function<bool()> dueAlarm{};
        // Push-to-talk voice input (VoiceService bridge). voiceAvailable hides
        // the mic button when no STT key is configured. voiceStart takes the
        // language (0 = En, 1 = Ru); state mirrors VoiceService::State
        // (0 Idle,1 Recording,2 Transcribing,3 Done,4 Error); voiceTake pulls
        // the recognized text once it is ready.
        std::function<bool()> voiceAvailable{};
        std::function<void(int)> voiceStart{};
        std::function<void()> voiceStop{};
        std::function<int()> voiceState{};
        std::function<std::string()> voiceTake{};
        // Weather skill (WeatherService bridge). state: 0 Idle,1 Fetching,
        // 2 Done,3 Error. take() returns the latest reading.
        std::function<void()> weatherRequest{};
        std::function<int()> weatherState{};
        std::function<WeatherReading()> weatherTake{};
        // Optional spoken replies (TextToSpeech bridge). text is UTF-8,
        // lang: 0 En / 1 Ru. No-op when TTS is not configured.
        std::function<void(const char*, int)> speak{};
        // Offline "babble" voice for the pet's own lines (SoundService).
        // Takes the phrase; the character is read from the pet.
        std::function<void(const char*)> babble{};
        // Settings sliders: speaker volume and screen backlight, 0-100%.
        // The UI persists the values in NVS ("vol"/"bri") and applies them
        // live through these; main.cpp re-applies them at boot.
        std::function<void(int)> setVolume{};
        std::function<void(int)> setBrightness{};
        // OTA firmware update (OtaService bridge). state: 0 Idle,
        // 1 Checking, 2 UpToDate, 3 Available, 4 Downloading (see progress),
        // 5 ReadyToReboot, 6 Failed, 7 Disabled. otaTag = latest release tag.
        std::function<void()> otaCheck{};
        std::function<void()> otaUpdate{};
        std::function<int()> otaState{};
        std::function<int()> otaProgress{};
        std::function<const char*()> otaTag{};
        std::function<void()> otaReboot{};
    };

    explicit VaiwuUi(Services services);

    // haveSave: jump straight to the main screen with the loaded pet;
    // otherwise start at the character select screen.
    [[nodiscard]] bool init(bool haveSave);

private:
    Services services_;
};

} // namespace vaiwu::ui
