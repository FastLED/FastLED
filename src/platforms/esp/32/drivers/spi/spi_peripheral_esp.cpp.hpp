/// @file spi_peripheral_esp.cpp
/// @brief Real ESP32 SPI peripheral implementation
///
/// Thin wrapper around ESP-IDF SPI Master driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "spi_peripheral_esp.h"
#include "fl/log.h"
#include "fl/warn.h"
#include "fl/error.h"

// Include ESP-IDF headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "driver/spi_master.h"
#include "driver/spi_common.h"
#include "driver/gpio.h"        // For gpio_num_t
#include "esp_heap_caps.h"
#include "esp_timer.h"          // For esp_timer_get_time()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

namespace fl {
namespace detail {

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of SpiPeripheralESP
///
/// This class contains all ESP-IDF-specific implementation details.
class SpiPeripheralESPImpl : public SpiPeripheralESP {
public:
    SpiPeripheralESPImpl();
    ~SpiPeripheralESPImpl() override;

    // ISpiPeripheral Interface Implementation
    bool initializeBus(const SpiBusConfig& config) override;
    bool addDevice(const SpiDeviceConfig& config) override;
    bool removeDevice() override;
    bool freeBus() override;
    bool isInitialized() const override;
    bool queueTransaction(const SpiTransaction& trans) override;
    bool pollTransaction(uint32_t timeout_ms) override;
    bool registerCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDma(size_t size) override;
    void freeDma(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    uint64_t getMicroseconds() override;

private:
    ::spi_host_device_t mHost;           ///< SPI host (SPI2_HOST or SPI3_HOST)
    ::spi_device_handle_t mDeviceHandle; ///< ESP-IDF device handle
    bool mBusInitialized;                ///< Track bus initialization state
    bool mDeviceAdded;                   ///< Track device addition state
    void* mCallback;                     ///< Post-transaction callback
    void* mUserContext;                  ///< User context for callback
};

//=============================================================================
// Singleton Instance
//=============================================================================

SpiPeripheralESP& SpiPeripheralESP::instance() {
    return Singleton<SpiPeripheralESPImpl>::instance();
}

//=============================================================================
// Destructor (base class)
//=============================================================================

SpiPeripheralESP::~SpiPeripheralESP() {
    // Empty - implementation in SpiPeripheralESPImpl
}

//=============================================================================
// Constructor / Destructor (implementation)
//=============================================================================

SpiPeripheralESPImpl::SpiPeripheralESPImpl()
    : mHost(SPI2_HOST)
    , mDeviceHandle(nullptr)
    , mBusInitialized(false)
    , mDeviceAdded(false)
    , mCallback(nullptr)
    , mUserContext(nullptr) {
}

SpiPeripheralESPImpl::~SpiPeripheralESPImpl() {
    // Clean up device if added
    if (mDeviceAdded) {
        removeDevice();
    }

    // Clean up bus if initialized
    if (mBusInitialized) {
        freeBus();
    }
}

//=============================================================================
// Bus Lifecycle Methods
//=============================================================================

bool SpiPeripheralESPImpl::initializeBus(const SpiBusConfig& config) {
    // Validate not already initialized
    if (mBusInitialized) {
        FL_WARN("SpiPeripheralESP: Bus already initialized");
        return false;
    }

    // Configure SPI bus (maps directly to ESP-IDF structure)
    ::spi_bus_config_t bus_config = {};
    bus_config.miso_io_num = static_cast<gpio_num_t>(config.miso_pin);
    bus_config.mosi_io_num = static_cast<gpio_num_t>(config.mosi_pin);
    bus_config.sclk_io_num = static_cast<gpio_num_t>(config.sclk_pin);
    bus_config.quadwp_io_num = static_cast<gpio_num_t>(config.data2_pin);
    bus_config.quadhd_io_num = static_cast<gpio_num_t>(config.data3_pin);
    bus_config.max_transfer_sz = (config.max_transfer_sz > 0) ? config.max_transfer_sz : 4094;
    bus_config.flags = config.flags;

    // Use SPI2_HOST by default (SPI2 is the first general-purpose host)
    // Note: SPI1_HOST is reserved for flash/PSRAM
    mHost = SPI2_HOST;

    // Initialize bus with auto DMA channel selection (delegate to ESP-IDF)
    esp_err_t err = ::spi_bus_initialize(mHost, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to initialize bus: " << err);
        return false;
    }

    mBusInitialized = true;
    FL_DBG("SpiPeripheralESP: Bus initialized (MOSI=" << config.mosi_pin
           << ", SCLK=" << config.sclk_pin << ")");

    return true;
}

bool SpiPeripheralESPImpl::addDevice(const SpiDeviceConfig& config) {
    if (!mBusInitialized) {
        FL_WARN("SpiPeripheralESP: Cannot add device - bus not initialized");
        return false;
    }

    if (mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Device already added");
        return false;
    }

    // Configure SPI device (maps directly to ESP-IDF structure)
    ::spi_device_interface_config_t dev_config = {};
    dev_config.mode = config.mode;
    dev_config.clock_speed_hz = config.clock_speed_hz;
    dev_config.queue_size = config.queue_size;
    dev_config.flags = config.flags;
    dev_config.spics_io_num = static_cast<gpio_num_t>(config.spics_io_num);

    // Register post-transaction callback if set
    if (mCallback != nullptr) {
        dev_config.post_cb = reinterpret_cast<transaction_cb_t>(mCallback); // ok reinterpret cast
    }

    // Add device to bus (delegate to ESP-IDF)
    esp_err_t err = ::spi_bus_add_device(mHost, &dev_config, &mDeviceHandle);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to add device: " << err);
        return false;
    }

