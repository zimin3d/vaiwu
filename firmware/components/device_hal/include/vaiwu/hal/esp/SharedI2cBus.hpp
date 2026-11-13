#pragma once

#include "vaiwu/interfaces/ILogger.hpp"

#ifdef ESP_PLATFORM
#include <driver/i2c_master.h>
#endif

namespace vaiwu::hal::esp {

/**
 * Purpose: owns the single I2C bus shared by the touch controller and the
 *          ES8311 audio codec on this board (both sit on sda=16/scl=15).
 * Responsibilities: create the bus once and hand out an opaque handle so
 *                    exactly one component (BoardFactory) is responsible for
 *                    its lifetime, instead of two drivers each creating their
 *                    own master bus on the same pins (which ESP-IDF rejects).
 * Public API: init(), handle().
 */
class SharedI2cBus final {
public:
    SharedI2cBus(ILogger& logger, int sda, int scl);
    ~SharedI2cBus();

    [[nodiscard]] bool init();
    // Opaque i2c_master_bus_handle_t (nullptr on host builds or before init()
    // succeeds). Kept as void* so this header - and anything that includes it
    // transitively - stays host-compilable without ESP-IDF headers.
    [[nodiscard]] void* handle() const;

private:
    ILogger& logger_;
    int sda_;
    int scl_;

#ifdef ESP_PLATFORM
    i2c_master_bus_handle_t bus_{nullptr};
#endif
};

} // namespace vaiwu::hal::esp
