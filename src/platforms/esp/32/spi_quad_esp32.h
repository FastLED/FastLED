#pragma once

/// @file spi_quad_esp32.h
/// @brief ESP32 hardware for Quad-SPI DMA transmission
///
/// This wraps ESP-IDF SPI Master APIs to provide:
/// - Quad-SPI mode configuration (4 data lines)
/// - DMA buffer allocation and management
/// - Asynchronous transaction queueing
/// - RAII resource management
///
/// Compatible with ESP32, ESP32-S2, ESP32-S3, ESP32-C3 variants.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "fl/namespace.h"
#include "platforms/shared/spi_quad.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring>

namespace fl {

/// ESP32 hardware for Quad-SPI DMA transmission
/// Implements SPIQuad interface for ESP-IDF SPI peripheral
/// @note Compatible with ESP32, ESP32-S2, ESP32-S3, ESP32-C3 variants
class SPIQuadESP32 : public SPIQuad {
public:

    /// Constructor with bus ID and peripheral name assignment
    /// @param bus_id SPI bus number (2 or 3 for ESP32)
    /// @param name Platform-specific peripheral name (e.g., "HSPI", "VSPI")
    explicit SPIQuadESP32(int bus_id = -1, const char* name = "Unknown")
        : mBusId(bus_id)
        , mName(name)
        , mSPIHandle(nullptr)
        , mHost(SPI2_HOST)
        , mTransactionActive(false)
        , mInitialized(false) {
        memset(&mTransaction, 0, sizeof(mTransaction));
    }

    ~SPIQuadESP32() {
        cleanup();
    }

    /// Initialize SPI peripheral with dynamic mode detection
    /// @param config Hardware configuration (from SPIQuad interface)
    /// @returns true on success, false on error
    /// @note Automatically selects dual/quad/octal mode based on active pins
    /// @note If mBusId is set, config.bus_num must match (validation)
    bool begin(const SPIQuad::Config& config) override {
        if (mInitialized) {
            return true;  // Already initialized
        }

        // Validate bus_num against mBusId if driver has pre-assigned ID
        if (mBusId != -1 && config.bus_num != static_cast<uint8_t>(mBusId)) {
            return false;  // Mismatch: driver is for bus X but config requests bus Y
        }

        // Use config.bus_num to determine SPI host
        uint8_t bus_num = (mBusId != -1) ? static_cast<uint8_t>(mBusId) : config.bus_num;

        // Convert platform-agnostic bus_num to ESP32 SPI host
        if (bus_num == 2) {
            mHost = SPI2_HOST;
        } else if (bus_num == 3) {
            mHost = SPI3_HOST;
        } else {
            return false;  // Invalid bus number
        }

        // Count active data pins to determine SPI mode
        uint8_t active_lanes = 1;  // data0 always present
        if (config.data1_pin >= 0) active_lanes++;
        if (config.data2_pin >= 0) active_lanes++;
        if (config.data3_pin >= 0) active_lanes++;

        // Configure SPI bus with appropriate mode flags
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = config.data0_pin;
        bus_config.miso_io_num = config.data1_pin;  // -1 if unused
        bus_config.sclk_io_num = config.clock_pin;
        bus_config.quadwp_io_num = config.data2_pin;  // -1 if unused
        bus_config.quadhd_io_num = config.data3_pin;  // -1 if unused
        bus_config.max_transfer_sz = config.max_transfer_sz;

        // Set flags based on active lane count
        bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
        if (active_lanes >= 4) {
            bus_config.flags |= SPICOMMON_BUSFLAG_QUAD;
        } else if (active_lanes >= 2) {
            bus_config.flags |= SPICOMMON_BUSFLAG_DUAL;
        }
        // else: standard SPI (1 data line)

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
    uint8_t* allocateDMABuffer(size_t size_bytes) override {
        if (size_bytes == 0) return nullptr;

        // Align to 4-byte boundary for optimal DMA performance
        size_t aligned_size = (size_bytes + 3) & ~3;

        return static_cast<uint8_t*>(
            heap_caps_malloc(aligned_size, MALLOC_CAP_DMA)
        );
    }

    /// Free DMA buffer
    void freeDMABuffer(uint8_t* buffer) override {
        if (buffer) {
            heap_caps_free(buffer);
        }
    }

    /// Queue asynchronous DMA transmission (non-blocking)
    /// @param buffer Pointer to DMA-capable buffer
    /// @param length_bytes Data length in bytes
    /// @returns true if queued successfully
    bool transmitAsync(const uint8_t* buffer, size_t length_bytes) override {
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
    /// @param timeout_ms Maximum wait time in milliseconds
    /// @returns true if completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override {
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
    bool isBusy() const override {
        return mTransactionActive;
    }

    /// Get initialization status
    bool isInitialized() const override {
        return mInitialized;
    }

    /// Get the SPI bus ID for this driver
    int getBusId() const override {
        return mBusId;
    }

    /// Get the platform-specific peripheral name
    const char* getName() const override {
        return mName;
    }

private:
    int mBusId;                       ///< SPI bus number (2 or 3), -1 if unassigned
    const char* mName;                ///< Platform-specific peripheral name (e.g., "HSPI", "VSPI")
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
    SPIQuadESP32(const SPIQuadESP32&) = delete;
    SPIQuadESP32& operator=(const SPIQuadESP32&) = delete;
};

}  // namespace fl

#endif  // ESP32 || ESP32S2 || ESP32S3 || ESP32C3 || ESP32P4
