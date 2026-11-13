#pragma once

#include "vaiwu/interfaces/INetwork.hpp"

#include <string>
#include <vector>

#ifdef ESP_PLATFORM
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: provides an ESP-IDF Wi-Fi station implementation.
 * Responsibilities: initialize ESP networking, connect to configured Wi-Fi,
 *                    expose normalized status, and support the on-screen
 *                    Wi-Fi setup flow (scan for nearby access points, connect
 *                    with an explicit SSID/password typed on the touchscreen
 *                    keyboard).
 * Public API: connect(), disconnect(), status(), startScan(), scanResults(),
 *             connectWithCredentials().
 */
class EspWifiNetwork final : public INetwork {
public:
    EspWifiNetwork(std::string ssid, std::string password);
    ~EspWifiNetwork() override;

    [[nodiscard]] bool connect() override;
    void disconnect() override;
    [[nodiscard]] NetworkInfo status() const override;

    [[nodiscard]] bool startScan() override;
    [[nodiscard]] std::vector<WifiNetworkInfo> scanResults() const override;
    [[nodiscard]] bool connectWithCredentials(const std::string& ssid, const std::string& password) override;

private:
#ifdef ESP_PLATFORM
    [[nodiscard]] bool initNvs();
    [[nodiscard]] bool initNetworkStack();
    // Brings the Wi-Fi driver up in STA mode (NVS+netif+esp_wifi_start()) if
    // it isn't already - idempotent, safe to call from connect(),
    // connectWithCredentials(), and startScan() alike, since any of them may
    // be the first to need the radio up.
    [[nodiscard]] bool ensureStarted();
    [[nodiscard]] bool configureStation();
    [[nodiscard]] bool waitForConnection();
    static void handleEvent(void* arg, esp_event_base_t eventBase, std::int32_t eventId, void* eventData);

    EventGroupHandle_t eventGroup_{nullptr};
    int retryCount_{0};
    bool stackInitialized_{false};
    bool handlersRegistered_{false};
    bool wifiStarted_{false};
#endif

    std::string ssid_;
    std::string password_;
    NetworkInfo info_{};
    std::vector<WifiNetworkInfo> scanResults_{};
};

} // namespace vaiwu::hal::esp
