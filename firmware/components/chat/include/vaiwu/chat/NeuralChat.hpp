#pragma once

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/OfflinePhrases.hpp"
#include "vaiwu/game/VaiwuPet.hpp"
#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/INetwork.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#ifdef ESP_PLATFORM
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace vaiwu::chat {

// Everything the prompt template needs. Plain fixed-size fields so the
// struct can be copied between tasks without heap traffic.
struct PromptContext {
    game::CharacterId character{game::CharacterId::Aoi};
    char petName[32]{};
    char action[48]{};   // e.g. "fed you", "hugged you", English, ASCII
    // Full tracked pet state, woven into the system prompt so replies reflect
    // how she actually feels right now (see NeuralChat.cpp).
    int hunger{0};
    int happiness{0};
    int energy{0};
    int discipline{0};
    int weightKg{0};
    bool sick{false};
    bool sleeping{false};
    game::Emotion emotion{game::Emotion::Idle}; // worded mood in the prompt
    game::PhraseContext fallback{game::PhraseContext::Talked};
    game::Lang lang{game::Lang::En}; // reply language (offline fallback + prompt)
    // Assistant mode: free-form question in `query` answered helpfully
    // (still in character); offline fallback is Str::OfflineAnswer.
    bool assistantMode{false};
    char query[160]{};
};

/**
 * Purpose: the pet's "brain" - OpenRouter chat completions when online,
 *          canned offline phrases otherwise (spec's two chat modes).
 * Responsibilities: build the roleplay prompt from a PromptContext, run the
 *                    HTTPS request on a background worker task (never on the
 *                    LVGL/UI task; 3 s timeout per spec), sanitize the answer
 *                    to plain ASCII that the UI font can draw, and fall back
 *                    to game::offlinePhrase() on any failure. Replies are
 *                    handed over via a poll-based mailbox (takeReply()) so
 *                    the UI thread never blocks.
 * Public API: init(), requestReply(), takeReply().
 */
class NeuralChat {
public:
    NeuralChat(ILogger& logger, INetwork& network);
    ~NeuralChat();

    NeuralChat(const NeuralChat&) = delete;
    NeuralChat& operator=(const NeuralChat&) = delete;

    [[nodiscard]] bool init();

    // Queues one reply request. If a previous request is still in flight it
    // is replaced (the pet only ever says the latest thing). Offline or
    // key-less setups get an instant canned phrase instead.
    void requestReply(const PromptContext& context);

    // Non-blocking: returns the pending reply once, then std::nullopt.
    [[nodiscard]] std::optional<std::string> takeReply();

private:
    void deliver(const std::string& text);
    void deliverOffline(const PromptContext& context);
    [[nodiscard]] bool onlineLikely() const;

    ILogger& logger_;
    INetwork& network_;

    mutable std::mutex mutex_;
    std::string reply_{};
    bool hasReply_{false};
    PromptContext pending_{};
    bool hasPending_{false};
    std::uint32_t salt_{0};

#ifdef ESP_PLATFORM
    [[nodiscard]] std::string performOnlineRequest(const PromptContext& context);
    static esp_err_t httpEventHandler(esp_http_client_event_t* event);
    static void taskEntry(void* arg);
    void taskLoop();

    SemaphoreHandle_t wakeSem_{nullptr};
    std::string responseBuffer_{};
#endif
};

} // namespace vaiwu::chat
