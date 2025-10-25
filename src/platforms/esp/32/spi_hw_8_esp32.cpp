/// @file spi_hw_8_esp32.cpp
/// @brief ESP32 implementation of 8-lane (Octal) SPI
///
/// This file provides the SpiHw8ESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.
///
/// Octal-SPI (8-lane) support requires ESP-IDF 5.0+ and ESP32-P4 or similar
/// hardware with sufficient data lines.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "platforms/shared/spi_hw_8.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include "fl/cstring.h"
#include <cstring> // ok include

// Include soc_caps.h if available (ESP-IDF 4.0+)
#include "fl/has_include.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#if FL_HAS_INCLUDE(<soc/soc_caps.h>)
  #include "soc/soc_caps.h"
#endif

// Determine SPI3_HOST availability using SOC capability macro
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

// ESP-IDF 3.3 compatibility: SPI_DMA_CH_AUTO was added in IDF 4.0
#ifndef SPI_DMA_CH_AUTO
    #define SPI_DMA_CH_AUTO 1
#endif

// ESP-IDF compatibility: Ensure SPI host constants are defined
#ifndef SPI2_HOST
    #define SPI2_HOST ((spi_host_device_t)1)
#endif
#ifndef SPI3_HOST
    #define SPI3_HOST ((spi_host_device_t)2)
#endif

namespace fl {

// Only compile 8-lane support if ESP-IDF 5.0+ is available
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

// ============================================================================
// SpiHw8ESP32 Class Definition
// ============================================================================

/// ESP32 hardware for 8-lane (Octal) SPI DMA transmission
/// Implements SpiHw8 interface for ESP-IDF SPI peripheral (ESP-IDF 5.0+)
class SpiHw8ESP32 : public SpiHw8 {
public:
    explicit SpiHw8ESP32(int bus_id = -1, const char* name = "Unknown");
    ~SpiHw8ESP32();

    bool begin(const SpiHw8::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;
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
    fl::span<uint8_t> mDMABuffer;    // Allocated DMA buffer (interleaved format for multi-lane)
    size_t mMaxBytesPerLane;         // Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        // Current transmission size (bytes_per_lane * 8)
    bool mBufferAcquired;

    SpiHw8ESP32(const SpiHw8ESP32&) = delete;
    SpiHw8ESP32& operator=(const SpiHw8ESP32&) = delete;
};

// ============================================================================
// SpiHw8ESP32 Implementation
// ============================================================================

SpiHw8ESP32::SpiHw8ESP32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIHandle(nullptr)
    , mHost(SPI2_HOST)
    , mTransactionActive(false)
    , mInitialized(false)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false) {
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
}

SpiHw8ESP32::~SpiHw8ESP32() {
    cleanup();
}

bool SpiHw8ESP32::begin(const SpiHw8::Config& config) {
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
    }
#if SOC_SPI_PERIPH_NUM > 2
    else if (bus_num == 3) {
        mHost = SPI3_HOST;
    }
#endif
    else {
        return false;  // Invalid bus number
    }

    // Validate that all 8 data pins are specified
    if (config.data0_pin < 0 || config.data1_pin < 0 ||
        config.data2_pin < 0 || config.data3_pin < 0 ||
        config.data4_pin < 0 || config.data5_pin < 0 ||
        config.data6_pin < 0 || config.data7_pin < 0) {
        return false;  // 8-lane SPI requires all 8 data pins
    }

    // Configure SPI bus with octal mode
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config.data0_pin;
    bus_config.miso_io_num = config.data1_pin;
    bus_config.sclk_io_num = config.clock_pin;
    bus_config.quadwp_io_num = config.data2_pin;
    bus_config.quadhd_io_num = config.data3_pin;
    bus_config.data4_io_num = config.data4_pin;
    bus_config.data5_io_num = config.data5_pin;
    bus_config.data6_io_num = config.data6_pin;
    bus_config.data7_io_num = config.data7_pin;
    bus_config.max_transfer_sz = config.max_transfer_sz;

