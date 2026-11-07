#pragma once

/// @file spi_master.h
/// @brief Minimal host stand-in for ESP-IDF's driver/spi_master.h. It records each
/// queued transaction's requested bit length and data, so the real ESP32SPIOutput
/// can run on a host. It is not on any device build's include path.

#include "fl/stl/stddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/malloc.h"

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 0x103
#define portMAX_DELAY 0xFFFFFFFFU
#define SPICOMMON_BUSFLAG_MASTER 1U
#define SPI_DMA_CH_AUTO 3
#define SPI_DEVICE_HALFDUPLEX 1U
#define SOC_SPI_PERIPH_NUM 2
#define MALLOC_CAP_DMA 1U

typedef enum { SPI2_HOST = 1, SPI3_HOST = 2, HSPI_HOST = 1, VSPI_HOST = 2 } spi_host_device_t;

struct spi_transaction_t {
    size_t length;
    const void* tx_buffer;
    void* rx_buffer;
};
struct spi_bus_config_t {
    int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num, max_transfer_sz;
    unsigned flags;
};
struct spi_device_interface_config_t {
    uint8_t mode;
    int clock_speed_hz, spics_io_num, queue_size;
    unsigned flags;
};
typedef struct fake_spi_device* spi_device_handle_t;

// Defined by the test that includes this.
esp_err_t spi_device_queue_trans(spi_device_handle_t, spi_transaction_t*, uint32_t);
esp_err_t spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t**, uint32_t);
esp_err_t spi_device_polling_transmit(spi_device_handle_t, spi_transaction_t*);

inline esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t*, int) { return ESP_OK; }
inline esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t*,
                                    spi_device_handle_t* handle) {
    *handle = reinterpret_cast<spi_device_handle_t>(handle);
    return ESP_OK;
}
inline esp_err_t spi_bus_remove_device(spi_device_handle_t) { return ESP_OK; }
inline esp_err_t spi_bus_free(spi_host_device_t) { return ESP_OK; }
inline void* heap_caps_malloc(size_t size, unsigned) { return fl::malloc(size); }
inline void heap_caps_free(void* p) { fl::free(p); }
