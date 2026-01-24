/// @file spi_single_esp32.cpp
/// @brief ESP32 implementation of Single-SPI
///
/// This file provides the SPISingleESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.
///
/// This implementation uses true async DMA via ESP-IDF's spi_device_queue_trans()
/// and spi_device_get_trans_result() functions for non-blocking transmission.

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_bus_manager.h"  // For DMABuffer, TransmitMode, SPIError
#include "platforms/esp/32/drivers/spi/spi_hw_base.h"  // Common ESP32 SPI definitions
#include "fl/stl/cstring.h"
#include "fl/numeric_limits.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring> // ok include

namespace fl {

// ============================================================================
// SPISingleESP32 Class Definition
// ============================================================================

/// ESP32 hardware for Single-SPI transmission
/// Implements SPISingle interface for ESP-IDF SPI peripheral
///
/// This implementation uses async DMA via spi_device_queue_trans()
class SPISingleESP32 : public SpiHw1 {
public:
    explicit SPISingleESP32(int bus_id = -1, const char* name = "Unknown");
    ~SPISingleESP32();

    bool begin(const SpiHw1::Config& config) override;
    void end() override;
    DMABuffer acquireDMABuffer(size_t bytes_per_lane) override;
    bool transmit(TransmitMode mode = TransmitMode::ASYNC) override;
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max()) override;
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
    DMABuffer mDMABuffer;            // Current DMA buffer
    bool mBufferAcquired;

    // Transaction structure for async DMA
    spi_transaction_t mTransaction;

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

DMABuffer SPISingleESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For single SPI: total size = bytes_per_lane × 1 lane
    constexpr size_t num_lanes = 1;
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

bool SPISingleESP32::transmit(TransmitMode mode) {
    if (!mInitialized || !mBufferAcquired) {
        return false;
    }

    if (!mDMABuffer.ok() || mDMABuffer.size() == 0) {
        return true;  // Nothing to transmit
    }

    // Mode is ignored - ESP32 always does async via DMA
    (void)mode;

    // Get the buffer span
    fl::span<uint8_t> buffer_span = mDMABuffer.data();

    // Configure transaction using internal DMA buffer
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
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

bool SPISingleESP32::waitComplete(uint32_t timeout_ms) {
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

bool SPISingleESP32::isBusy() const {
    return mTransactionActive;
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
// Instance Management (accessed by spi_esp32_init.cpp)
// ============================================================================

// Singleton getters for controller instances (Meyer's Singleton pattern)
// These are called from the centralized registration in spi_esp32_init.cpp
// Return as SpiHw1 base class pointer to avoid forward declaration issues
fl::shared_ptr<SpiHw1>& getController2() {
    static fl::shared_ptr<SpiHw1> instance = fl::make_shared<SPISingleESP32>(2, "SPI2");
    return instance;
}

#if SOC_SPI_PERIPH_NUM > 2
fl::shared_ptr<SpiHw1>& getController3() {
    static fl::shared_ptr<SpiHw1> instance = fl::make_shared<SPISingleESP32>(3, "SPI3");
    return instance;
}
#endif

}  // namespace fl

#endif  // FL_IS_ESP32
