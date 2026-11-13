#pragma once

#include "vaiwu/interfaces/ILogger.hpp"

#include <cstdint>
#include <vector>

namespace vaiwu::hal::esp {

/**
 * Purpose: optional on-device wake word (hybrid voice design) - when enabled,
 *          detects a spoken trigger phrase locally so the assistant can start
 *          recording hands-free; the recorded command still goes to the cloud
 *          Whisper STT.
 * Responsibilities: wrap Espressif's esp-sr WakeNet behind a tiny, host- and
 *                    default-build-safe interface. The real implementation is
 *                    compiled only when CONFIG_VAIWU_ENABLE_WAKEWORD is set
 *                    (see docs/VOICE.md: needs the esp-sr component, PSRAM and
 *                    a model partition). With the option off - the default -
 *                    init() returns false and the class is an inert no-op, so
 *                    the firmware builds and runs on push-to-talk alone.
 * Public API: init(), enabled(), feed(), detected().
 *
 * Usage: after init() succeeds, feed() raw PCM16 mono at expectedSampleRate()
 * from the same microphone loop; detected() returns true once for each
 * trigger, after which the caller starts a VoiceService recording.
 */
class WakeWord {
public:
    explicit WakeWord(ILogger& logger);
    ~WakeWord();

    WakeWord(const WakeWord&) = delete;
    WakeWord& operator=(const WakeWord&) = delete;

    // Returns false when the wake word is compiled out or model setup failed.
    [[nodiscard]] bool init();
    [[nodiscard]] bool enabled() const { return enabled_; }

    // Sample rate the detector expects (esp-sr WakeNet is fixed at 16 kHz).
    [[nodiscard]] static std::uint32_t expectedSampleRate() { return 16000; }

    // Feed one audio chunk; returns true if the wake word fired in it.
    [[nodiscard]] bool feed(const std::int16_t* samples, std::size_t count);

    // True once per detection (also cleared by feed()); convenience for
    // callers that poll separately from feeding.
    [[nodiscard]] bool detected();

private:
    ILogger& logger_;
    bool enabled_{false};
    bool detected_{false};
#if defined(ESP_PLATFORM) && defined(CONFIG_VAIWU_ENABLE_WAKEWORD)
    void* afeData_{nullptr};   // esp_afe_sr_data_t*
    const void* afeHandle_{nullptr}; // esp_afe_sr_iface_t*
    std::vector<std::int16_t> feedBuffer_{};
    int feedChunkSamples_{0};
#endif
};

} // namespace vaiwu::hal::esp