    mDeviceAdded = true;
    FL_DBG("SpiPeripheralESP: Device added (clock=" << config.clock_speed_hz
           << " Hz, queue=" << config.queue_size << ")");

    return true;
}

bool SpiPeripheralESPImpl::removeDevice() {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: No device to remove");
        return false;
    }

    // Wait for any pending transactions before removing device
    if (mDeviceHandle != nullptr) {
        // Poll for completion with timeout (do not block forever)
        ::spi_transaction_t* result = nullptr;
        while (::spi_device_get_trans_result(mDeviceHandle, &result, 0) == ESP_OK) {
            // Drain queue
        }

        // Remove device (delegate to ESP-IDF)
        esp_err_t err = ::spi_bus_remove_device(mDeviceHandle);
        if (err != ESP_OK) {
            FL_WARN("SpiPeripheralESP: Failed to remove device: " << err);
            return false;
        }

        mDeviceHandle = nullptr;
    }

    mDeviceAdded = false;
    FL_DBG("SpiPeripheralESP: Device removed");

    return true;
}

bool SpiPeripheralESPImpl::freeBus() {
    if (!mBusInitialized) {
        FL_WARN("SpiPeripheralESP: Bus not initialized");
        return false;
    }

    if (mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Cannot free bus - device still attached");
        return false;
    }

    // Free bus resources (delegate to ESP-IDF)
    esp_err_t err = ::spi_bus_free(mHost);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to free bus: " << err);
        return false;
    }

    mBusInitialized = false;
    FL_DBG("SpiPeripheralESP: Bus freed");

    return true;
}

bool SpiPeripheralESPImpl::isInitialized() const {
    return mBusInitialized && mDeviceAdded;
}

//=============================================================================
// Transaction Methods
//=============================================================================

bool SpiPeripheralESPImpl::queueTransaction(const SpiTransaction& trans) {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Cannot queue transaction - device not added");
        return false;
    }

    // Map to ESP-IDF transaction structure
    ::spi_transaction_t esp_trans = {};
    esp_trans.tx_buffer = trans.tx_buffer;
    esp_trans.length = trans.length_bits;
    esp_trans.flags = trans.flags;
    esp_trans.user = trans.user;

    // Queue transaction (delegate to ESP-IDF)
    // portMAX_DELAY = block until queue has space
    esp_err_t err = ::spi_device_queue_trans(mDeviceHandle, &esp_trans, portMAX_DELAY);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to queue transaction: " << err);
        return false;
    }

    return true;
}

bool SpiPeripheralESPImpl::pollTransaction(uint32_t timeout_ms) {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Cannot poll transaction - device not added");
        return false;
    }

    // Wait for transaction completion (delegate to ESP-IDF)
    ::spi_transaction_t* result = nullptr;
    TickType_t timeout_ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    esp_err_t err = ::spi_device_get_trans_result(mDeviceHandle, &result, timeout_ticks);
    if (err == ESP_ERR_TIMEOUT) {
        // Timeout is not an error - just return false
        return false;
    }

    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to get transaction result: " << err);
        return false;
    }

    return true;
}

bool SpiPeripheralESPImpl::registerCallback(void* callback, void* user_ctx) {
    // Store callback and user context
    mCallback = callback;
    mUserContext = user_ctx;

    // Note: The callback will be registered when addDevice() is called
    // (ESP-IDF requires callback to be set in device config)

    FL_DBG("SpiPeripheralESP: Callback registered");
    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

uint8_t* SpiPeripheralESPImpl::allocateDma(size_t size) {
    // Round up to 4-byte multiple (ESP32 DMA alignment requirement)
    size_t aligned_size = (size + 3) & ~3;

    // Allocate DMA-capable memory (delegate to ESP-IDF)
    uint8_t* buffer = static_cast<uint8_t*>(
        ::heap_caps_malloc(aligned_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    );

    if (buffer == nullptr) {
        FL_WARN("SpiPeripheralESP: Failed to allocate DMA buffer (" << size << " bytes)");
    }

    return buffer;
}

void SpiPeripheralESPImpl::freeDma(uint8_t* buffer) {
    if (buffer != nullptr) {
        ::heap_caps_free(buffer);
    }
}

//=============================================================================
// Platform Utilities
//=============================================================================

void SpiPeripheralESPImpl::delay(uint32_t ms) {
    ::vTaskDelay(pdMS_TO_TICKS(ms));
}

uint64_t SpiPeripheralESPImpl::getMicroseconds() {
    return ::esp_timer_get_time();
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI
#endif // ESP32
