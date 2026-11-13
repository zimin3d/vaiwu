#pragma once

#include "vaiwu/game/Characters.hpp"
#include "vaiwu/game/I18n.hpp"
#include "vaiwu/game/VaiwuPet.hpp"

#include <cstdint>

namespace vaiwu::game {

// What just happened, from the phrase picker's point of view. Mirrors the
// user actions plus a few passive situations the UI wants a line for.
enum class PhraseContext : std::uint8_t {
    Greeting = 0,
    Fed,
    Played,
    Washed,
    Toilet,
    Talked,
    Hugged,
    Patted,
    Treated,
    RefusedFood,
    Sleeping,
    Sick,
    Sad,
    Dead,
    Poked,  // tapped the sprite while awake
    Woken,  // tapped the sprite while asleep - woken up early
    Victory, // came home from an adventure with loot and XP
    Fainted, // lost a fight outside the gates and stumbled back home
};

/**
 * Purpose: offline fallback for the AI chat (spec: canned phrases when
 *          there is no Wi-Fi or the request fails/times out).
 * Responsibilities: return a short in-character English phrase for a given
 *                    character + context, cycling through variants so
 *                    repeated actions do not always show the same line.
 *                    Pure C++, host-testable.
 * Public API: offlinePhrase().
 */
[[nodiscard]] const char* offlinePhrase(CharacterId id, PhraseContext context, std::uint32_t salt, Lang lang = Lang::En);

} // namespace vaiwu::game
