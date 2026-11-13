#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vaiwu {

enum class NetworkStatus : std::uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Failed,
};

struct NetworkInfo {
    NetworkStatus status{NetworkStatus::Disconnected};
    std::string ssid{};
    std::uint8_t quality{0};
};

// One access point found by a Wi-Fi scan, used by the on-screen Wi-Fi setup
// flow (see docs/UI.md / docs/SETTINGS.md) to list nearby networks. rssiDbm
// is the raw signal strength (negative dBm, closer to 0 is stronger);
// AssistantUi converts it to a 0-100 display percent.
struct WifiNetworkInfo {
    std::string ssid{};
    std::int8_t rssiDbm{0};
    bool secure{true};
};

/**
 * Purpose: abstracts network connectivity.
 * Responsibilities: connect the device, report normalized connectivity
 *                    state, and support the on-screen Wi-Fi setup flow
 *                    (scan for nearby networks, connect with explicit
 *                    credentials entered on the touchscreen keyboard).
 * Public API: connect(), disconnect(), status(), startScan(),
 *             scanResults(), connectWithCredentials().
 */
class INetwork {
public:
    virtual ~INetwork() = default;

    [[nodiscard]] virtual bool connect() = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual NetworkInfo status() const = 0;

    // Blocking (like connect() already is): scans for nearby access points
    // and returns once the scan completes, storing results for
    // scanResults(). Returns false if the scan itself could not be started
    // (e.g. no Wi-Fi hardware on host builds).
    [[nodiscard]] virtual bool startScan() = 0;
    [[nodiscard]] virtual std::vector<WifiNetworkInfo> scanResults() const = 0;

    // Reconfigures Wi-Fi with an explicit SSID/password (from the on-screen
    // setup flow) and attempts to connect, same blocking behavior as
    // connect(). On success, subsequent connect() calls (e.g. after a
    // reboot) should keep using these credentials - callers are responsible
    // for persisting them (see SettingsController).
    [[nodiscard]] virtual bool connectWithCredentials(const std::string& ssid, const std::string& password) = 0;
};

} // namespace vaiwu
