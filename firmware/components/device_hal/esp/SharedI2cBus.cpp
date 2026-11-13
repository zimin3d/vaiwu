#include "vaiwu/hal/esp/SharedI2cBus.hpp"

namespace vaiwu::hal::esp {

SharedI2cBus::SharedI2cBus(ILogger& logger, int sda, int scl)
    : logger_(logger)
    , sda_(sda)
    , scl_(scl)
{
}

SharedI2cBus::~SharedI2cBus()
{
#ifdef ESP_PLATFORM
    if (bus_ != nullptr) {
        i2c_del_master_bus(bus_);
    }
#endif
}

bool SharedI2cBus::init()
{
#ifndef ESP_PLATFORM
    logger_.error("SharedI2cBus requires ESP-IDF; not available on host builds");
    return false;
#else
    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = I2C_NUM_0;
    busConfig.sda_io_num = static_cast<gpio_num_t>(sda_);
    busConfig.scl_io_num = static_cast<gpio_num_t>(scl_);
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = true;

    if (i2c_new_master_bus(&busConfig, &bus_) != ESP_OK) {
        logger_.error("Failed to create shared touch/audio I2C bus");
        return false;
    }

    return true;
#endif
}

void* SharedI2cBus::handle() const
{
#ifdef ESP_PLATFORM
    return bus_;
#else
    return nullptr;
#endif
}

} // namespace vaiwu::hal::esp
