#include "vaiwu/hal/esp/EspWifiNetwork.hpp"

#include <algorithm>
#include <utility>

#ifdef ESP_PLATFORM
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <cstdio>
#endif

namespace vaiwu::hal::esp {
namespace {
#ifdef ESP_PLATFORM
constexpr EventBits_t connectedBit = BIT0;
constexpr EventBits_t failedBit = BIT1;
constexpr int maxRetryCount = 5;
constexpr std::uint16_t kMaxScanResults = 16;
#endif
} // namespace

EspWifiNetwork::EspWifiNetwork(std::string ssid, std::string password)
    : ssid_(std::move(ssid))
    , password_(std::move(password))
{
}

EspWifiNetwork::~EspWifiNetwork()
{
#ifdef ESP_PLATFORM
    if (eventGroup_ != nullptr) {
        vEventGroupDelete(eventGroup_);
    }
#endif
}

bool EspWifiNetwork::connect()
{
#ifndef ESP_PLATFORM
    info_ = {NetworkStatus::Failed, ssid_, 0};
    return false;
#else
    if (ssid_.empty() || !ensureStarted() || !configureStation()) {
        info_ = {NetworkStatus::Failed, ssid_, 0};
        return false;
    }

    info_ = {NetworkStatus::Connecting, ssid_, 0};
    retryCount_ = 0;
    xEventGroupClearBits(eventGroup_, connectedBit | failedBit);
    // Explicit (re)connect rather than relying solely on the
    // WIFI_EVENT_STA_START handler's auto-connect - that event only fires
    // once, the first time the radio starts, so a later reconnect (or a
    // scan that already brought the radio up) needs this to actually kick
    // off a connection attempt. Ignoring the error is deliberate/matches
    // the handler's own style below: a redundant "already connecting" call
    // here is harmless.
    static_cast<void>(esp_wifi_disconnect());
    static_cast<void>(esp_wifi_connect());
    return waitForConnection();
#endif
}

void EspWifiNetwork::disconnect()
{
#ifdef ESP_PLATFORM
    if (stackInitialized_) {
        static_cast<void>(esp_wifi_disconnect());
    }
#endif
    info_ = {NetworkStatus::Disconnected, ssid_, 0};
}

NetworkInfo EspWifiNetwork::status() const
{
    return info_;
}

bool EspWifiNetwork::startScan()
{
#ifndef ESP_PLATFORM
    return false;
#else
    if (!ensureStarted()) {
        return false;
    }

    wifi_scan_config_t scanConfig{};
    scanConfig.show_hidden = false;
    // Blocking (block=true): returns once the scan across all channels
    // completes, matching connect()'s own blocking style - see
    // INetwork::startScan()'s doc comment.
    if (esp_wifi_scan_start(&scanConfig, true) != ESP_OK) {
        return false;
    }

    std::uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    scanResults_.clear();
    if (apCount == 0) {
        return true;
    }

    apCount = std::min(apCount, kMaxScanResults);
    std::vector<wifi_ap_record_t> records(apCount);
    std::uint16_t actualCount = apCount;
    if (esp_wifi_scan_get_ap_records(&actualCount, records.data()) != ESP_OK) {
        return false;
    }
    records.resize(actualCount);

    for (const auto& record : records) {
        const std::string ssid(reinterpret_cast<const char*>(record.ssid));
        if (ssid.empty()) {
            continue; // hidden network with no broadcast SSID - nothing to select
        }

        // Merge duplicate SSIDs (e.g. a mesh router advertising the same
        // name on 2.4GHz and 5GHz) - keep only the strongest signal, so the
        // on-screen list doesn't show the same name multiple times.
        const auto existing = std::find_if(scanResults_.begin(), scanResults_.end(),
            [&ssid](const WifiNetworkInfo& info) { return info.ssid == ssid; });
        const bool secure = record.authmode != WIFI_AUTH_OPEN;
        if (existing != scanResults_.end()) {
            if (record.rssi > existing->rssiDbm) {
                existing->rssiDbm = record.rssi;
                existing->secure = secure;
            }
            continue;
        }
        scanResults_.push_back({ssid, static_cast<std::int8_t>(record.rssi), secure});
    }

    std::sort(scanResults_.begin(), scanResults_.end(),
        [](const WifiNetworkInfo& a, const WifiNetworkInfo& b) { return a.rssiDbm > b.rssiDbm; });

    return true;
#endif
}

std::vector<WifiNetworkInfo> EspWifiNetwork::scanResults() const
{
    return scanResults_;
}

bool EspWifiNetwork::connectWithCredentials(const std::string& ssid, const std::string& password)
{
#ifndef ESP_PLATFORM
    static_cast<void>(password);
    info_ = {NetworkStatus::Failed, ssid, 0};
    return false;
#else
    ssid_ = ssid;
    password_ = password;

    if (ssid_.empty() || !ensureStarted() || !configureStation()) {
        info_ = {NetworkStatus::Failed, ssid_, 0};
        return false;
    }

    info_ = {NetworkStatus::Connecting, ssid_, 0};
    retryCount_ = 0;
    xEventGroupClearBits(eventGroup_, connectedBit | failedBit);
    static_cast<void>(esp_wifi_disconnect());
    static_cast<void>(esp_wifi_connect());
    return waitForConnection();
#endif
}

#ifdef ESP_PLATFORM
bool EspWifiNetwork::initNvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret == ESP_OK;
}

