#pragma once

#include "vaiwu/interfaces/ILogger.hpp"

namespace vaiwu::services {

/**
 * Purpose: provides a concrete logger for firmware and host checks.
 * Responsibilities: route logs to ESP-IDF logging on device or stdout on host.
 * Public API: info(), warn(), error().
 */
class ConsoleLogger final : public ILogger {
public:
    void info(std::string_view message) override;
    void warn(std::string_view message) override;
    void error(std::string_view message) override;
};

} // namespace vaiwu::services
