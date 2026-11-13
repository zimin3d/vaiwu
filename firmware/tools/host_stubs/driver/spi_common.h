/* Host-check stub (IDF 5.5 API subset). */
#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 } spi_host_device_t;
#define SPI_DMA_CH_AUTO 3
typedef struct {
    int mosi_io_num;
    int miso_io_num;
    int sclk_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    int max_transfer_sz;
    unsigned int flags;
    int intr_flags;
} spi_bus_config_t;
esp_err_t spi_bus_initialize(spi_host_device_t host_id, const spi_bus_config_t* bus_config, int dma_chan);
#ifdef __cplusplus
}
#endif
