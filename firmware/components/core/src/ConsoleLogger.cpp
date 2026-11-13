#include "vaiwu/services/ConsoleLogger.hpp"

#ifdef ESP_PLATFORM
#include "esp_log.h"
#else
#include <iostream>
#endif

namespace vaiwu::services {
namespace {
constexpr const char* tag = "vaiwu";
}

void ConsoleLogger::info(std::string_view message)
{
#ifdef ESP_PLATFORM
    ESP_LOGI(tag, "%.*s", static_cast<int>(message.size()), message.data());
#else
    std::cout << "[info] " << message << '\n';
#endif
}

void ConsoleLogger::warn(std::string_view message)
{
#ifdef ESP_PLATFORM
    ESP_LOGW(tag, "%.*s", static_cast<int>(message.size()), message.data());
#else
    std::cout << "[warn] " << message << '\n';
#endif
}

void ConsoleLogger::error(std::string_view message)
{
#ifdef ESP_PLATFORM
    ESP_LOGE(tag, "%.*s", static_cast<int>(message.size()), message.data());
#else
    std::cerr << "[error] " << message << '\n';
#endif
}

} // namespace vaiwu::services
