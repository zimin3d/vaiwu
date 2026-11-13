#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace vaiwu {

/**
 * Purpose: abstracts persistent key-value storage.
 * Responsibilities: initialize the storage backend, then read and write text
 *                    settings without exposing storage backend details.
 * Public API: init(), readText(), writeText().
 */
class IStorage {
public:
    virtual ~IStorage() = default;

    [[nodiscard]] virtual bool init() = 0;
    [[nodiscard]] virtual std::optional<std::string> readText(std::string_view key) const = 0;
    [[nodiscard]] virtual bool writeText(std::string_view key, std::string_view value) = 0;
};

} // namespace vaiwu
