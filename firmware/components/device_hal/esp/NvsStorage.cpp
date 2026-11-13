#include "vaiwu/hal/esp/NvsStorage.hpp"

#ifdef ESP_PLATFORM
#include <nvs_flash.h>
#endif

namespace vaiwu::hal::esp {

namespace {
// NVS caps both namespace and key names at 15 characters
// (NVS_KEY_NAME_MAX_SIZE is 16 including the null terminator).
constexpr std::size_t kNvsMaxNameLength = 15;
} // namespace

NvsStorage::NvsStorage(ILogger& logger, std::string nvsNamespace)
    : logger_(logger)
    , namespace_(std::move(nvsNamespace))
{
}

NvsStorage::~NvsStorage()
{
#ifdef ESP_PLATFORM
    if (ready_) {
        nvs_close(handle_);
    }
#endif
}

#ifdef ESP_PLATFORM
bool NvsStorage::initNvsFlash()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // A previous NVS layout is incompatible (e.g. partition table or
        // NVS version changed); erase and retry once, matching
        // EspWifiNetwork::initNvs()'s established pattern so both callers
        // stay consistent about how they recover from this.
        ret = nvs_flash_erase();
        if (ret == ESP_OK) {
            ret = nvs_flash_init();
        }
    }

    return ret == ESP_OK;
}
#endif

bool NvsStorage::init()
{
    if (namespace_.size() > kNvsMaxNameLength) {
        logger_.error("NVS namespace name too long (max 15 chars): " + namespace_);
        return false;
    }

#ifdef ESP_PLATFORM
    if (!initNvsFlash()) {
        logger_.error("NVS flash init failed");
        return false;
    }

    const esp_err_t openResult = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle_);
    if (openResult != ESP_OK) {
        logger_.error("NVS namespace open failed");
        return false;
    }

    ready_ = true;
    return true;
#else
    // Host builds have no NVS partition; report success is misleading, so
    // BoardFactory should not select this backend off-device. Kept as a
    // safe no-op fallback in case NvsStorage is ever constructed on host.
    return false;
#endif
}

std::optional<std::string> NvsStorage::readText(std::string_view key) const
{
    if (key.size() > kNvsMaxNameLength) {
        return std::nullopt;
    }

#ifdef ESP_PLATFORM
    if (!ready_) {
        return std::nullopt;
    }

    const std::string keyStr{key};
    std::size_t requiredSize = 0;
    esp_err_t result = nvs_get_str(handle_, keyStr.c_str(), nullptr, &requiredSize);
    if (result != ESP_OK) {
        return std::nullopt;
    }

    std::string value(requiredSize, '\0');
    result = nvs_get_str(handle_, keyStr.c_str(), value.data(), &requiredSize);
    if (result != ESP_OK) {
        return std::nullopt;
    }

    // requiredSize includes the null terminator; trim it off.
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }

    return value;
#else
    static_cast<void>(key);
    return std::nullopt;
#endif
}

bool NvsStorage::writeText(std::string_view key, std::string_view value)
{
    if (key.size() > kNvsMaxNameLength) {
        logger_.error("NVS key too long (max 15 chars)");
        return false;
    }

#ifdef ESP_PLATFORM
    if (!ready_) {
        return false;
    }

    const std::string keyStr{key};
    const std::string valueStr{value};
    if (nvs_set_str(handle_, keyStr.c_str(), valueStr.c_str()) != ESP_OK) {
        return false;
    }

    return nvs_commit(handle_) == ESP_OK;
#else
    static_cast<void>(key);
    static_cast<void>(value);
    return false;
#endif
}

} // namespace vaiwu::hal::esp
