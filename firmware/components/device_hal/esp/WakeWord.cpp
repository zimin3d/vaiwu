#include "vaiwu/hal/esp/WakeWord.hpp"

// The wake word is an opt-in feature. Only when CONFIG_VAIWU_ENABLE_WAKEWORD
// is set (and the esp-sr component + a model partition are present, see
// docs/VOICE.md) is the real WakeNet implementation compiled. Otherwise this
// translation unit is an inert no-op so the default firmware always builds.
#if defined(ESP_PLATFORM) && defined(CONFIG_VAIWU_ENABLE_WAKEWORD)
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_wn_iface.h"
#include "model_path.h"
#include <cstring>
#endif

namespace vaiwu::hal::esp {

WakeWord::WakeWord(ILogger& logger)
    : logger_(logger)
{
}

WakeWord::~WakeWord()
{
#if defined(ESP_PLATFORM) && defined(CONFIG_VAIWU_ENABLE_WAKEWORD)
    if (afeData_ != nullptr && afeHandle_ != nullptr) {
        const auto* iface = static_cast<const esp_afe_sr_iface_t*>(afeHandle_);
        iface->destroy(static_cast<esp_afe_sr_data_t*>(afeData_));
    }
#endif
}

bool WakeWord::init()
{
#if defined(ESP_PLATFORM) && defined(CONFIG_VAIWU_ENABLE_WAKEWORD)
    // Load whatever wake-word model was flashed to the "model" partition.
    srmodel_list_t* models = esp_srmodel_init("model");
    if (models == nullptr || models->num <= 0) {
        logger_.error("WakeWord: no models found in 'model' partition");
        return false;
    }
    const esp_afe_sr_iface_t* iface = &ESP_AFE_SR_HANDLE;
    afe_config_t* config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (config == nullptr) {
        logger_.error("WakeWord: afe_config_init failed");
        return false;
    }
    esp_afe_sr_data_t* data = iface->create_from_config(config);
    afe_config_free(config);
    if (data == nullptr) {
        logger_.error("WakeWord: AFE create failed");
        return false;
    }
    afeHandle_ = iface;
    afeData_ = data;
    feedChunkSamples_ = iface->get_feed_chunksize(data);
    feedBuffer_.reserve(static_cast<std::size_t>(feedChunkSamples_) * 2);
    enabled_ = true;
    logger_.info("WakeWord: WakeNet ready");
    return true;
#else
    // Feature compiled out: stay a no-op (push-to-talk still works).
    return false;
#endif
}

bool WakeWord::feed(const std::int16_t* samples, std::size_t count)
{
#if defined(ESP_PLATFORM) && defined(CONFIG_VAIWU_ENABLE_WAKEWORD)
    if (!enabled_ || samples == nullptr || count == 0) {
        return false;
    }
    const auto* iface = static_cast<const esp_afe_sr_iface_t*>(afeHandle_);
    auto* data = static_cast<esp_afe_sr_data_t*>(afeData_);
    feedBuffer_.insert(feedBuffer_.end(), samples, samples + count);
    while (static_cast<int>(feedBuffer_.size()) >= feedChunkSamples_) {
        iface->feed(data, feedBuffer_.data());
        feedBuffer_.erase(feedBuffer_.begin(), feedBuffer_.begin() + feedChunkSamples_);
        afe_fetch_result_t* result = iface->fetch(data);
        if (result != nullptr && result->wakeup_state == WAKENET_DETECTED) {
            detected_ = true;
        }
    }
    return detected_;
#else
    static_cast<void>(samples);
    static_cast<void>(count);
    return false;
#endif
}

bool WakeWord::detected()
{
    const bool value = detected_;
    detected_ = false;
    return value;
}

} // namespace vaiwu::hal::esp