    // Set octal mode flags
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_OCTAL;

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
    dev_config.queue_size = 1;  // Single transaction slot (double-buffered with CRGB buffer)
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

void SpiHw8ESP32::end() {
    cleanup();
}

DMABuffer SpiHw8ESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return SPIError::BUSY;
        }
    }

    // For octal SPI: total size = bytes_per_lane × 8 lanes (interleaved)
    constexpr size_t num_lanes = 8;
    const size_t total_size = bytes_per_lane * num_lanes;

    // Validate size against max_transfer_sz from bus config
    // ESP32 SPI max is typically 64KB per transaction
    if (total_size > 65536) {
        return SPIError::BUFFER_TOO_LARGE;
    }

    // Reallocate buffer only if we need more capacity
    if (bytes_per_lane > mMaxBytesPerLane) {
        if (!mDMABuffer.empty()) {
            heap_caps_free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
        }

        // Allocate DMA-capable memory for max size
        uint8_t* ptr = (uint8_t*)heap_caps_malloc(total_size, MALLOC_CAP_DMA);
        if (!ptr) {
            return SPIError::ALLOCATION_FAILED;
        }

        mDMABuffer = fl::span<uint8_t>(ptr, total_size);
        mMaxBytesPerLane = bytes_per_lane;
    }

    mBufferAcquired = true;
    mCurrentTotalSize = total_size;

    // Return span of current size (not max allocated size)
    return fl::span<uint8_t>(mDMABuffer.data(), total_size);
}

bool SpiHw8ESP32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    // Mode is ignored - ESP32 always does async via DMA
    (void)mode;

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Configure transaction for octal mode using internal DMA buffer
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
    mTransaction.flags = SPI_TRANS_MODE_OCT;  // Octal I/O mode
    mTransaction.length = mCurrentTotalSize * 8;   // Length in BITS (critical!)
    mTransaction.tx_buffer = mDMABuffer.data();

    // Queue transaction (non-blocking)
    esp_err_t ret = spi_device_queue_trans(mSPIHandle, &mTransaction, portMAX_DELAY);
    if (ret != ESP_OK) {
        return false;
    }

    mTransactionActive = true;
    return true;
}

bool SpiHw8ESP32::waitComplete(uint32_t timeout_ms) {
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
    mCurrentTotalSize = 0;

    return (ret == ESP_OK);
}

bool SpiHw8ESP32::isBusy() const {
    return mTransactionActive;
}

bool SpiHw8ESP32::isInitialized() const {
    return mInitialized;
}

int SpiHw8ESP32::getBusId() const {
    return mBusId;
}

const char* SpiHw8ESP32::getName() const {
    return mName;
}

void SpiHw8ESP32::cleanup() {
    if (mInitialized) {
        // Wait for any pending transmission
        if (mTransactionActive) {
            waitComplete();
        }

        // Free DMA buffer
        if (!mDMABuffer.empty()) {
            heap_caps_free(mDMABuffer.data());
            mDMABuffer = fl::span<uint8_t>();
            mMaxBytesPerLane = 0;
            mCurrentTotalSize = 0;
            mBufferAcquired = false;
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

#endif  // ESP_IDF_VERSION >= 5.0.0

// ============================================================================
// Factory Implementation
// ============================================================================

/// ESP32 factory override - returns available 8-lane SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw8*> SpiHw8::createInstances() {
    fl::vector<SpiHw8*> controllers;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // Octal-SPI is only available on ESP-IDF 5.0+
    // Note: Not all ESP32 variants support octal mode even with IDF 5.0+
    // ESP32-P4 and some newer chips support it

    // Bus 2 is available on all ESP32 platforms
    static SpiHw8ESP32 controller2(2, "SPI2_OCTAL");  // Bus 2 - static lifetime
    controllers.push_back(&controller2);

#if SOC_SPI_PERIPH_NUM > 2
    // Bus 3 is only available when SOC has more than 2 SPI peripherals
    static SpiHw8ESP32 controller3(3, "SPI3_OCTAL");  // Bus 3 - static lifetime
    controllers.push_back(&controller3);
#endif

#endif  // ESP_IDF_VERSION >= 5.0.0

    return controllers;
}

}  // namespace fl

#endif  // ESP32 variants
