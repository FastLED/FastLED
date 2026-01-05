/// @file parlio_peripheral_esp.cpp
/// @brief Real ESP32 PARLIO peripheral implementation
///
/// Thin wrapper around ESP-IDF PARLIO TX driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "parlio_peripheral_esp.h"
#include "fl/log.h"
#include "fl/warn.h"

// Include ESP-IDF headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "driver/gptimer.h"
#include "driver/parlio_tx.h"
#include "esp_cache.h"      // For esp_cache_msync (DMA cache coherency)
#include "esp_heap_caps.h"
#include "esp_timer.h"      // For esp_timer_get_time()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

#include "platforms/memory_barrier.h"  // For FL_MEMORY_BARRIER

    // Set interrupt priority (configurable via FL_ESP_PARLIO_ISR_PRIORITY)
    // Default is level 3 (highest for C handlers on ESP32)
#ifndef FL_ESP_PARLIO_ISR_PRIORITY
#define FL_ESP_PARLIO_ISR_PRIORITY 3
#endif

namespace fl {
namespace detail {

//=============================================================================
// Implementation Class (internal)
//=============================================================================

/// @brief Internal implementation of ParlioPeripheralESP
///
/// This class contains all ESP-IDF-specific implementation details.
class ParlioPeripheralESPImpl : public ParlioPeripheralESP {
public:
    ParlioPeripheralESPImpl();
    ~ParlioPeripheralESPImpl() override;

    // IParlioPeripheral Interface Implementation
    bool initialize(const ParlioPeripheralConfig& config) override;
    bool enable() override;
    bool disable() override;
    bool isInitialized() const override;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override;
    bool waitAllDone(uint32_t timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;
    void delay(uint32_t ms) override;
    uint64_t getMicroseconds() override;
    void freeDmaBuffer(void* ptr) override;

private:
    ::parlio_tx_unit_handle_t mTxUnit;  ///< ESP-IDF TX unit handle
    bool mEnabled;                       ///< Track enable state (for cleanup)
};

//=============================================================================
// Singleton Instance
//=============================================================================

ParlioPeripheralESP& ParlioPeripheralESP::instance() {
    return Singleton<ParlioPeripheralESPImpl>::instance();
}

//=============================================================================
// Destructor (base class)
//=============================================================================

ParlioPeripheralESP::~ParlioPeripheralESP() {
    // Empty - implementation in ParlioPeripheralESPImpl
}

//=============================================================================
// Constructor / Destructor (implementation)
//=============================================================================

ParlioPeripheralESPImpl::ParlioPeripheralESPImpl()
    : mTxUnit(nullptr),
      mEnabled(false) {
}

ParlioPeripheralESPImpl::~ParlioPeripheralESPImpl() {
    // Clean up TX unit if initialized
    if (mTxUnit != nullptr) {
        // Wait for any pending transmissions (with timeout)
        esp_err_t err = parlio_tx_unit_wait_all_done(mTxUnit, pdMS_TO_TICKS(1000));
        if (err != ESP_OK) {
            FL_LOG_PARLIO("ParlioPeripheralESP: Wait timeout during cleanup: " << err);
        }

        // Disable TX unit if enabled
        if (mEnabled) {
            err = parlio_tx_unit_disable(mTxUnit);
            if (err != ESP_OK) {
                FL_LOG_PARLIO("ParlioPeripheralESP: Failed to disable TX unit: " << err);
            }
            mEnabled = false;
        }

        // Delete TX unit
        err = parlio_del_tx_unit(mTxUnit);
        if (err != ESP_OK) {
            FL_LOG_PARLIO("ParlioPeripheralESP: Failed to delete TX unit: " << err);
        }

        mTxUnit = nullptr;
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool ParlioPeripheralESPImpl::initialize(const ParlioPeripheralConfig& config) {
    // Validate not already initialized
    if (mTxUnit != nullptr) {
        FL_WARN("ParlioPeripheralESP: Already initialized");
        return false;
    }

    // Configure PARLIO TX unit (maps directly to ESP-IDF structure)
    parlio_tx_unit_config_t esp_config = {};
    esp_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    esp_config.clk_in_gpio_num = static_cast<gpio_num_t>(-1);
    esp_config.output_clk_freq_hz = config.clock_freq_hz;
    esp_config.data_width = config.data_width;
    esp_config.trans_queue_depth = config.queue_depth;
    esp_config.max_transfer_size = config.max_transfer_size;
    esp_config.bit_pack_order = PARLIO_BIT_PACK_ORDER_LSB;
    esp_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;


    esp_config.intr_priority = FL_ESP_PARLIO_ISR_PRIORITY;

    // Assign GPIO pins
    for (size_t i = 0; i < 16; i++) {
        esp_config.data_gpio_nums[i] = static_cast<gpio_num_t>(config.gpio_pins[i]);
    }

    // No external clock or valid signal
    esp_config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    esp_config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Create TX unit (delegate to ESP-IDF)
    esp_err_t err = parlio_new_tx_unit(&esp_config, &mTxUnit);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to create TX unit: " << err);
        mTxUnit = nullptr;
        return false;
    }

    FL_LOG_PARLIO("PARLIO: Initialized with ISR priority level " << FL_ESP_PARLIO_ISR_PRIORITY
                  << " (data_width=" << config.data_width
                  << ", clock=" << config.clock_freq_hz << " Hz)");

    return true;
}

bool ParlioPeripheralESPImpl::enable() {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot enable - not initialized");
        return false;
    }

    // Delegate to ESP-IDF
    esp_err_t err = parlio_tx_unit_enable(mTxUnit);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to enable TX unit: " << err);
        return false;
    }

