#include "vaiwu/chat/NeuralChat.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_crt_bundle.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <cJSON.h>
#endif

namespace vaiwu::chat {

namespace {
constexpr std::size_t kMaxReplyBytes = 140;    // speech-bubble budget (UTF-8 bytes)
constexpr std::size_t kMaxAnswerBytes = 300;   // chat-screen answer budget

// Keep printable ASCII plus UTF-8 multibyte sequences (Cyrillic replies are
// drawn by the generated vaiwu fonts); control chars and newlines collapse
// into single spaces, then trim and clamp to the bubble budget (in bytes,
// cut only at a UTF-8 character boundary).
// (Only referenced from the ESP-only online path; maybe_unused keeps the
// host smoke build warning-clean.)
[[maybe_unused]] [[nodiscard]] std::string sanitizeReply(const std::string& raw, std::size_t maxBytes = kMaxReplyBytes)
{
    std::string out;
    out.reserve(raw.size());
    bool lastWasSpace = true;
    for (const char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        const bool keep = (uc >= 0x20 && uc < 0x7F) || uc >= 0x80;
        const char mapped = keep ? c : ' ';
        if (mapped == ' ' && lastWasSpace) {
            continue;
        }
        lastWasSpace = mapped == ' ';
        out.push_back(mapped);
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    // Strip a single pair of surrounding quotes many models like to add.
    if (out.size() >= 2 && out.front() == '"' && out.back() == '"') {
        out = out.substr(1, out.size() - 2);
    }
    if (out.size() > maxBytes) {
        std::size_t cut = maxBytes - 3;
        // Do not cut inside a UTF-8 multibyte sequence.
        while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
            --cut;
        }
        out.resize(cut);
        out += "...";
    }
    return out;
}
} // namespace

NeuralChat::NeuralChat(ILogger& logger, INetwork& network)
    : logger_(logger)
    , network_(network)
{
}

NeuralChat::~NeuralChat() = default;

bool NeuralChat::init()
{
#ifdef ESP_PLATFORM
    wakeSem_ = xSemaphoreCreateBinary();
    if (wakeSem_ == nullptr) {
        logger_.error("NeuralChat: failed to create semaphore");
        return false;
    }
    // 8 KB stack: TLS handshake via esp_http_client happens on this task.
    if (xTaskCreate(&NeuralChat::taskEntry, "vaiwu_chat", 8192, this, 3, nullptr) != pdPASS) {
        logger_.error("NeuralChat: failed to create worker task");
        return false;
    }
#endif
    return true;
}

bool NeuralChat::onlineLikely() const
{
#ifdef ESP_PLATFORM
    if (std::strlen(CONFIG_VAIWU_OPENROUTER_API_KEY) == 0) {
        return false;
    }
    return network_.status().status == NetworkStatus::Connected;
#else
    return false;
#endif
}

void NeuralChat::requestReply(const PromptContext& context)
{
    ++salt_;
    if (!onlineLikely()) {
        deliverOffline(context);
        return;
    }
#ifdef ESP_PLATFORM
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        pending_ = context;
        hasPending_ = true;
    }
    xSemaphoreGive(wakeSem_);
#endif
}

std::optional<std::string> NeuralChat::takeReply()
{
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!hasReply_) {
        return std::nullopt;
    }
    hasReply_ = false;
    return reply_;
}

void NeuralChat::deliver(const std::string& text)
{
    const std::lock_guard<std::mutex> lock(mutex_);
    reply_ = text;
    hasReply_ = true;
}

void NeuralChat::deliverOffline(const PromptContext& context)
{
    if (context.assistantMode) {
        deliver(game::tr(context.lang, game::Str::OfflineAnswer));
        return;
    }
    deliver(game::offlinePhrase(context.character, context.fallback, salt_, context.lang));
}

#ifdef ESP_PLATFORM

void NeuralChat::taskEntry(void* arg)
{
    static_cast<NeuralChat*>(arg)->taskLoop();
}

void NeuralChat::taskLoop()
{
    for (;;) {
        if (xSemaphoreTake(wakeSem_, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        PromptContext context;
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (!hasPending_) {
                continue;
            }
            context = pending_;
            hasPending_ = false;
        }
        const std::string answer = performOnlineRequest(context);
        if (answer.empty()) {
            deliverOffline(context);
        } else {
            deliver(answer);
        }
    }
}

esp_err_t NeuralChat::httpEventHandler(esp_http_client_event_t* event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->user_data != nullptr) {
        auto* self = static_cast<NeuralChat*>(event->user_data);
        self->responseBuffer_.append(static_cast<const char*>(event->data),
            static_cast<std::size_t>(event->data_len));
    }
    return ESP_OK;
}

