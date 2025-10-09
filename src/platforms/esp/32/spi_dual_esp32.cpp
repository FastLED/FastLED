/// @file spi_dual_esp32.cpp
/// @brief ESP32 implementation of Dual-SPI
///
/// This file provides the SPIDualESP32 class and factory for ESP32 platforms.
/// All class definition and implementation is contained in this single file.

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "platforms/shared/spi_dual.h"
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_err.h>
#include <cstring>
#include "soc/soc_caps.h"

// Determine SPI3_HOST availability using SOC capability macro
// SOC_SPI_PERIPH_NUM indicates the number of SPI peripherals available
// SPI3_HOST is available when SOC_SPI_PERIPH_NUM > 2 (SPI1, SPI2, SPI3)
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

namespace fl {

// ============================================================================
// SPIDualESP32 Class Definition
// ============================================================================

/// ESP32 hardware for Dual-SPI DMA transmission
/// Implements SPIDual interface for ESP-IDF SPI peripheral
class SPIDualESP32 : public SPIDual {
public:
    explicit SPIDualESP32(int bus_id = -1, const char* name = "Unknown");
    ~SPIDualESP32();

    bool begin(const SPIDual::Config& config) override;
    void end() override;
    bool transmitAsync(fl::span<const uint8_t> buffer) override;
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
    , mInitialized(false) {
    memset(&mTransaction, 0, sizeof(mTransaction));
}

SPIDualESP32::~SPIDualESP32() {
    cleanup();
}

bool SPIDualESP32::begin(const SPIDual::Config& config) {
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

void SPIDualESP32::end() {
    cleanup();
}

bool SPIDualESP32::transmitAsync(fl::span<const uint8_t> buffer) {
    if (!mInitialized) {
        return false;
    }

    // Wait for previous transaction if still active
    if (mTransactionActive) {
        waitComplete();
    }

    if (buffer.empty()) {
        return true;  // Nothing to transmit
    }

    // Configure transaction
    memset(&mTransaction, 0, sizeof(mTransaction));
    mTransaction.flags = SPI_TRANS_MODE_DIO;  // Dual I/O mode
    mTransaction.length = buffer.size() * 8;   // Length in BITS (critical!)
    mTransaction.tx_buffer = buffer.data();

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
fl::vector<SPIDual*> SPIDual::createInstances() {
    fl::vector<SPIDual*> controllers;

    // Bus 2 is available on all ESP32 platforms
    static SPIDualESP32 controller2(2, "SPI2");  // Bus 2 - static lifetime
    controllers.push_back(&controller2);

#if SOC_SPI_PERIPH_NUM > 2
    // Bus 3 is only available when SOC has more than 2 SPI peripherals
    static SPIDualESP32 controller3(3, "SPI3");  // Bus 3 - static lifetime
    controllers.push_back(&controller3);
#endif

    return controllers;
}

}  // namespace fl

#endif  // ESP32 variants
