/// @file spi_dual_esp32.cpp
/// @brief ESP32 implementation of Dual-SPI
///
/// This file provides the SPIDualESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "platforms/shared/spi_hw_2.h"
#include "fl/dbg.h"
#include "fl/log.h"
#include "fl/cstring.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring> // ok include

// Include soc_caps.h if available (ESP-IDF 4.0+)
// Older versions (like IDF 3.3) don't have this header
#include "fl/has_include.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#if FL_HAS_INCLUDE(<soc/soc_caps.h>)
  #include "soc/soc_caps.h"
#endif

// Determine SPI3_HOST availability using SOC capability macro
// SOC_SPI_PERIPH_NUM indicates the number of SPI peripherals available
// SPI3_HOST is available when SOC_SPI_PERIPH_NUM > 2 (SPI1, SPI2, SPI3)
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

// ESP-IDF 3.3 compatibility: SPI_DMA_CH_AUTO was added in IDF 4.0
#ifndef SPI_DMA_CH_AUTO
    #define SPI_DMA_CH_AUTO 1
#endif

// ESP-IDF compatibility: Ensure SPI host constants are defined
// Modern IDF uses SPI2_HOST/SPI3_HOST, older versions may not have them
#ifndef SPI2_HOST
    #define SPI2_HOST ((spi_host_device_t)1)
#endif
#ifndef SPI3_HOST
    #define SPI3_HOST ((spi_host_device_t)2)
#endif

namespace fl {

// ============================================================================
// SPIDualESP32 Class Definition
// ============================================================================

/// ESP32 hardware for Dual-SPI DMA transmission
/// Implements SPIDual interface for ESP-IDF SPI peripheral
class SPIDualESP32 : public SpiHw2 {
public:
    explicit SPIDualESP32(int bus_id = -1, const char* name = "Unknown");
    ~SPIDualESP32();

    bool begin(const SpiHw2::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t size) override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;
    bool waitComplete(uint32_t timeout_ms = UINT32_MAX) override;
    bool isBusy() const override;
    bool isInitialized() const override;
    int getBusId() const override;
    const char* getName() const override;

private:
    void cleanup();

    int mBusId;
    const char* mName;
    spi_device_handle_t mSPIHandle;
    spi_host_device_t mHost;
    spi_transaction_t mTransaction;
    bool mTransactionActive;
    bool mInitialized;

    // DMA buffer management
    DMABuffer mDMABuffer;            // Current DMA buffer
    bool mBufferAcquired;

    SPIDualESP32(const SPIDualESP32&) = delete;
    SPIDualESP32& operator=(const SPIDualESP32&) = delete;
};

// ============================================================================
// SPIDualESP32 Implementation
// ============================================================================

SPIDualESP32::SPIDualESP32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIHandle(nullptr)
    , mHost(SPI2_HOST)
    , mTransactionActive(false)
    , mInitialized(false)
    , mDMABuffer()
    , mBufferAcquired(false) {
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
}

SPIDualESP32::~SPIDualESP32() {
    cleanup();
}

bool SPIDualESP32::begin(const SpiHw2::Config& config) {
    FL_LOG_SPI("SPIDualESP32::begin - Initializing Dual SPI");
    FL_LOG_SPI("Configuration Details:");
    FL_LOG_SPI("  Bus Number: " << static_cast<int>(config.bus_num));
    FL_LOG_SPI("  Clock Pin: " << config.clock_pin);
    FL_LOG_SPI("  Data0 Pin: " << config.data0_pin);
    FL_LOG_SPI("  Data1 Pin: " << config.data1_pin);
    FL_LOG_SPI("  Clock Speed: " << config.clock_speed_hz);

    if (mInitialized) {
        FL_LOG_SPI("SPIDualESP32::begin - Already initialized, skipping");
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
    }
#if SOC_SPI_PERIPH_NUM > 2
    else if (bus_num == 3) {
        mHost = SPI3_HOST;
    }
#endif
    else {
        return false;  // Invalid bus number
    }

    // Configure SPI bus for dual mode
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config.data0_pin;
    bus_config.miso_io_num = config.data1_pin;
    bus_config.sclk_io_num = config.clock_pin;
    bus_config.quadwp_io_num = -1;  // Not used for dual mode
    bus_config.quadhd_io_num = -1;  // Not used for dual mode
    bus_config.max_transfer_sz = config.max_transfer_sz;

    // Set dual mode flag
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_DUAL;

    // Initialize bus with auto DMA channel selection
    esp_err_t ret = spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_LOG_SPI("SPIDualESP32::begin - Bus initialization FAILED. ESP Error: " << esp_err_to_name(ret));
        return false;
    }
    FL_LOG_SPI("SPIDualESP32::begin - Bus initialization successful");