    mEnabled = true;
    return true;
}

bool ParlioPeripheralESPImpl::disable() {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot disable - not initialized");
        return false;
    }

    // Delegate to ESP-IDF
    esp_err_t err = parlio_tx_unit_disable(mTxUnit);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to disable TX unit: " << err);
        return false;
    }

    mEnabled = false;
    return true;
}

bool ParlioPeripheralESPImpl::isInitialized() const {
    // Real hardware: initialized if TX unit handle is valid
    return mTxUnit != nullptr;
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool ParlioPeripheralESPImpl::transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot transmit - not initialized");
        return false;
    }

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot transmit - null buffer");
        return false;
    }

    // CRITICAL: Flush CPU cache to memory before DMA reads buffer
    // This is the ONLY place cache sync happens (before DMA submission)
    // Calculate buffer size in bytes (bit_count / 8)
    size_t buffer_size = (bit_count + 7) / 8;

    // Memory barrier: Ensure all preceding writes complete before DMA submission
    FL_MEMORY_BARRIER;

    // Cache sync: SKIP for DMA buffers
    // Buffers are allocated with MALLOC_CAP_DMA (non-cacheable SRAM1 on ESP32-C6)
    // esp_cache_msync() is unnecessary for non-cacheable memory and causes:
    //   E (xxxx) cache: esp_cache_msync(103): invalid addr or null pointer
    // on ESP32-C6. Memory barriers provide sufficient ordering guarantees.
    // See parlio_engine.cpp lines 881-891 for buffer allocation details.

    // Memory barrier: Ensure writes complete before DMA submission
    FL_MEMORY_BARRIER;

    // Prepare transmission payload
    parlio_transmit_config_t payload = {};
    payload.idle_value = idle_value;

    // Delegate to ESP-IDF (ISR-safe call)
    esp_err_t err = parlio_tx_unit_transmit(mTxUnit, buffer, bit_count, &payload);
    if (err != ESP_OK) {
        // Don't log in potential ISR context - return error silently
        return false;
    }

    return true;
}

bool ParlioPeripheralESPImpl::waitAllDone(uint32_t timeout_ms) {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot wait - not initialized");
        return false;
    }

    // Convert timeout to FreeRTOS ticks
    TickType_t timeout_ticks;
    if (timeout_ms == 0) {
        timeout_ticks = 0;  // Non-blocking poll
    } else {
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    }

    // Delegate to ESP-IDF
    esp_err_t err = parlio_tx_unit_wait_all_done(mTxUnit, timeout_ticks);

    // ESP_OK means all done, ESP_ERR_TIMEOUT means still busy (not an error)
    return (err == ESP_OK);
}

//=============================================================================
// ISR Callback Registration
//=============================================================================

bool ParlioPeripheralESPImpl::registerTxDoneCallback(void* callback, void* user_ctx) {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot register callback - not initialized");
        return false;
    }

    // Setup callback structure
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = reinterpret_cast<parlio_tx_done_callback_t>(callback);

    // Delegate to ESP-IDF
    esp_err_t err = parlio_tx_unit_register_event_callbacks(mTxUnit, &callbacks, user_ctx);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to register callbacks: " << err);
        return false;
    }

    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

uint8_t* ParlioPeripheralESPImpl::allocateDmaBuffer(size_t size) {
    // Round up to 64-byte multiple for cache line alignment
    size_t aligned_size = ((size + 63) / 64) * 64;

    // Allocate DMA-capable memory with 64-byte alignment
    // MALLOC_CAP_DMA ensures buffer is accessible by PARLIO peripheral
    uint8_t* buffer = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(64, aligned_size, MALLOC_CAP_DMA)
    );

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralESP: Failed to allocate DMA buffer (" << aligned_size << " bytes)");
    }

    return buffer;
}

void ParlioPeripheralESPImpl::freeDmaBuffer(uint8_t* buffer) {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

void ParlioPeripheralESPImpl::delay(uint32_t ms) {
    // Map to FreeRTOS vTaskDelay
    // pdMS_TO_TICKS converts milliseconds to RTOS ticks
    vTaskDelay(pdMS_TO_TICKS(ms));
}

//=============================================================================
// Task Management (REMOVED - Use fl::TaskCoroutine directly)
//=============================================================================
// Task methods removed. Use fl::TaskCoroutine directly from engine code.

//=============================================================================
// Timer Management (REMOVED - Use fl/isr.h directly)
//=============================================================================
// Timer methods removed. Use fl::isr::attachTimerHandler() and related
// functions from fl/isr.h instead.

uint64_t ParlioPeripheralESPImpl::getMicroseconds() {
    return esp_timer_get_time();
}

void ParlioPeripheralESPImpl::freeDmaBuffer(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
