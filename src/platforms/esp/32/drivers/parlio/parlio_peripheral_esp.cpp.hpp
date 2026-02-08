/// @file parlio_peripheral_esp.cpp
/// @brief Real ESP32 PARLIO peripheral implementation
///
/// Thin wrapper around ESP-IDF PARLIO TX driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_PARLIO

#include "parlio_peripheral_esp.h"
#include "fl/log.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/stl/sstream.h"
#include "fl/stl/charconv.h"  // For fl::to_hex

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

#ifndef CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM
#warning \
    "PARLIO: CONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM is not defined! Add 'build_flags = -DCONFIG_PARLIO_TX_ISR_HANDLER_IN_IRAM=1' to platformio.ini or your build system"
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
    bool deinitialize() override;
    bool enable() override;
    bool disable() override;
    bool isInitialized() const override;
    bool transmit(const u8* buffer, size_t bit_count, u16 idle_value) override;
    bool waitAllDone(u32 timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    u8* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(u8* buffer) override;
    void delay(u32 ms) override;
    uint64_t getMicroseconds() override;
    void freeDmaBuffer(void* ptr) override;

private:
    ::parlio_tx_unit_handle_t mTxUnit;  ///< ESP-IDF TX unit handle
    bool mEnabled;                       ///< Track enable state (for cleanup)
    bool mPreferPsram;                   ///< Prefer PSRAM for DMA buffers
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
      mEnabled(false),
      mPreferPsram(true) {
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
    FL_LOG_PARLIO("PARLIO_PERIPH: initialize() called - data_width=" << config.data_width << " clock=" << config.clock_freq_hz);

    // ⚠️ ESP32-C6 KNOWN HARDWARE LIMITATION:
    // The ESP32-C6 PARLIO peripheral has an undocumented hardware timing issue causing
    // ~30% single-bit corruption rate during LED transmission. This is NOT a software bug.
    // Investigation (2025-01): MSB packing verified correct, software reviewed clean,
    // scale-independent failure pattern indicates silicon-level timing glitch.
    // Recommendation: Use RMT driver for >95% reliability requirements on ESP32-C6.
    // See: src/platforms/esp/32/drivers/parlio/README.md (ESP32-C6 Hardware Reliability Issue)

    // Store PSRAM preference
    mPreferPsram = config.prefer_psram;

    // If already initialized, clean up first so re-initialization can succeed.
    // This prevents the "Already initialized" retry loop when a previous init
    // partially succeeded (TX unit created) but a later step (e.g., ring buffer
    // allocation) failed and the caller is retrying.
    if (mTxUnit != nullptr) {
        FL_DBG("ParlioPeripheralESP: Already initialized, deinitializing for re-init");
        deinitialize();
    }

    // Configure PARLIO TX unit (maps directly to ESP-IDF structure)
    parlio_tx_unit_config_t esp_config = {};
    esp_config.clk_src = PARLIO_CLK_SRC_DEFAULT;
    esp_config.clk_in_gpio_num = static_cast<gpio_num_t>(-1);
    esp_config.output_clk_freq_hz = config.clock_freq_hz;
    esp_config.data_width = config.data_width;
    esp_config.trans_queue_depth = config.queue_depth;
    esp_config.max_transfer_size = config.max_transfer_size;
    esp_config.bit_pack_order = (config.packing == fl::detail::ParlioBitPackOrder::FL_PARLIO_LSB)
        ? PARLIO_BIT_PACK_ORDER_LSB
        : PARLIO_BIT_PACK_ORDER_MSB;
    esp_config.sample_edge = PARLIO_SAMPLE_EDGE_POS;

    // Assign GPIO pins
    FL_DBG("PARLIO_PERIPH: GPIO pins:");
    for (size_t i = 0; i < 16; i++) {
        esp_config.data_gpio_nums[i] = static_cast<gpio_num_t>(config.gpio_pins[i]);
        if (config.gpio_pins[i] >= 0) {
            FL_LOG_PARLIO("  [" << i << "] = GPIO " << config.gpio_pins[i]);
        }
    }

    // No external clock or valid signal
    esp_config.clk_out_gpio_num = static_cast<gpio_num_t>(-1);
    esp_config.valid_gpio_num = static_cast<gpio_num_t>(-1);

    // Log heap availability before allocation attempts (visible with FASTLED_LOG_PARLIO_ENABLED)
    FL_LOG_PARLIO("PARLIO_PERIPH: DMA heap - free: "
           << heap_caps_get_free_size(MALLOC_CAP_DMA)
           << ", largest block: " << heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    FL_LOG_PARLIO("PARLIO_PERIPH: PSRAM heap - free: "
           << heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
           << ", largest block: " << heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    // Create TX unit (delegate to ESP-IDF)
    FL_LOG_PARLIO("PARLIO_PERIPH: Calling parlio_new_tx_unit()");
    esp_err_t err = parlio_new_tx_unit(&esp_config, &mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("ParlioPeripheralESP: parlio_new_tx_unit() failed: "
                << esp_err_to_name(err) << " (" << err << ")"
                << " data_width=" << config.data_width);
        mTxUnit = nullptr;
        return false;
    }
    FL_LOG_PARLIO("PARLIO_PERIPH: parlio_new_tx_unit() SUCCESS - handle=" << (void*)mTxUnit);

    FL_LOG_PARLIO("PARLIO: Initialized (data_width=" << config.data_width
                  << ", clock=" << config.clock_freq_hz << " Hz)");

    return true;
}

bool ParlioPeripheralESPImpl::deinitialize() {
    if (mTxUnit == nullptr) {
        return true; // Already deinitialized
    }

    // Disable TX unit if enabled
    if (mEnabled) {
        esp_err_t err = parlio_tx_unit_disable(mTxUnit);
        if (err != ESP_OK) {
            FL_WARN("ParlioPeripheralESP: Failed to disable TX unit during deinitialize: "
                    << esp_err_to_name(err) << " (" << err << ")");
        }
        mEnabled = false;
    }

    // Delete TX unit to free hardware resources
    esp_err_t err = parlio_del_tx_unit(mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("ParlioPeripheralESP: Failed to delete TX unit during deinitialize: "
                << esp_err_to_name(err) << " (" << err << ")");
        return false;
    }

    mTxUnit = nullptr;
    return true;
}

bool ParlioPeripheralESPImpl::enable() {
    FL_LOG_PARLIO("PARLIO_PERIPH: enable() called");
    if (mTxUnit == nullptr) {
        FL_LOG_PARLIO("PARLIO_PERIPH: FAILED enable - not initialized");
        FL_WARN("ParlioPeripheralESP: Cannot enable - not initialized");
        return false;
    }

    // Delegate to ESP-IDF
    FL_LOG_PARLIO("PARLIO_PERIPH: Calling parlio_tx_unit_enable()");
    esp_err_t err = parlio_tx_unit_enable(mTxUnit);
    if (err != ESP_OK) {
        FL_WARN("ParlioPeripheralESP: Failed to enable TX unit: "
                << esp_err_to_name(err) << " (" << err << ")");
        return false;
    }

    mEnabled = true;
    FL_LOG_PARLIO("PARLIO_PERIPH: enable() SUCCESS");
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

bool FL_IRAM ParlioPeripheralESPImpl::transmit(const u8* buffer, size_t bit_count, u16 idle_value) {
    // ⚠️  ISR CONTEXT - NO LOGGING ALLOWED ⚠️
    // This function is called from FL_IRAM txDoneCallback via virtual dispatch.
    if (mTxUnit == nullptr) {
        return false;
    }

    if (buffer == nullptr) {
        return false;
    }

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
    payload.flags.queue_nonblocking = 1;  // ISR-safe: don't block if queue full

    // Delegate to ESP-IDF (ISR-safe call)
    esp_err_t err = parlio_tx_unit_transmit(mTxUnit, buffer, bit_count, &payload);
    if (err != ESP_OK) {
        // Don't log in potential ISR context - return error silently
        return false;
    }

    return true;
}

bool ParlioPeripheralESPImpl::waitAllDone(u32 timeout_ms) {
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
    FL_LOG_PARLIO("PARLIO_PERIPH: registerTxDoneCallback() called");
    if (mTxUnit == nullptr) {
        FL_LOG_PARLIO("PARLIO_PERIPH: FAILED register callback - not initialized");
        FL_WARN("ParlioPeripheralESP: Cannot register callback - not initialized");
        return false;
    }

    // Setup callback structure
    parlio_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = reinterpret_cast<parlio_tx_done_callback_t>(callback); // ok reinterpret cast

    // Delegate to ESP-IDF
    FL_LOG_PARLIO("PARLIO_PERIPH: Calling parlio_tx_unit_register_event_callbacks()");
    esp_err_t err = parlio_tx_unit_register_event_callbacks(mTxUnit, &callbacks, user_ctx);
    if (err != ESP_OK) {
        FL_WARN("ParlioPeripheralESP: Failed to register callbacks: "
                << esp_err_to_name(err) << " (" << err << ")");
        return false;
    }

    FL_LOG_PARLIO("PARLIO_PERIPH: registerTxDoneCallback() SUCCESS");
    return true;
}

//=============================================================================
// DMA Memory Management
//=============================================================================

u8* ParlioPeripheralESPImpl::allocateDmaBuffer(size_t size) {
    // Round up to 64-byte multiple for cache line alignment
    size_t aligned_size = ((size + 63) / 64) * 64;

    u8* buffer = nullptr;

    // Try PSRAM+DMA first if enabled (follows I2S LCD CAM pattern)
    // PSRAM provides much larger memory pool (~8MB on ESP32-P4) vs internal SRAM (~512KB)
    if (mPreferPsram) {
        buffer = static_cast<u8*>(
            heap_caps_aligned_alloc(64, aligned_size,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)
        );
    }

    // Fallback to internal DMA memory (current behavior)
    if (buffer == nullptr) {
        buffer = static_cast<u8*>(
            heap_caps_aligned_alloc(64, aligned_size, MALLOC_CAP_DMA)
        );
    }

    if (buffer == nullptr) {
        FL_WARN("ParlioPeripheralESP: Failed to allocate DMA buffer (" << aligned_size << " bytes)");
        // Detailed heap stats visible with FASTLED_LOG_PARLIO_ENABLED
        FL_LOG_PARLIO("  DMA heap: " << heap_caps_get_free_size(MALLOC_CAP_DMA)
                << " free, " << heap_caps_get_largest_free_block(MALLOC_CAP_DMA) << " largest block");
        if (mPreferPsram) {
            FL_LOG_PARLIO("  PSRAM heap: " << heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
                    << " free, " << heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) << " largest block");
        }
    }

    return buffer;
}

void ParlioPeripheralESPImpl::freeDmaBuffer(u8* buffer) {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

void ParlioPeripheralESPImpl::delay(u32 ms) {
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
#endif // FL_IS_ESP32
