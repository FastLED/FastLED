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

namespace fl {
namespace detail {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ParlioPeripheralESP::ParlioPeripheralESP()
    : mTxUnit(nullptr),
      mEnabled(false) {
}

ParlioPeripheralESP::~ParlioPeripheralESP() {
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

bool ParlioPeripheralESP::initialize(const ParlioPeripheralConfig& config) {
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

    return true;
}

bool ParlioPeripheralESP::enable() {
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

bool ParlioPeripheralESP::disable() {
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

//=============================================================================
// Transmission Methods
//=============================================================================

bool ParlioPeripheralESP::transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) {
    if (mTxUnit == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot transmit - not initialized");
        return false;
    }

    // CRITICAL: Flush CPU cache to memory before DMA reads buffer
    // This is the ONLY place cache sync happens (before DMA submission)
    // Calculate buffer size in bytes (bit_count / 8)
    size_t buffer_size = (bit_count + 7) / 8;

    // Memory barrier: Ensure all preceding writes complete before cache sync
    FL_MEMORY_BARRIER;

    // Cache sync: Writeback cache to memory for DMA access
    // May return ESP_ERR_INVALID_ARG on some platforms (non-cacheable memory), but still beneficial
    (void)esp_cache_msync(
        const_cast<void*>(reinterpret_cast<const void*>(buffer)),
        buffer_size,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M);  // Cache-to-Memory writeback

    // Memory barrier: Ensure cache sync completes before DMA submission
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

bool ParlioPeripheralESP::waitAllDone(uint32_t timeout_ms) {
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

bool ParlioPeripheralESP::registerTxDoneCallback(void* callback, void* user_ctx) {
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

uint8_t* ParlioPeripheralESP::allocateDmaBuffer(size_t size) {
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

void ParlioPeripheralESP::freeDmaBuffer(uint8_t* buffer) {
    if (buffer != nullptr) {
        heap_caps_free(buffer);
    }
}

void ParlioPeripheralESP::delay(uint32_t ms) {
    // Map to FreeRTOS vTaskDelay
    // pdMS_TO_TICKS converts milliseconds to RTOS ticks
    vTaskDelay(pdMS_TO_TICKS(ms));
}

//=============================================================================
// Task Management
//=============================================================================

task_handle_t ParlioPeripheralESP::createTask(const TaskConfig& config) {
    TaskHandle_t handle = nullptr;
    BaseType_t result = xTaskCreate(
        config.task_function,
        config.name,
        config.stack_size,
        config.user_data,
        config.priority,
        &handle);

    if (result != pdPASS) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to create task '" << config.name << "'");
        return nullptr;
    }

    return static_cast<task_handle_t>(handle);
}

void ParlioPeripheralESP::deleteTask(task_handle_t task_handle) {
    if (task_handle == nullptr) {
        return;
    }

    TaskHandle_t handle = static_cast<TaskHandle_t>(task_handle);
    vTaskDelete(handle);
}

void ParlioPeripheralESP::deleteCurrentTask() {
    // Self-delete the currently executing task
    // This function does NOT return on ESP32/FreeRTOS
    vTaskDelete(NULL);
}

//=============================================================================
// Timer Management
//=============================================================================

timer_handle_t ParlioPeripheralESP::createTimer(const TimerConfig& config) {
    // Configure gptimer
    gptimer_config_t timer_config = {};
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = config.resolution_hz;
    timer_config.intr_priority = config.priority;

    // Create timer
    gptimer_handle_t timer_handle = nullptr;
    esp_err_t err = gptimer_new_timer(&timer_config, &timer_handle);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to create timer: " << err);
        return nullptr;
    }

    // Register callback if provided
    if (config.callback != nullptr) {
        gptimer_event_callbacks_t callbacks = {};
        callbacks.on_alarm = reinterpret_cast<gptimer_alarm_cb_t>(config.callback);

        err = gptimer_register_event_callbacks(timer_handle, &callbacks, config.user_data);
        if (err != ESP_OK) {
            FL_LOG_PARLIO("ParlioPeripheralESP: Failed to register timer callbacks: " << err);
            gptimer_del_timer(timer_handle);
            return nullptr;
        }
    }

    // Configure alarm
    gptimer_alarm_config_t alarm_config = {};
    alarm_config.alarm_count = config.period_us * (config.resolution_hz / 1000000);
    alarm_config.reload_count = 0;
    alarm_config.flags.auto_reload_on_alarm = config.auto_reload;

    err = gptimer_set_alarm_action(timer_handle, &alarm_config);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to set timer alarm: " << err);
        gptimer_del_timer(timer_handle);
        return nullptr;
    }

    return static_cast<timer_handle_t>(timer_handle);
}

bool ParlioPeripheralESP::enableTimer(timer_handle_t handle) {
    if (handle == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot enable null timer");
        return false;
    }

    gptimer_handle_t timer = static_cast<gptimer_handle_t>(handle);
    esp_err_t err = gptimer_enable(timer);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to enable timer: " << err);
        return false;
    }

    return true;
}

bool ParlioPeripheralESP::startTimer(timer_handle_t handle) {
    if (handle == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot start null timer");
        return false;
    }

    gptimer_handle_t timer = static_cast<gptimer_handle_t>(handle);
    esp_err_t err = gptimer_start(timer);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to start timer: " << err);
        return false;
    }

    return true;
}

bool ParlioPeripheralESP::stopTimer(timer_handle_t handle) {
    if (handle == nullptr) {
        // Stopping a null timer is a no-op (not an error)
        return true;
    }

    gptimer_handle_t timer = static_cast<gptimer_handle_t>(handle);
    esp_err_t err = gptimer_stop(timer);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to stop timer: " << err);
        return false;
    }

    return true;
}

bool ParlioPeripheralESP::disableTimer(timer_handle_t handle) {
    if (handle == nullptr) {
        FL_WARN("ParlioPeripheralESP: Cannot disable null timer");
        return false;
    }

    gptimer_handle_t timer = static_cast<gptimer_handle_t>(handle);
    esp_err_t err = gptimer_disable(timer);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to disable timer: " << err);
        return false;
    }

    return true;
}

void ParlioPeripheralESP::deleteTimer(timer_handle_t handle) {
    if (handle == nullptr) {
        return;
    }

    gptimer_handle_t timer = static_cast<gptimer_handle_t>(handle);
    esp_err_t err = gptimer_del_timer(timer);
    if (err != ESP_OK) {
        FL_LOG_PARLIO("ParlioPeripheralESP: Failed to delete timer: " << err);
    }
}

uint64_t ParlioPeripheralESP::getMicroseconds() {
    return esp_timer_get_time();
}

void ParlioPeripheralESP::freeDmaBuffer(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