namespace {
// A short English mood word for the prompt, derived from the current emotion.
const char* moodWord(game::Emotion emotion)
{
    switch (emotion) {
    case game::Emotion::Happy: return "happy and cheerful";
    case game::Emotion::Sad: return "sad and down";
    case game::Emotion::Angry: return "grumpy and irritated";
    case game::Emotion::Sleepy: return "sleepy";
    case game::Emotion::Sick: return "unwell and weak";
    case game::Emotion::Dead: return "gone";
    case game::Emotion::Idle: break;
    }
    return "calm and content";
}

// One human-readable line describing every tracked stat, shared by both the
// pet-reaction and assistant prompts so replies match how she really feels.
void buildStateLine(char* out, std::size_t size, const PromptContext& c)
{
    std::snprintf(out, size,
        "Right now she feels %s%s%s: hunger %d%%, happiness %d%%, energy %d%%, "
        "discipline %d%%, weight %d kg.",
        moodWord(c.emotion),
        c.sick ? " and is sick" : "",
        c.sleeping ? " and is asleep" : "",
        c.hunger, c.happiness, c.energy, c.discipline, c.weightKg);
}
} // namespace

std::string NeuralChat::performOnlineRequest(const PromptContext& context)
{
    const game::CharacterTraits& traits = game::traitsFor(context.character);

    const char* replyLanguage = context.lang == game::Lang::Ru ? "Russian" : "English";
    char stateLine[220];
    buildStateLine(stateLine, sizeof(stateLine), context);
    char systemPrompt[640];
    char userPrompt[176];
    if (context.assistantMode) {
        std::snprintf(systemPrompt, sizeof(systemPrompt),
            "You are %s, a virtual waifu assistant. Your personality: %s. "
            "%s Answer the user's question or request helpfully and correctly, "
            "in %s, staying lightly in character and letting your current mood "
            "colour your tone. 40 words maximum, plain text, no emoji, no markdown.",
            context.petName, traits.personality, stateLine, replyLanguage);
        std::snprintf(userPrompt, sizeof(userPrompt), "%s", context.query);
    } else {
        std::snprintf(systemPrompt, sizeof(systemPrompt),
            "You are a virtual waifu pet named %s. Your personality: %s. "
            "Your favorite food: %s. %s "
            "Reply with ONE short emotional phrase that fits your current mood, "
            "in character, in %s, 10 words maximum, no emoji, no quotes.",
            context.petName, traits.personality, traits.favoriteFood,
            stateLine, replyLanguage);
        std::snprintf(userPrompt, sizeof(userPrompt), "The user just %s.", context.action);
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", CONFIG_VAIWU_OPENROUTER_MODEL);
    cJSON_AddNumberToObject(root, "max_tokens", context.assistantMode ? 140 : 60);
    cJSON* messages = cJSON_AddArrayToObject(root, "messages");
    cJSON* sys = cJSON_CreateObject();
    cJSON_AddStringToObject(sys, "role", "system");
    cJSON_AddStringToObject(sys, "content", systemPrompt);
    cJSON_AddItemToArray(messages, sys);
    cJSON* user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "role", "user");
    cJSON_AddStringToObject(user, "content", userPrompt);
    cJSON_AddItemToArray(messages, user);

    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr) {
        logger_.error("NeuralChat: failed to build request JSON");
        return {};
    }

    responseBuffer_.clear();

    esp_http_client_config_t config = {};
    config.url = "https://openrouter.ai/api/v1/chat/completions";
    config.method = HTTP_METHOD_POST;
    config.event_handler = &NeuralChat::httpEventHandler;
    config.user_data = this;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 3000; // spec: 3 s budget for the reply

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        cJSON_free(body);
        logger_.error("NeuralChat: failed to create HTTP client");
        return {};
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    char auth[192];
    std::snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_VAIWU_OPENROUTER_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_post_field(client, body, static_cast<int>(std::strlen(body)));

    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    cJSON_free(body);

    if (result != ESP_OK) {
        logger_.warn(std::string("NeuralChat: request failed: ") + esp_err_to_name(result));
        return {};
    }
    if (status != 200 || responseBuffer_.empty()) {
        logger_.warn("NeuralChat: bad HTTP status " + std::to_string(status));
        return {};
    }

    cJSON* response = cJSON_Parse(responseBuffer_.c_str());
    if (response == nullptr) {
        logger_.warn("NeuralChat: failed to parse response JSON");
        return {};
    }

    std::string text;
    const cJSON* choices = cJSON_GetObjectItemCaseSensitive(response, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        const cJSON* first = cJSON_GetArrayItem(choices, 0);
        const cJSON* message = cJSON_GetObjectItemCaseSensitive(first, "message");
        const cJSON* content = cJSON_GetObjectItemCaseSensitive(message, "content");
        if (cJSON_IsString(content) && content->valuestring != nullptr) {
            text = sanitizeReply(content->valuestring,
                context.assistantMode ? kMaxAnswerBytes : kMaxReplyBytes);
        }
    }
    cJSON_Delete(response);

    if (text.empty()) {
        logger_.warn("NeuralChat: response had no usable text");
    }
    return text;
}

#endif // ESP_PLATFORM

} // namespace vaiwu::chat
