/// @file spi_hw_4_esp32.cpp
/// @brief ESP32 implementation of 4-lane (Quad) SPI
///
/// This file provides the SPIQuadESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.
///
/// For 8-lane (Octal) SPI support, see spi_hw_8_esp32.cpp

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

#include "platforms/shared/spi_hw_4.h"
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
// SPIQuadESP32 Class Definition
// ============================================================================

/// ESP32 hardware for 4-lane (Quad) SPI DMA transmission
/// Implements SpiHw4 interface for ESP-IDF SPI peripheral (1-4 lanes)
class SPIQuadESP32 : public SpiHw4 {
public:
    explicit SPIQuadESP32(int bus_id = -1, const char* name = "Unknown");
    ~SPIQuadESP32();

    bool begin(const SpiHw4::Config& config) override;
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
    uint8_t mActiveLanes;

    // DMA buffer management
    DMABuffer mDMABuffer;            // Current DMA buffer
    bool mBufferAcquired;

    SPIQuadESP32(const SPIQuadESP32&) = delete;
    SPIQuadESP32& operator=(const SPIQuadESP32&) = delete;
};

// ============================================================================
// SPIQuadESP32 Implementation
// ============================================================================

SPIQuadESP32::SPIQuadESP32(int bus_id, const char* name)
    : mBusId(bus_id)
    , mName(name)
    , mSPIHandle(nullptr)
    , mHost(SPI2_HOST)
    , mTransactionActive(false)
    , mInitialized(false)
    , mActiveLanes(1)
    , mDMABuffer()
    , mBufferAcquired(false) {
    fl::memset(&mTransaction, 0, sizeof(mTransaction));
}

SPIQuadESP32::~SPIQuadESP32() {
    cleanup();
}

bool SPIQuadESP32::begin(const SpiHw4::Config& config) {
    FL_LOG_SPI("SPIQuadESP32::begin - Initializing Quad SPI");
    FL_LOG_SPI("Configuration Details:");
    FL_LOG_SPI("  Bus Number: " << static_cast<int>(config.bus_num));
    FL_LOG_SPI("  Clock Pin: " << config.clock_pin);
    FL_LOG_SPI("  Data0 Pin: " << config.data0_pin);
    FL_LOG_SPI("  Data1 Pin: " << config.data1_pin);
    FL_LOG_SPI("  Data2 Pin: " << config.data2_pin);
    FL_LOG_SPI("  Data3 Pin: " << config.data3_pin);
    FL_LOG_SPI("  Clock Speed: " << config.clock_speed_hz);

    if (mInitialized) {
        FL_LOG_SPI("SPIQuadESP32::begin - Already initialized, skipping");
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

    // Count active data pins to determine SPI mode (1-4 lanes)
    mActiveLanes = 1;  // data0 always present
    if (config.data1_pin >= 0) mActiveLanes++;
    if (config.data2_pin >= 0) mActiveLanes++;
    if (config.data3_pin >= 0) mActiveLanes++;

    // Configure SPI bus with appropriate mode flags
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = config.data0_pin;
    bus_config.miso_io_num = config.data1_pin;  // -1 if unused
    bus_config.sclk_io_num = config.clock_pin;
    bus_config.quadwp_io_num = config.data2_pin;  // -1 if unused
    bus_config.quadhd_io_num = config.data3_pin;  // -1 if unused
    bus_config.max_transfer_sz = config.max_transfer_sz;

    // Set flags based on active lane count (1-4 lanes)
    bus_config.flags = SPICOMMON_BUSFLAG_MASTER;
    if (mActiveLanes >= 4) {
        bus_config.flags |= SPICOMMON_BUSFLAG_QUAD;
    } else if (mActiveLanes >= 2) {
        bus_config.flags |= SPICOMMON_BUSFLAG_DUAL;
    }
    // else: standard SPI (1 data line)

    FL_LOG_SPI("SPIQuadESP32::begin - Active Lanes: " << static_cast<int>(mActiveLanes));
    FL_LOG_SPI("Bus Config Flags: " << fl::hex << bus_config.flags << fl::dec);

    // Initialize bus with auto DMA channel selection
    esp_err_t ret = spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        FL_LOG_SPI("SPIQuadESP32::begin - Bus initialization FAILED. ESP Error: " << esp_err_to_name(ret));
        return false;
    }
    FL_LOG_SPI("SPIQuadESP32::begin - Bus initialization successful");

    // Configure SPI device
    spi_device_interface_config_t dev_config = {};
    dev_config.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
    dev_config.clock_speed_hz = config.clock_speed_hz;
    dev_config.spics_io_num = -1;  // No CS pin for LED strips
    dev_config.queue_size = 1;  // Single transaction slot (double-buffered with CRGB buffer)
    dev_config.flags = SPI_DEVICE_HALFDUPLEX;  // Transmit-only mode

    FL_LOG_SPI("SPIQuadESP32::begin - Configuring device:");
    FL_LOG_SPI("  Mode: 0");
    FL_LOG_SPI("  Clock Speed: " << dev_config.clock_speed_hz);
    FL_LOG_SPI("  Queue Size: " << dev_config.queue_size);

    // Add device to bus
    ret = spi_bus_add_device(mHost, &dev_config, &mSPIHandle);
    if (ret != ESP_OK) {
        FL_LOG_SPI("SPIQuadESP32::begin - Device addition FAILED. ESP Error: " << esp_err_to_name(ret));
        spi_bus_free(mHost);
        return false;
    }
    FL_LOG_SPI("SPIQuadESP32::begin - Device added successfully");

    mInitialized = true;
    mTransactionActive = false;

    FL_LOG_SPI("SPIQuadESP32::begin - Quad SPI initialization SUCCESSFUL");
    return true;
}

