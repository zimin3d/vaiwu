#pragma once

#include <string_view>

namespace vaiwu {

/**
 * Purpose: abstracts logging.
 * Responsibilities: write diagnostic messages without tying application code to ESP-IDF logging.
 * Public API: info(), warn(), error().
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void info(std::string_view message) = 0;
    virtual void warn(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
};

} // namespace vaiwu