    // Configure SPI device
    spi_device_interface_config_t dev_config = {};
    dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
    dev_config.clock_speed_hz = config.clock_speed_hz;
    dev_config.spics_io_num = -1;  // No CS pin for LED strips
    dev_config.queue_size = 1;  // Single transaction slot (double-buffered with CRGB buffer)
    dev_config.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only mode

    FL_LOG_SPI("SPIDualESP32::begin - Configuring device:");
    FL_LOG_SPI("  Mode: 0");
    FL_LOG_SPI("  Clock Speed: " << dev_config.clock_speed_hz);
    FL_LOG_SPI("  Queue Size: " << dev_config.queue_size);

    // Add device to bus
    ret = spi_bus_add_device(mHost, &dev_config, &mSPIHandle);
    if (ret != ESP_OK) {
        FL_LOG_SPI("SPIDualESP32::begin - Device addition FAILED. ESP Error: " << esp_err_to_name(ret));
        spi_bus_free(mHost);
        return false;
    }
    FL_LOG_SPI("SPIDualESP32::begin - Device added successfully");

    mInitialized = true;
    mTransactionActive = false;

    FL_LOG_SPI("SPIDualESP32::begin - Dual SPI initialization SUCCESSFUL");
    return true;
}

void SPIDualESP32::end() {
    cleanup();
}

DMABuffer SPIDualESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For dual SPI: total size = bytes_per_lane × 2 lanes (interleaved)
    constexpr size_t num_lanes = 2;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against max_transfer_sz from bus config
    // ESP32 SPI max is typically 64KB per transaction
    if (total_size > 65536) {
        return DMABuffer(SPIError::BUFFER_TOO_LARGE);
    }

    // Allocate new DMABuffer - it will manage its own memory
    mDMABuffer = DMABuffer(total_size);
    if (!mDMABuffer.ok()) {
        return DMABuffer(SPIError::ALLOCATION_FAILED);
    }

    mBufferAcquired = true;

    // Return the DMABuffer
    return mDMABuffer;
}

bool SPIDualESP32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is ignored - ESP32 always does async via DMA
    (void)mode;

    if (!mDMABuffer.ok() || mDMABuffer.size() == 0) {
        return true;  // Nothing to transmit
    }

    // Get the buffer span
    fl::span<uint8_t> buffer_span = mDMABuffer.data();

    // Configure transaction using internal DMA buffer
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
    mTransaction.flags = SPI_TRANS_MODE_DIO;  // Dual I/O mode
    mTransaction.length = buffer_span.size() * 8;   // Length in BITS (critical!)
    mTransaction.tx_buffer = buffer_span.data();

    // Queue transaction (non-blocking)
    esp_err_t ret = spi_device_queue_trans(mSPIHandle, &mTransaction, portMAX_DELAY);
    if (ret != ESP_OK) {
        return false;
    }

    mTransactionActive = true;
    return true;
}

bool SPIDualESP32::waitComplete(uint32_t timeout_ms) {
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

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mDMABuffer.reset();

    return (ret == ESP_OK);
}

bool SPIDualESP32::isBusy() const {
    return mTransactionActive;
}

bool SPIDualESP32::isInitialized() const {
    return mInitialized;
}

int SPIDualESP32::getBusId() const {
    return mBusId;
}

const char* SPIDualESP32::getName() const {
    return mName;
}

void SPIDualESP32::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Release DMA buffer
        mDMABuffer.reset();
        mBufferAcquired = false;

        // Remove device and free bus
        if (mSPIHandle) {
            spi_bus_remove_device(mSPIHandle);
            mSPIHandle = nullptr;
        }

        spi_bus_free(mHost);
        mInitialized = false;
    }
}

// ============================================================================
// Factory Implementation
// ============================================================================

/// ESP32 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw2*> SpiHw2::createInstances() {
    FL_LOG_SPI("SpiHw2::createInstances - Creating SPI Dual controllers");
    FL_LOG_SPI("SPI Peripheral Count: " << SOC_SPI_PERIPH_NUM);

    fl::vector<SpiHw2*> controllers;

    // Bus 2 is available on all ESP32 platforms
    static SPIDualESP32 controller2(2, "SPI2");
    FL_LOG_SPI("Adding SPI2 Controller");
    controllers.push_back(&controller2);

#if SOC_SPI_PERIPH_NUM > 2
    // Bus 3 is only available when SOC has more than 2 SPI peripherals
    static SPIDualESP32 controller3(3, "SPI3");
    FL_LOG_SPI("Adding SPI3 Controller");
    controllers.push_back(&controller3);
#endif

    FL_LOG_SPI("Created " << controllers.size() << " SPI Dual controllers");
    return controllers;
}

}  // namespace fl

#endif  // ESP32 variants
