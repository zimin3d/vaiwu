#pragma once

#include <cstdlib>

namespace vaiwu {

// Semantic version compare for OTA: "v5.1" > "4.9" etc. Missing parts count
// as 0, a leading 'v' or 'V' is ignored, anything past a non-digit/non-dot
// character (like "-rc1") is cut off. Pure and host-testable.
inline bool isNewerVersion(const char* current, const char* latest)
{
    if (current == nullptr || latest == nullptr) {
        return false;
    }
    if (*current == 'v' || *current == 'V') {
        ++current;
    }
    if (*latest == 'v' || *latest == 'V') {
        ++latest;
    }
    for (int part = 0; part < 4; ++part) {
        char* endCur = nullptr;
        char* endNew = nullptr;
        const long a = std::strtol(current, &endCur, 10);
        const long b = std::strtol(latest, &endNew, 10);
        if (b > a) {
            return true;
        }
        if (b < a) {
            return false;
        }
        const bool moreCur = (endCur != nullptr && *endCur == '.');
        const bool moreNew = (endNew != nullptr && *endNew == '.');
        if (!moreCur && !moreNew) {
            return false; // fully equal
        }
        current = moreCur ? endCur + 1 : "0";
        latest = moreNew ? endNew + 1 : "0";
    }
    return false;
}

} // namespace vaiwu
