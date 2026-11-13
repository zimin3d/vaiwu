// Host test for the WAV container builder (SpeechToText::makeWav).
#include "vaiwu/chat/SpeechToText.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

std::uint32_t rd32(const std::vector<std::uint8_t>& v, std::size_t o)
{
    return v[o] | (v[o + 1] << 8) | (v[o + 2] << 16) | (static_cast<std::uint32_t>(v[o + 3]) << 24);
}
std::uint16_t rd16(const std::vector<std::uint8_t>& v, std::size_t o)
{
    return static_cast<std::uint16_t>(v[o] | (v[o + 1] << 8));
}
}

int main()
{
    std::vector<std::uint8_t> pcm(2000, 0);
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = static_cast<std::uint8_t>(i & 0xFF);
    }
    const auto wav = vaiwu::chat::makeWav(pcm, 24000);

    CHECK(wav.size() == 44 + pcm.size());
    CHECK(std::memcmp(wav.data(), "RIFF", 4) == 0);
    CHECK(std::memcmp(wav.data() + 8, "WAVE", 4) == 0);
    CHECK(std::memcmp(wav.data() + 12, "fmt ", 4) == 0);
    CHECK(std::memcmp(wav.data() + 36, "data", 4) == 0);
    CHECK(rd32(wav, 4) == 36 + pcm.size());   // RIFF chunk size
    CHECK(rd32(wav, 16) == 16);               // PCM fmt size
    CHECK(rd16(wav, 20) == 1);                // PCM format
    CHECK(rd16(wav, 22) == 1);                // mono
    CHECK(rd32(wav, 24) == 24000);            // sample rate
    CHECK(rd32(wav, 28) == 24000 * 2);        // byte rate
    CHECK(rd16(wav, 32) == 2);                // block align
    CHECK(rd16(wav, 34) == 16);               // bits per sample
    CHECK(rd32(wav, 40) == pcm.size());       // data size
    // Payload preserved after the header.
    CHECK(std::memcmp(wav.data() + 44, pcm.data(), pcm.size()) == 0);

    if (failures == 0) { std::printf("ALL WAV TESTS PASSED\n"); return 0; }
    std::printf("%d FAILURES\n", failures);
    return 1;
}
