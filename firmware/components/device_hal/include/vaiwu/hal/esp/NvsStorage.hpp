#pragma once

#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/IStorage.hpp"

#include <string>

#ifdef ESP_PLATFORM
#include <nvs.h>
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: persists settings across reboots using ESP-IDF's NVS flash partition.
 * Responsibilities: initialize the NVS flash partition (idempotent, erasing and
 *                    retrying on ESP_ERR_NVS_NO_FREE_PAGES/NEW_VERSION_FOUND,
 *                    the same pattern EspWifiNetwork::initNvs() already uses)
 *                    and open a dedicated namespace, then read/write text
 *                    values as NVS string entries.
 * Public API: init(), readText(), writeText().
 */
class NvsStorage final : public IStorage {
public:
    explicit NvsStorage(ILogger& logger, std::string nvsNamespace = "vaiwu_cfg");
    ~NvsStorage() override;

    [[nodiscard]] bool init() override;
    [[nodiscard]] std::optional<std::string> readText(std::string_view key) const override;
    [[nodiscard]] bool writeText(std::string_view key, std::string_view value) override;

private:
    ILogger& logger_;
    std::string namespace_;
    bool ready_{false};

#ifdef ESP_PLATFORM
    [[nodiscard]] bool initNvsFlash();

    nvs_handle_t handle_{0};
#endif
};

} // namespace vaiwu::hal::esp
