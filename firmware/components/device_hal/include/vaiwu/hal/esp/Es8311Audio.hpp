#pragma once

#include "vaiwu/interfaces/IAudio.hpp"
#include "vaiwu/interfaces/ILogger.hpp"

#include <cstdint>

#ifdef ESP_PLATFORM
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: drives the board's ES8311 audio codec for playback.
 * Responsibilities: bring up I2S data + (shared, externally-owned) I2C control
 *                    paths for the codec through ESP-IDF's esp_codec_dev
 *                    framework and write PCM16 mono audio buffers to it.
 *                    Capture is intentionally not implemented yet (see
 *                    docs/ROADMAP.md Milestone 3); startCapture()/stopCapture()
 *                    are safe no-ops so this class can be dropped in as a
 *                    complete IAudio today without blocking on the microphone
 *                    pipeline.
 * Public API: init(), startCapture(), stopCapture(), play(), setVolume().
 */
class Es8311Audio final : public IAudio {
public:
    // Audio buffers passed to play() are raw little-endian PCM16 mono samples
    // at sampleRateHz. i2cBusHandle is an opaque i2c_master_bus_handle_t owned
    // by SharedI2cBus (the touch controller shares the same bus on this
    // board); Es8311Audio only adds its own device on it and never creates or
    // deletes the bus itself. paActiveLow defaults to true because the
    // confirmed-working xiaozhi-esp32 reference firmware for this exact board
    // (freenove-esp32s3-display-2.8-lcd.cc) constructs its ES8311 codec with
    // pa_inverted=true: GPIO1 (the PA enable pin) is active-LOW on this
    // hardware, not active-HIGH like the ES8311 driver's own default.
    Es8311Audio(ILogger& logger,
        void* i2cBusHandle,
        int i2sMclk, int i2sBclk, int i2sWs, int i2sDout, int i2sDin,
        int paPin,
        std::uint32_t sampleRateHz = kAudioSampleRateHz,
        std::uint8_t codecI2cAddr = 0x30,
        bool paActiveLow = true);
    ~Es8311Audio() override;

    [[nodiscard]] bool init() override;
    [[nodiscard]] bool startCapture() override;
    [[nodiscard]] AudioBuffer stopCapture() override;
    [[nodiscard]] bool play(const AudioBuffer& buffer) override;
    void setVolume(std::uint8_t percent) override;

    // Blocking microphone read: fills `bytes` of raw little-endian PCM16 mono
    // at sampleRateHz() from the ES8311 ADC (the codec is opened IN_OUT).
    // Returns false on host builds or if the codec is not initialized.
    // Used by VoiceService for push-to-talk recording.
    [[nodiscard]] bool readMic(std::uint8_t* dst, std::size_t bytes);
    [[nodiscard]] std::uint32_t sampleRateHz() const { return sampleRateHz_; }

private:
    ILogger& logger_;
    void* i2cBusHandle_;
    int i2sMclk_;
    int i2sBclk_;
    int i2sWs_;
    int i2sDout_;
    int i2sDin_;
    int paPin_;
    std::uint32_t sampleRateHz_;
    std::uint8_t codecI2cAddr_;
    bool paActiveLow_;
    bool initialized_{false};

#ifdef ESP_PLATFORM
    i2s_chan_handle_t txHandle_{nullptr};
    i2s_chan_handle_t rxHandle_{nullptr};
    const audio_codec_data_if_t* dataIf_{nullptr};
    const audio_codec_ctrl_if_t* ctrlIf_{nullptr};
    const audio_codec_gpio_if_t* gpioIf_{nullptr};
    const audio_codec_if_t* codecIf_{nullptr};
    esp_codec_dev_handle_t dev_{nullptr};
#endif
};

} // namespace vaiwu::hal::esp
