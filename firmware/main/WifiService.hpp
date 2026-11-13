#pragma once

#include "vaiwu/interfaces/ILogger.hpp"
#include "vaiwu/interfaces/INetwork.hpp"
#include "vaiwu/interfaces/IStorage.hpp"

#include <cstring>
#include <functional>
#include <mutex>
#include <vector>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

namespace vaiwu {

/**
 * Purpose: runs every blocking Wi-Fi operation (initial connect, scan,
 *          connect-with-credentials) on its own FreeRTOS task so the LVGL
 *          task never freezes; the UI polls state() and takes scan results
 *          from a mutex-guarded snapshot.
 * Responsibilities: serialize Wi-Fi ops through a small queue, persist
 *                    credentials to storage after a successful on-screen
 *                    connect ("wifi_ssid"/"wifi_pass" keys, read back at
 *                    boot by main.cpp), and expose a coarse state machine
 *                    for the settings screens.
 * Public API: init(), requestAutoConnect(), requestScan(),
 *             requestConnect(), state(), scanResults().
 */
class WifiService {
public:
    enum class State : int {
        Idle = 0,
        Scanning,
        ScanDone,
        Connecting,
        Connected,
        Failed,
    };

    // onConnected runs on the Wi-Fi task after any successful connect
    // (used to kick off the one-time SNTP clock init without blocking boot).
    WifiService(ILogger& logger, INetwork& network, IStorage& storage,
        std::function<void()> onConnected = {})
        : logger_(logger)
        , network_(network)
        , storage_(storage)
        , onConnected_(std::move(onConnected))
    {
    }

    [[nodiscard]] bool init()
    {
#ifdef ESP_PLATFORM
        queue_ = xQueueCreate(4, sizeof(Cmd));
        if (queue_ == nullptr) {
            logger_.error("WifiService: queue creation failed");
            return false;
        }
        if (xTaskCreate(&WifiService::taskEntry, "vaiwu_wifi", 6144, this, 3, nullptr) != pdPASS) {
            logger_.error("WifiService: task creation failed");
            return false;
        }
#endif
        return true;
    }

    // Boot-time connect with already-configured credentials (NVS/Kconfig).
    void requestAutoConnect() { post(Op::AutoConnect, "", ""); }
    void requestScan() { post(Op::Scan, "", ""); }
    void requestConnect(const char* ssid, const char* password)
    {
        post(Op::Connect, ssid, password);
    }

    [[nodiscard]] State state() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    [[nodiscard]] std::vector<WifiNetworkInfo> scanResults() const
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        return scanResults_;
    }

private:
    enum class Op : int { AutoConnect = 0, Scan, Connect };

    struct Cmd {
        Op op;
        char ssid[33];
        char password[65];
    };

    void post(Op op, const char* ssid, const char* password)
    {
#ifdef ESP_PLATFORM
        if (queue_ == nullptr) {
            return;
        }
        Cmd cmd{};
        cmd.op = op;
        std::strncpy(cmd.ssid, ssid, sizeof(cmd.ssid) - 1);
        std::strncpy(cmd.password, password, sizeof(cmd.password) - 1);
        setState(op == Op::Scan ? State::Scanning : State::Connecting);
        static_cast<void>(xQueueSend(queue_, &cmd, 0));
#else
        static_cast<void>(op);
        static_cast<void>(ssid);
        static_cast<void>(password);
#endif
    }

    void setState(State state)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        state_ = state;
    }

#ifdef ESP_PLATFORM
    static void taskEntry(void* arg) { static_cast<WifiService*>(arg)->taskLoop(); }

    void taskLoop()
    {
        for (;;) {
            Cmd cmd{};
            if (xQueueReceive(queue_, &cmd, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            switch (cmd.op) {
            case Op::AutoConnect: {
                const bool ok = network_.connect();
                setState(ok ? State::Connected : State::Failed);
                if (ok && onConnected_) {
                    onConnected_();
                }
                break;
            }
            case Op::Scan: {
                const bool ok = network_.startScan();
                {
                    const std::lock_guard<std::mutex> lock(mutex_);
                    scanResults_ = ok ? network_.scanResults() : std::vector<WifiNetworkInfo>{};
                    state_ = State::ScanDone;
                }
                break;
            }
            case Op::Connect: {
                const bool ok = network_.connectWithCredentials(cmd.ssid, cmd.password);
                if (ok) {
                    // Persist so the next boot reconnects automatically.
                    static_cast<void>(storage_.writeText("wifi_ssid", cmd.ssid));
                    static_cast<void>(storage_.writeText("wifi_pass", cmd.password));
                }
                setState(ok ? State::Connected : State::Failed);
                if (ok && onConnected_) {
                    onConnected_();
                }
                break;
            }
            }
        }
    }

    QueueHandle_t queue_{nullptr};
#endif

    ILogger& logger_;
    INetwork& network_;
    IStorage& storage_;
    std::function<void()> onConnected_;
    mutable std::mutex mutex_;
    State state_{State::Idle};
    std::vector<WifiNetworkInfo> scanResults_{};
};

} // namespace vaiwu
