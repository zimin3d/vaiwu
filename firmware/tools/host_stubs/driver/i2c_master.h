/* Host-check stub. */
#pragma once
#include "esp_err.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct i2c_master_bus_t* i2c_master_bus_handle_t;
typedef struct i2c_master_dev_t* i2c_master_dev_handle_t;
typedef struct {
    uint16_t device_address;
    uint32_t scl_speed_hz;
} i2c_device_config_t;
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
    const i2c_device_config_t* config, i2c_master_dev_handle_t* handle);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t* wbuf,
    size_t wlen, uint8_t* rbuf, size_t rlen, int timeout_ms);
#ifdef __cplusplus
}
#endif
