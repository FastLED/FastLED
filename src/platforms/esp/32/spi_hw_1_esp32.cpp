/// @file spi_single_esp32.cpp
/// @brief ESP32 implementation of Single-SPI (backwards compatibility layer)
///
/// This file provides the SPISingleESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.
///
/// **IMPORTANT COMPATIBILITY NOTE:**
/// This implementation uses BLOCKING transmission in transmit() for backwards
/// compatibility. While the interface appears async, the transmission completes
/// synchronously before returning.
///
/// This is to make it backwards compatbile with the original implementation.
///
/// TODO: Convert to true async DMA implementation in the future.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "platforms/shared/spi_hw_1.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring> // ok include

// Include soc_caps.h if available (ESP-IDF 4.0+)
// Older versions (like IDF 3.3) don't have this header
#include "fl/has_include.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABufferResult, TransmitMode, SPIError
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
// On older versions, use DMA channel 1 as default
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
// SPISingleESP32 Class Definition
// ============================================================================

/// ESP32 hardware for Single-SPI transmission
/// Implements SPISingle interface for ESP-IDF SPI peripheral
///
/// **COMPATIBILITY WARNING**: transmit() is currently BLOCKING
class SPISingleESP32 : public SpiHw1 {
public:
    explicit SPISingleESP32(int bus_id = -1, const char* name = "Unknown");
    ~SPISingleESP32();

    bool begin(const SpiHw1::Config& config) override;
    void end() override;
    DMABufferResult acquireDMABuffer(size_t bytes_per_lane) override;
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
    bool mInitialized;
    bool mTransactionActive;

    // DMA buffer management
    fl::span<uint8_t> mDMABuffer;    // Allocated DMA buffer
    size_t mMaxBytesPerLane;         // Max bytes per lane we've allocated for
    size_t mCurrentTotalSize;        // Current transmission size (for single-lane: bytes_per_lane * 1)
    bool mBufferAcquired;

    SPISingleESP32(const SPISingleESP32&) = delete;
    SPISingleESP32& operator=(const SPISingleESP32&) = delete;
};

// ============================================================================
// SPISingleESP32 Implementation
// ============================================================================

SPISingleESP32::SPISingleESP32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIHandle(nullptr)
    , mHost(SPI2_HOST)
    , mInitialized(false)
    , mTransactionActive(false)
    , mDMABuffer()
    , mMaxBytesPerLane(0)
    , mCurrentTotalSize(0)
    , mBufferAcquired(false) {
}

SPISingleESP32::~SPISingleESP32() {
    cleanup();
}

bool SPISingleESP32::begin(const SpiHw1::Config& config) {
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

    // Configure SPI bus for standard single-lane mode
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config.data_pin;
    bus_config.miso_io_num = -1;  // Not used for LED strips
    bus_config.sclk_io_num = config.clock_pin;
    bus_config.quadwp_io_num = -1;  // Not used
    bus_config.quadhd_io_num = -1;  // Not used
    bus_config.max_transfer_sz = config.max_transfer_sz;

    // Standard SPI mode (no dual/quad flags)
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;

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

    return true;
}

void SPISingleESP32::end() {
    cleanup();
}

DMABufferResult SPISingleESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return SPIError::NOT_INITIALIZED;
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return SPIError::BUSY;
        }
    }

    // For single SPI: total size = bytes_per_lane × 1 lane
    constexpr size_t num_lanes = 1;
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

bool SPISingleESP32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (mCurrentTotalSize == 0) {
        return true;  // Nothing to transmit
    }

    // Mode is currently ignored - always blocks (TODO: implement async DMA)
    (void)mode;

    // TODO: Convert to true async DMA implementation
    // Currently BLOCKING for backwards compatibility
    // This is a critical fallback path - do not break existing code!

    // Configure transaction using internal DMA buffer
    spi_transaction_t transaction = {};
    transaction.length = mCurrentTotalSize * 8;   // Length in BITS (critical!)
    transaction.tx_buffer = mDMABuffer.data();

    // BLOCKING transmission - completes before returning
    esp_err_t ret = spi_device_transmit(mSPIHandle, &transaction);
    if (ret != ESP_OK) {
        return false;
    }

    // Mark transaction as active (even though it's already done for blocking mode)
    mTransactionActive = true;

    // Transmission already complete at this point
    return true;
}

bool SPISingleESP32::waitComplete(uint32_t timeout_ms) {
    (void)timeout_ms;  // Unused - transmission already complete

    if (!mTransactionActive) {
        return true;  // Nothing to wait for
    }

    // Since transmit() is blocking, transmission is already complete
    mTransactionActive = false;

    // Auto-release DMA buffer
    mBufferAcquired = false;
    mCurrentTotalSize = 0;

    return true;
}

bool SPISingleESP32::isBusy() const {
    // Since transmit() is blocking, never busy
    return false;
}

bool SPISingleESP32::isInitialized() const {
    return mInitialized;
}

int SPISingleESP32::getBusId() const {
    return mBusId;
}

const char* SPISingleESP32::getName() const {
    return mName;
}

void SPISingleESP32::cleanup() {
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

// ============================================================================
// Factory Implementation
// ============================================================================

/// ESP32 factory override - returns available SPI bus instances
/// Strong definition overrides weak default
fl::vector<SpiHw1*> SpiHw1::createInstances() {
    fl::vector<SpiHw1*> controllers;

    // Bus 2 is available on all ESP32 platforms
    static SPISingleESP32 controller2(2, "SPI2");  // Bus 2 - static lifetime
    controllers.push_back(&controller2);

#if SOC_SPI_PERIPH_NUM > 2
    // Bus 3 is only available when SOC has more than 2 SPI peripherals
    static SPISingleESP32 controller3(3, "SPI3");  // Bus 3 - static lifetime
    controllers.push_back(&controller3);
#endif

    return controllers;
}

}  // namespace fl

#endif  // ESP32 variants