bool EspWifiNetwork::initNetworkStack()
{
    if (stackInitialized_) {
        return true;
    }

    eventGroup_ = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    const esp_err_t loopResult = esp_event_loop_create_default();
    if (loopResult != ESP_OK && loopResult != ESP_ERR_INVALID_STATE) {
        return false;
    }

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EspWifiNetwork::handleEvent, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &EspWifiNetwork::handleEvent, this));
    handlersRegistered_ = true;
    stackInitialized_ = true;
    return eventGroup_ != nullptr;
}

bool EspWifiNetwork::ensureStarted()
{
    if (wifiStarted_) {
        return true;
    }

    if (!initNvs() || !initNetworkStack()) {
        return false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    const esp_err_t startResult = esp_wifi_start();
    if (startResult != ESP_OK && startResult != ESP_ERR_WIFI_STATE) {
        return false;
    }

    wifiStarted_ = true;
    return true;
}

bool EspWifiNetwork::configureStation()
{
    wifi_config_t wifiConfig{};
    std::snprintf(reinterpret_cast<char*>(wifiConfig.sta.ssid), sizeof(wifiConfig.sta.ssid), "%s", ssid_.c_str());
    std::snprintf(reinterpret_cast<char*>(wifiConfig.sta.password), sizeof(wifiConfig.sta.password), "%s", password_.c_str());
    wifiConfig.sta.threshold.authmode = password_.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
    return true;
}

bool EspWifiNetwork::waitForConnection()
{
    const EventBits_t bits = xEventGroupWaitBits(
        eventGroup_,
        connectedBit | failedBit,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000));

    if ((bits & connectedBit) != 0) {
        info_ = {NetworkStatus::Connected, ssid_, 100};
        return true;
    }

    info_ = {NetworkStatus::Failed, ssid_, 0};
    return false;
}

void EspWifiNetwork::handleEvent(void* arg, esp_event_base_t eventBase, std::int32_t eventId, void* eventData)
{
    static_cast<void>(eventData);
    auto* self = static_cast<EspWifiNetwork*>(arg);

    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        static_cast<void>(esp_wifi_connect());
    } else if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        if (self->retryCount_ < maxRetryCount) {
            self->retryCount_++;
            static_cast<void>(esp_wifi_connect());
            return;
        }

        xEventGroupSetBits(self->eventGroup_, failedBit);
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        self->retryCount_ = 0;
        xEventGroupSetBits(self->eventGroup_, connectedBit);
    }
}
#endif

} // namespace vaiwu::hal::esp
