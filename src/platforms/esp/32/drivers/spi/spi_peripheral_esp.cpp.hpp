// IWYU pragma: private

/// @file spi_peripheral_esp.cpp
/// @brief Real ESP32 SPI peripheral implementation
///
/// Thin wrapper around ESP-IDF SPI Master driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "platforms/esp/32/drivers/spi/spi_peripheral_esp.h"
#include "platforms/esp/esp_version.h"
#include "fl/log/log.h"
#include "fl/stl/array.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

// Include ESP-IDF headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/spi_master.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "driver/spi_common.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "driver/gpio.h"        // For gpio_num_t
// IWYU pragma: end_keep
#include "esp_heap_caps.h"
#include "esp_timer.h"          // For esp_timer_get_time()
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace detail {

FL_STATIC_ASSERT(kSpiDeviceHalfDuplex == SPI_DEVICE_HALFDUPLEX,
                 "kSpiDeviceHalfDuplex must match ESP-IDF");
FL_STATIC_ASSERT(kSpiTransModeDio == SPI_TRANS_MODE_DIO,
                 "kSpiTransModeDio must match ESP-IDF");
FL_STATIC_ASSERT(kSpiTransModeQio == SPI_TRANS_MODE_QIO,
                 "kSpiTransModeQio must match ESP-IDF");

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of SpiPeripheralESP
///
/// This class contains all ESP-IDF-specific implementation details.
class SpiPeripheralESPImpl : public SpiPeripheralESP {
public:
    SpiPeripheralESPImpl() FL_NO_EXCEPT;
    ~SpiPeripheralESPImpl() override;

    // ISpiPeripheral Interface Implementation
    bool initializeBus(const SpiBusConfig& config) FL_NO_EXCEPT override;
    bool addDevice(const SpiDeviceConfig& config) FL_NO_EXCEPT override;
    bool removeDevice() FL_NO_EXCEPT override;
    bool freeBus() FL_NO_EXCEPT override;
    bool isInitialized() const FL_NO_EXCEPT override;
    bool queueTransaction(const SpiTransaction& trans) FL_NO_EXCEPT override;
    bool pollTransaction(u32 timeout_ms, void** out_user) FL_NO_EXCEPT override;
    bool pollingStart(const SpiTransaction& trans) FL_NO_EXCEPT override;
    bool pollingEnd(u32 timeout_ms) FL_NO_EXCEPT override;
    int getActualFrequencyKhz() const FL_NO_EXCEPT override;
    bool registerCallback(void* callback, void* user_ctx) FL_NO_EXCEPT override;
    u8* allocateDma(size_t size) FL_NO_EXCEPT override;
    void freeDma(u8* buffer) FL_NO_EXCEPT override;
    u8* allocateInternal(size_t size) FL_NO_EXCEPT override;
    void freeInternal(u8* buffer) FL_NO_EXCEPT override;
    void resetPin(int pin) FL_NO_EXCEPT override;
    u32 getPinRouting(int pin) FL_NO_EXCEPT override;
    void delay(u32 ms) FL_NO_EXCEPT override;
    u64 getMicroseconds() FL_NO_EXCEPT override;

private:
    /// @brief Copy @p trans into the next persistent ESP-IDF descriptor
    ///
    /// ESP-IDF keeps the spi_transaction_t pointer until the transaction
    /// completes, so the descriptor cannot live on the caller's stack.
    ::spi_transaction_t* fillTransactionSlot(const SpiTransaction& trans) FL_NO_EXCEPT;

    /// Descriptor ring. Sized to the device queue depth ChannelEngineSpi
    /// uses (4); it keeps at most two transactions in flight.
    static constexpr size_t kTransactionSlots = 4;
    fl::array<::spi_transaction_t, kTransactionSlots> mTransactions;
    size_t mNextTransaction;

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

SpiPeripheralESP& SpiPeripheralESP::instance() FL_NO_EXCEPT {
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

SpiPeripheralESPImpl::SpiPeripheralESPImpl() FL_NO_EXCEPT
    : mHost(SPI2_HOST)
    , mDeviceHandle(nullptr)
    , mBusInitialized(false)
    , mDeviceAdded(false)
    , mCallback(nullptr)
    , mUserContext(nullptr)
    , mTransactions()
    , mNextTransaction(0) {
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

bool SpiPeripheralESPImpl::initializeBus(const SpiBusConfig& config) FL_NO_EXCEPT {
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
    FL_DBG("SpiPeripheralESP: Bus initialized (MOSI=" << config.mosi_pin << ", SCLK=" << config.sclk_pin << ")");

    return true;
}

bool SpiPeripheralESPImpl::addDevice(const SpiDeviceConfig& config) FL_NO_EXCEPT {
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
    dev_config.clock_source = SPI_CLK_SRC_DEFAULT;
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
    FL_DBG("SpiPeripheralESP: Device added (clock=" << config.clock_speed_hz << " Hz, queue=" << config.queue_size << ")");

    return true;
}

bool SpiPeripheralESPImpl::removeDevice() FL_NO_EXCEPT {
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

bool SpiPeripheralESPImpl::freeBus() FL_NO_EXCEPT {
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

bool SpiPeripheralESPImpl::isInitialized() const FL_NO_EXCEPT {
    return mBusInitialized && mDeviceAdded;
}

//=============================================================================
// Transaction Methods
//=============================================================================

bool SpiPeripheralESPImpl::queueTransaction(const SpiTransaction& trans) FL_NO_EXCEPT {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Cannot queue transaction - device not added");
        return false;
    }

    // Queue transaction (delegate to ESP-IDF)
    // portMAX_DELAY = block until queue has space
    esp_err_t err = ::spi_device_queue_trans(mDeviceHandle, fillTransactionSlot(trans), portMAX_DELAY);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to queue transaction: " << err);
        return false;
    }

    return true;
}

bool SpiPeripheralESPImpl::pollTransaction(u32 timeout_ms, void** out_user) FL_NO_EXCEPT {
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

    if (out_user != nullptr) {
        *out_user = (result != nullptr) ? result->user : nullptr;
    }
    return true;
}

bool SpiPeripheralESPImpl::pollingStart(const SpiTransaction& trans) FL_NO_EXCEPT {
    if (!mDeviceAdded) {
        FL_WARN("SpiPeripheralESP: Cannot start polling transaction - device not added");
        return false;
    }

    esp_err_t err = ::spi_device_polling_start(mDeviceHandle, fillTransactionSlot(trans), portMAX_DELAY);
    if (err != ESP_OK) {
        FL_WARN("SpiPeripheralESP: Failed to start polling transaction: " << err);
        return false;
    }
    return true;
}

bool SpiPeripheralESPImpl::pollingEnd(u32 timeout_ms) FL_NO_EXCEPT {
    if (!mDeviceAdded) {
        return false;
    }
    TickType_t timeout_ticks = 0;
    if (timeout_ms == kSpiWaitForeverMs) {
        timeout_ticks = portMAX_DELAY;
    } else if (timeout_ms != 0) {
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    }
    // ESP_ERR_TIMEOUT means still in flight; the caller polls again.
    return ::spi_device_polling_end(mDeviceHandle, timeout_ticks) == ESP_OK;
}

int SpiPeripheralESPImpl::getActualFrequencyKhz() const FL_NO_EXCEPT {
    int freq_khz = 0;
    if (mDeviceHandle != nullptr) {
        ::spi_device_get_actual_freq(mDeviceHandle, &freq_khz);
    }
    return freq_khz;
}

::spi_transaction_t* SpiPeripheralESPImpl::fillTransactionSlot(const SpiTransaction& trans) FL_NO_EXCEPT {
    // fl-lint: container-data-ptr-ok ESP-IDF C API holds this slot pointer until the transaction completes
    ::spi_transaction_t* esp_trans = &mTransactions[mNextTransaction];
    mNextTransaction = (mNextTransaction + 1) % kTransactionSlots;
    *esp_trans = {};
    esp_trans->tx_buffer = trans.tx_buffer;
    esp_trans->length = trans.length_bits;
    esp_trans->flags = trans.flags;
    esp_trans->user = trans.user;
    return esp_trans;
}

bool SpiPeripheralESPImpl::registerCallback(void* callback, void* user_ctx) FL_NO_EXCEPT {
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

u8* SpiPeripheralESPImpl::allocateDma(size_t size) FL_NO_EXCEPT {
    // Round up to 4-byte multiple (ESP32 DMA alignment requirement)
    size_t aligned_size = (size + 3) & ~3;

    // Allocate DMA-capable memory (delegate to ESP-IDF)
    u8* buffer = static_cast<u8*>(
        ::heap_caps_malloc(aligned_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)
    );

    if (buffer == nullptr) {
        FL_WARN("SpiPeripheralESP: Failed to allocate DMA buffer (" << size << " bytes)");
    }

    return buffer;
}

void SpiPeripheralESPImpl::freeDma(u8* buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
        ::heap_caps_free(buffer);
    }
}

u8* SpiPeripheralESPImpl::allocateInternal(size_t size) FL_NO_EXCEPT {
    return static_cast<u8*>(
        ::heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

void SpiPeripheralESPImpl::freeInternal(u8* buffer) FL_NO_EXCEPT {
    if (buffer != nullptr) {
        ::heap_caps_free(buffer);
    }
}

//=============================================================================
// Pin Routing
//=============================================================================

void SpiPeripheralESPImpl::resetPin(int pin) FL_NO_EXCEPT {
    ::gpio_reset_pin(static_cast<gpio_num_t>(pin));
}

u32 SpiPeripheralESPImpl::getPinRouting(int pin) FL_NO_EXCEPT {
    // FastLED#4907: needs gpio_get_io_config() (IDF 5.5); older cores report
    // unknown and keep the previous behaviour.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    gpio_io_config_t io = {};
    if (::gpio_get_io_config(static_cast<gpio_num_t>(pin), &io) != ESP_OK) {
        return kPinRoutingUnknown;
    }
    return (io.sig_out & 0xFFFFu) | (io.oe ? (1u << 16) : 0u) |
           ((io.fun_sel & 0xFFu) << 20);
#else
    (void)pin;
    return kPinRoutingUnknown;
#endif
}

//=============================================================================
// Platform Utilities
//=============================================================================

void SpiPeripheralESPImpl::delay(u32 ms) FL_NO_EXCEPT {
    ::vTaskDelay(pdMS_TO_TICKS(ms));
}

u64 SpiPeripheralESPImpl::getMicroseconds() FL_NO_EXCEPT {
    return ::esp_timer_get_time();
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI
#endif // FL_IS_ESP32
