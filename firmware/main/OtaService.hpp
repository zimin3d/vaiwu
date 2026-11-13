#pragma once

#include "vaiwu/VersionCompare.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <atomic>
#include <cstring>
#include <string>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"

#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

namespace vaiwu {

/**
 * Purpose: over-the-air firmware updates from GitHub Releases.
 * Responsibilities: a FreeRTOS worker handles two commands - CHECK (fetch the
 *                   latest-release manifest, compare the tag with the running
 *                   app version) and UPDATE (stream the .wfu into the idle OTA
 *                   slot, verify and mark it as the boot partition). The UI
 *                   polls state()/progress() from its slow tick; nothing here
 *                   ever blocks the UI.
 * Public API: init(), requestCheck(), requestUpdate(), state(), progress(),
 *             latestTag(), rebootNow().
 *
 * States: 0 Idle, 1 Checking, 2 UpToDate, 3 Available, 4 Downloading,
 *         5 ReadyToReboot, 6 Failed, 7 Disabled (no manifest URL configured).
 */
class OtaService {
public:
    explicit OtaService(ILogger& logger)
        : logger_(logger)
    {
    }

    [[nodiscard]] bool init()
    {
#ifdef ESP_PLATFORM
        queue_ = xQueueCreate(2, sizeof(int));
        if (queue_ == nullptr) {
            logger_.error("OtaService: queue creation failed");
            return false;
        }
        // 16 KB stack: the TLS handshake + esp_https_ota run on this task.
        if (xTaskCreate(&OtaService::taskEntry, "vaiwu_ota", 16384, this, 1, nullptr) != pdPASS) {
            logger_.error("OtaService: task creation failed");
            return false;
        }
#endif
        return true;
    }

    void requestCheck()
    {
#ifdef ESP_PLATFORM
        if (std::strlen(CONFIG_VAIWU_OTA_MANIFEST_URL) == 0) {
            state_.store(7);
            return;
        }
        send(1);
#endif
    }

    void requestUpdate()
    {
#ifdef ESP_PLATFORM
        if (state_.load() == 3) {
            send(2);
        }
#endif
    }

    int state() const { return state_.load(); }
    int progress() const { return progress_.load(); }
    const char* latestTag() const { return latestTag_; }

    void rebootNow()
    {
#ifdef ESP_PLATFORM
        if (state_.load() == 5) {
            esp_restart();
        }
#endif
    }

private:
#ifdef ESP_PLATFORM
    void send(int command)
    {
        if (queue_ != nullptr) {
            static_cast<void>(xQueueSend(queue_, &command, 0));
        }
    }

    static void taskEntry(void* arg)
    {
        static_cast<OtaService*>(arg)->taskLoop();
    }

    void taskLoop()
    {
        int command = 0;
        for (;;) {
            if (xQueueReceive(queue_, &command, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            if (command == 1) {
                checkLatest();
            } else if (command == 2) {
                downloadAndFlash();
            }
        }
    }

    // GitHub releases API answers with a JSON document; all we need is the
    // "tag_name" field, so a streamed strstr is enough (no cJSON, no big
    // parse buffer).
    void checkLatest()
    {
        state_.store(1);
        esp_http_client_config_t config = {};
        config.url = CONFIG_VAIWU_OTA_MANIFEST_URL;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = 8000;
        config.buffer_size = 2048;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            state_.store(6);
            return;
        }
        std::string body;
        char chunk[2048];
        if (esp_http_client_open(client, 0) == ESP_OK) {
            esp_http_client_fetch_headers(client);
            int read = 0;
            while ((read = esp_http_client_read(client, chunk, sizeof(chunk))) > 0
                   && body.size() < 256 * 1024) {
                body.append(chunk, static_cast<size_t>(read));
            }
        } else {
            state_.store(6);
        }
        esp_http_client_cleanup(client);
        if (state_.load() == 6) {
            return;
        }

        const char* key = "\"tag_name\":\"";
        const size_t at = body.find(key);
        if (at == std::string::npos) {
            state_.store(6);
            return;
        }
        const size_t begin = at + std::strlen(key);
        const size_t end = body.find('"', begin);
        if (end == std::string::npos || end - begin >= sizeof(latestTag_)) {
            state_.store(6);
            return;
        }
        std::memcpy(latestTag_, body.data() + begin, end - begin);
        latestTag_[end - begin] = '\0';

        const char* running = esp_app_get_description()->version;
        char log[96];
        std::snprintf(log, sizeof(log), "OTA: running %s, latest %s", running, latestTag_);
        logger_.info(log);
        state_.store(isNewerVersion(running, latestTag_) ? 3 : 2);
    }

    void downloadAndFlash()
    {
        char url[192];
        std::snprintf(url, sizeof(url), CONFIG_VAIWU_OTA_DOWNLOAD_FMT,
            latestTag_, latestTag_);
        { char log[224]; std::snprintf(log, sizeof(log), "OTA: flashing %s", url); logger_.info(log); }
        state_.store(4);
        progress_.store(0);

        esp_http_client_config_t httpConfig = {};
        httpConfig.url = url;
        httpConfig.crt_bundle_attach = esp_crt_bundle_attach;
        httpConfig.timeout_ms = 15000;
        httpConfig.buffer_size = 4096;
        httpConfig.buffer_size_tx = 1024;
        esp_https_ota_config_t otaConfig = {};
        otaConfig.http_config = &httpConfig;

        esp_https_ota_handle_t handle = nullptr;
        if (esp_https_ota_begin(&otaConfig, &handle) != ESP_OK) {
            state_.store(6);
            return;
        }
        const int total = esp_https_ota_get_image_size(handle);
        esp_err_t result = ESP_ERR_HTTPS_OTA_IN_PROGRESS;
        while (result == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            result = esp_https_ota_perform(handle);
            if (total > 0) {
                progress_.store(esp_https_ota_get_image_len_read(handle) * 100 / total);
            }
        }
        if (result != ESP_OK || esp_https_ota_finish(handle) != ESP_OK) {
            logger_.error("OTA: flash failed");
            esp_https_ota_abort(handle);
            state_.store(6);
            return;
        }
        progress_.store(100);
        state_.store(5);
        { char log[96]; std::snprintf(log, sizeof(log), "OTA: %s flashed, waiting for reboot", latestTag_); logger_.info(log); }
    }

    QueueHandle_t queue_{nullptr};
#endif

    ILogger& logger_;
    std::atomic<int> state_{0};
    std::atomic<int> progress_{0};
    char latestTag_[32]{};
};

} // namespace vaiwu
