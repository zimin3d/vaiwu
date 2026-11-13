#include "vaiwu/chat/SpeechToText.hpp"

#include <cstring>

#ifdef ESP_PLATFORM
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "sdkconfig.h"
#include <cJSON.h>
#endif

namespace vaiwu::chat {

namespace {
void put32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}
void put16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}
void putStr(std::vector<std::uint8_t>& out, const char* text)
{
    for (const char* p = text; *p != '\0'; ++p) {
        out.push_back(static_cast<std::uint8_t>(*p));
    }
}
} // namespace

std::vector<std::uint8_t> makeWav(const std::vector<std::uint8_t>& pcm, std::uint32_t sampleRateHz)
{
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(pcm.size());
    const std::uint16_t channels = 1;
    const std::uint16_t bitsPerSample = 16;
    const std::uint32_t byteRate = sampleRateHz * channels * (bitsPerSample / 8);
    const std::uint16_t blockAlign = channels * (bitsPerSample / 8);

    std::vector<std::uint8_t> wav;
    wav.reserve(44 + pcm.size());
    putStr(wav, "RIFF");
    put32(wav, 36 + dataBytes);
    putStr(wav, "WAVE");
    putStr(wav, "fmt ");
    put32(wav, 16);            // PCM fmt chunk size
    put16(wav, 1);             // audio format = PCM
    put16(wav, channels);
    put32(wav, sampleRateHz);
    put32(wav, byteRate);
    put16(wav, blockAlign);
    put16(wav, bitsPerSample);
    putStr(wav, "data");
    put32(wav, dataBytes);
    wav.insert(wav.end(), pcm.begin(), pcm.end());
    return wav;
}

SpeechToText::SpeechToText(ILogger& logger)
    : logger_(logger)
{
}

bool SpeechToText::configured()
{
#ifdef ESP_PLATFORM
    return std::strlen(CONFIG_VAIWU_STT_API_KEY) > 0;
#else
    return false;
#endif
}

#ifdef ESP_PLATFORM
namespace {
esp_err_t sttHttpEvent(esp_http_client_event_t* event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && event->user_data != nullptr) {
        auto* buffer = static_cast<std::string*>(event->user_data);
        buffer->append(static_cast<const char*>(event->data), static_cast<std::size_t>(event->data_len));
    }
    return ESP_OK;
}
} // namespace
#endif

std::string SpeechToText::transcribe(const std::vector<std::uint8_t>& pcm,
    std::uint32_t sampleRateHz, game::Lang lang)
{
#ifndef ESP_PLATFORM
    static_cast<void>(pcm);
    static_cast<void>(sampleRateHz);
    static_cast<void>(lang);
    return {};
#else
    if (!configured() || pcm.empty()) {
        return {};
    }

    const std::vector<std::uint8_t> wav = makeWav(pcm, sampleRateHz);

    // Build a multipart/form-data body: model, language, file (audio/wav).
    const char* boundary = "----vaiwuvoice8f3a2b1c";
    const char* langCode = lang == game::Lang::Ru ? "ru" : "en";

    std::string head;
    head += "--";
    head += boundary;
    head += "\r\nContent-Disposition: form-data; name=\"model\"\r\n\r\n";
    head += CONFIG_VAIWU_STT_MODEL;
    head += "\r\n--";
    head += boundary;
    head += "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\n";
    head += langCode;
    head += "\r\n--";
    head += boundary;
    // Ask for a JSON body ({"text": ...}); OpenAI, whisper.cpp and FunASR
    // servers all honour this, so our parser works against any of them.
    head += "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\n";
    head += "json";
    head += "\r\n--";
    head += boundary;
    head += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
    head += "Content-Type: audio/wav\r\n\r\n";

    std::string tail = "\r\n--";
    tail += boundary;
    tail += "--\r\n";

    std::vector<std::uint8_t> body;
    body.reserve(head.size() + wav.size() + tail.size());
    body.insert(body.end(), head.begin(), head.end());
    body.insert(body.end(), wav.begin(), wav.end());
    body.insert(body.end(), tail.begin(), tail.end());

    responseBuffer_.clear();

    esp_http_client_config_t config = {};
    config.url = CONFIG_VAIWU_STT_URL;
    config.method = HTTP_METHOD_POST;
    config.event_handler = &sttHttpEvent;
    config.user_data = &responseBuffer_;
    // TLS only for https endpoints; a self-hosted LAN server over http:// needs
    // no certificate bundle (see server/README.md).
    if (std::strncmp(config.url, "https", 5) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }
    config.timeout_ms = 15000; // CPU Whisper on a small VM can take a few seconds
    config.buffer_size_tx = 2048;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        logger_.error("STT: failed to create HTTP client");
        return {};
    }

    char contentType[96];
    std::snprintf(contentType, sizeof(contentType), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(client, "Content-Type", contentType);
    char auth[192];
    std::snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_VAIWU_STT_API_KEY);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_post_field(client,
        reinterpret_cast<const char*>(body.data()), static_cast<int>(body.size()));

    const esp_err_t result = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (result != ESP_OK) {
        logger_.warn(std::string("STT: request failed: ") + esp_err_to_name(result));
        return {};
    }
    if (status != 200 || responseBuffer_.empty()) {
        logger_.warn("STT: bad HTTP status " + std::to_string(status));
        return {};
    }

    cJSON* root = cJSON_Parse(responseBuffer_.c_str());
    if (root == nullptr) {
        logger_.warn("STT: failed to parse response JSON");
        return {};
    }
    std::string text;
    const cJSON* textNode = cJSON_GetObjectItemCaseSensitive(root, "text");
    if (cJSON_IsString(textNode) && textNode->valuestring != nullptr) {
        text = textNode->valuestring;
    }
    cJSON_Delete(root);
    return text;
#endif
}

} // namespace vaiwu::chat