void SPIQuadESP32::end() {
    cleanup();
}

DMABuffer SPIQuadESP32::acquireDMABuffer(size_t bytes_per_lane) {
    if (!mInitialized) {
        return DMABuffer(SPIError::NOT_INITIALIZED);
    }

    // Auto-wait if previous transmission still active
    if (mTransactionActive) {
        if (!waitComplete()) {
            return DMABuffer(SPIError::BUSY);
        }
    }

    // For quad SPI: total size = bytes_per_lane × 4 lanes (interleaved)
    constexpr size_t num_lanes = 4;
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

bool SPIQuadESP32::transmit(TransmitMode mode) {
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

    // Set transaction mode based on lane count (1-4 lanes)
    if (mActiveLanes >= 4) {
        mTransaction.flags = SPI_TRANS_MODE_QIO;  // Quad I/O mode
    } else if (mActiveLanes >= 2) {
        mTransaction.flags = SPI_TRANS_MODE_DIO;  // Dual I/O mode
    } else {
        mTransaction.flags = 0;  // Standard SPI mode
    }

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

bool SPIQuadESP32::waitComplete(uint32_t timeout_ms) {
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

bool SPIQuadESP32::isBusy() const {
    return mTransactionActive;
}

bool SPIQuadESP32::isInitialized() const {
    return mInitialized;
}

int SPIQuadESP32::getBusId() const {
    return mBusId;
}

const char* SPIQuadESP32::getName() const {
    return mName;
}

void SPIQuadESP32::cleanup() {
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
///
/// IMPORTANT: ESP32's SPI peripheral QSPI mode (SPI_TRANS_MODE_QIO) is designed
/// for QSPI flash communication, NOT parallel LED strips. QSPI mode sends a
/// single byte stream to one device, splitting each byte across 4 data lines
/// (4 bits per clock cycle). This is fundamentally different from parallel LED
/// strips, which need 4 independent byte streams to 4 separate devices.
///
/// For parallel LED output on ESP32, use the I2S peripheral (SpiHw16) instead,
/// which supports 1-16 independent data streams with true parallel DMA output.
///
/// This factory returns an empty vector to force SPIBusManager to use SpiHw16
/// for 3-4 strip configurations instead of trying to use the broken SpiHw4.
fl::vector<SpiHw4*> SpiHw4::createInstances() {
    FL_LOG_SPI("SpiHw4::createInstances - ESP32 uses I2S (SpiHw16) for parallel strips");
    FL_LOG_SPI("Returning empty controller list - SPIBusManager will use SpiHw16 instead");

    // Return empty - ESP32 parallel LED support is via I2S (SpiHw16), not SPI peripheral
    return fl::vector<SpiHw4*>();
}

}  // namespace fl

#endif  // FL_IS_ESP32
