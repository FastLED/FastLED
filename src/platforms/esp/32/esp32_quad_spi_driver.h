#pragma once

/// @file esp32_quad_spi_driver.h
/// @brief ESP32 hardware driver for Quad-SPI DMA transmission
///
/// This driver wraps ESP-IDF SPI Master APIs to provide:
/// - Quad-SPI mode configuration (4 data lines)
/// - DMA buffer allocation and management
/// - Asynchronous transaction queueing
/// - RAII resource management
///
/// Compatible with ESP32, ESP32-S2, ESP32-S3, ESP32-C3 variants.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "fl/namespace.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring>

namespace fl {

/// ESP32 hardware driver for Quad-SPI DMA transmission
/// Wraps ESP-IDF SPI driver with RAII and type safety
/// @note Compatible with ESP32, ESP32-S2, ESP32-S3, ESP32-C3 variants
class ESP32QuadSPIDriver {
public:
    /// Configuration for a single SPI bus
    struct Config {
        spi_host_device_t host;        ///< HSPI_HOST (SPI2) or VSPI_HOST (SPI3)
        uint32_t clock_speed_hz;       ///< Clock frequency (recommended: 20-40 MHz)
        uint8_t clock_pin;             ///< SCK GPIO pin
        uint8_t data0_pin;             ///< D0/MOSI GPIO pin
        uint8_t data1_pin;             ///< D1/MISO GPIO pin
        uint8_t data2_pin;             ///< D2/WP GPIO pin
        uint8_t data3_pin;             ///< D3/HD GPIO pin
        size_t max_transfer_sz;        ///< Max bytes per transfer (default 65536)

        Config()
            : host(SPI2_HOST)
            , clock_speed_hz(20000000)
            , clock_pin(18)
            , data0_pin(23)
            , data1_pin(19)
            , data2_pin(22)
            , data3_pin(21)
            , max_transfer_sz(65536) {}
    };

    ESP32QuadSPIDriver()
        : mSPIHandle(nullptr)
        , mHost(SPI2_HOST)
        , mTransactionActive(false)
        , mInitialized(false) {
        memset(&mTransaction, 0, sizeof(mTransaction));
    }

    ~ESP32QuadSPIDriver() {
        cleanup();
    }

    /// Initialize SPI peripheral in quad mode
    /// @param config Hardware configuration
    /// @returns true on success, false on error
    bool begin(const Config& config) {
        if (mInitialized) {
            return true;  // Already initialized
        }

        mHost = config.host;

        // Configure SPI bus for quad mode
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = config.data0_pin;
        bus_config.miso_io_num = config.data1_pin;
        bus_config.sclk_io_num = config.clock_pin;
        bus_config.quadwp_io_num = config.data2_pin;
        bus_config.quadhd_io_num = config.data3_pin;
        bus_config.max_transfer_sz = config.max_transfer_sz;
        bus_config.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_QUAD;

        // Initialize bus with auto DMA channel selection
        esp_err_t ret = spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            return false;
        }

        // Configure SPI device
        spi_device_interface_config_t dev_config = {};
        dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
        dev_config.clock_speed_hz = config.clock_speed_hz;
        dev_config.spics_io_num = -1;  // No CS pin for LED strips
        dev_config.queue_size = 7;  // Allow up to 7 queued transactions
        dev_config.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only mode

        // Add device to bus
        ret = spi_bus_add_device(mHost, &dev_config, &mSPIHandle);
        if (ret != ESP_OK) {
            spi_bus_free(mHost);
            return false;
        }

        mInitialized = true;
        mTransactionActive = false;

        return true;
    }

    /// Allocate DMA-capable buffer (word-aligned)
    /// @param size_bytes Buffer size in bytes
    /// @returns Pointer to DMA buffer, or nullptr on failure
    uint8_t* allocateDMABuffer(size_t size_bytes) {
        if (size_bytes == 0) return nullptr;

        // Align to 4-byte boundary for optimal DMA performance
        size_t aligned_size = (size_bytes + 3) & ~3;

        return static_cast<uint8_t*>(
            heap_caps_malloc(aligned_size, MALLOC_CAP_DMA)
        );
    }

    /// Free DMA buffer
    void freeDMABuffer(uint8_t* buffer) {
        if (buffer) {
            heap_caps_free(buffer);
        }
    }

    /// Queue asynchronous DMA transmission (non-blocking)
    /// @param buffer Pointer to DMA-capable buffer
    /// @param length_bytes Data length in bytes
    /// @returns true if queued successfully
    bool transmitAsync(const uint8_t* buffer, size_t length_bytes) {
        if (!mInitialized) {
            return false;
        }

        // Wait for previous transaction if still active
        if (mTransactionActive) {
            waitComplete();
        }

        if (length_bytes == 0) {
            return true;  // Nothing to transmit
        }

        // Configure transaction
        memset(&mTransaction, 0, sizeof(mTransaction));
        mTransaction.flags = SPI_TRANS_MODE_QIO;  // Quad I/O mode
        mTransaction.length = length_bytes * 8;   // Length in BITS (critical!)
        mTransaction.tx_buffer = buffer;

        // Queue transaction (non-blocking)
        esp_err_t ret = spi_device_queue_trans(mSPIHandle, &mTransaction, portMAX_DELAY);
        if (ret != ESP_OK) {
            return false;
        }

        mTransactionActive = true;
        return true;
    }

    /// Wait for current transmission to complete (blocking)
    /// @param timeout_ms Maximum wait time in milliseconds (portMAX_DELAY for infinite)
    /// @returns true if completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = portMAX_DELAY) {
        if (!mTransactionActive) {
            return true;  // Nothing to wait for
        }

        spi_transaction_t* result = nullptr;
        esp_err_t ret = spi_device_get_trans_result(
            mSPIHandle,
            &result,
            pdMS_TO_TICKS(timeout_ms)
        );

        mTransactionActive = false;
        return (ret == ESP_OK);
    }

    /// Check if a transmission is currently in progress
    bool isBusy() const {
        return mTransactionActive;
    }

    /// Get initialization status
    bool isInitialized() const {
        return mInitialized;
    }

private:
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;
    spi_transaction_t mTransaction;
    bool mTransactionActive;
    bool mInitialized;

    /// Cleanup SPI resources
    void cleanup() {
        if (mInitialized) {
            // Wait for any pending transmission
            if (mTransactionActive) {
                waitComplete();
            }

            // Remove device and free bus
            if (mSPIHandle) {
                spi_bus_remove_device(mSPIHandle);
                mSPIHandle = nullptr;
            }

            spi_bus_free(mHost);
            mInitialized = false;
        }
    }

    // Non-copyable
    ESP32QuadSPIDriver(const ESP32QuadSPIDriver&) = delete;
    ESP32QuadSPIDriver& operator=(const ESP32QuadSPIDriver&) = delete;
};

}  // namespace fl

#endif  // ESP32 || ESP32S2 || ESP32S3 || ESP32C3 || ESP32P4
