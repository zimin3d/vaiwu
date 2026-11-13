#include "vaiwu/chat/TextToSpeech.hpp"

#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "sdkconfig.h"
#include <cJSON.h>
#endif

namespace vaiwu::chat {

TextToSpeech::TextToSpeech(ILogger& logger, IAudio& audio)
    : logger_(logger)
    , audio_(audio)
{
}

bool TextToSpeech::configured()
{
#ifdef ESP_PLATFORM
    return std::strlen(CONFIG_VAIWU_TTS_URL) > 0;
#else
    return false;
#endif
}

bool TextToSpeech::init()
{
#ifdef ESP_PLATFORM
    if (!configured()) {
        return true; // feature off; nothing to start
    }
    queue_ = xQueueCreate(3, sizeof(Job));
    if (queue_ == nullptr) {
        logger_.error("TTS: queue creation failed");
        return false;
    }
    if (xTaskCreate(&TextToSpeech::taskEntry, "vaiwu_tts", 8192, this, 3, nullptr) != pdPASS) {
        logger_.error("TTS: task creation failed");
        return false;
    }
#endif
    return true;
}

void TextToSpeech::speak(const std::string& text, game::Lang lang)
{
#ifdef ESP_PLATFORM
    if (queue_ == nullptr || text.empty()) {
        return;
    }
    Job job{};
    std::snprintf(job.text, sizeof(job.text), "%s", text.c_str());
    job.lang = static_cast<int>(lang);
    static_cast<void>(xQueueSend(queue_, &job, 0));
#else
    static_cast<void>(text);
    static_cast<void>(lang);
#endif
}

#ifdef ESP_PLATFORM

void TextToSpeech::taskEntry(void* arg)
{
    static_cast<TextToSpeech*>(arg)->taskLoop();
}

void TextToSpeech::taskLoop()
{
    for (;;) {
        Job job{};
        if (xQueueReceive(queue_, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        static_cast<void>(synthesizeAndPlay(job));
    }
}

bool TextToSpeech::synthesizeAndPlay(const Job& job)
{
    // Request raw PCM16 mono at the codec's rate so playback needs no decoder.
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", CONFIG_VAIWU_TTS_MODEL);
    cJSON_AddStringToObject(root, "voice", CONFIG_VAIWU_TTS_VOICE);
    cJSON_AddStringToObject(root, "input", job.text);
    cJSON_AddStringToObject(root, "response_format", "pcm");
    cJSON_AddNumberToObject(root, "sample_rate", static_cast<double>(kAudioSampleRateHz));
    char* body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (body == nullptr) {
        logger_.error("TTS: failed to build request JSON");
        return false;
    }

    responseBuffer_.clear();

    esp_http_client_config_t config = {};
    config.url = CONFIG_VAIWU_TTS_URL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 15000;
    config.buffer_size = 2048;
    if (std::strncmp(config.url, "https", 5) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        cJSON_free(body);
        logger_.error("TTS: failed to create HTTP client");
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (std::strlen(CONFIG_VAIWU_TTS_API_KEY) > 0) {
        char auth[192];
        std::snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_VAIWU_TTS_API_KEY);
        esp_http_client_set_header(client, "Authorization", auth);
    }
    esp_http_client_set_header(client, "Accept", "audio/pcm");

    // Stream the response body directly into memory (a sentence of PCM is a
    // few hundred KB; PSRAM has room).
    esp_err_t err = esp_http_client_open(client, static_cast<int>(std::strlen(body)));
    if (err != ESP_OK) {
        logger_.warn(std::string("TTS: connection failed: ") + esp_err_to_name(err));
        esp_http_client_cleanup(client);
        cJSON_free(body);
        return false;
    }
    esp_http_client_write(client, body, static_cast<int>(std::strlen(body)));
    cJSON_free(body);

    const int contentLength = esp_http_client_fetch_headers(client);
    static_cast<void>(contentLength);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        logger_.warn("TTS: bad HTTP status " + std::to_string(status));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    AudioBuffer pcm;
    pcm.reserve(64 * 1024);
    char chunk[1024];
    int read = 0;
    while ((read = esp_http_client_read(client, chunk, sizeof(chunk))) > 0) {
        pcm.insert(pcm.end(), chunk, chunk + read);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (pcm.size() < 4) {
        logger_.warn("TTS: empty audio response");
        return false;
    }
    // Some servers return a WAV container instead of raw PCM; skip its 44-byte
    // header so playback isn't corrupted by the "RIFF...data" preamble.
    std::size_t offset = 0;
    if (pcm.size() > 44 && std::memcmp(pcm.data(), "RIFF", 4) == 0) {
        offset = 44;
    }
    const AudioBuffer body16(pcm.begin() + static_cast<std::ptrdiff_t>(offset), pcm.end());
    return audio_.play(body16);
}

#endif // ESP_PLATFORM

} // namespace vaiwu::chat
