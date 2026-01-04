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
#include "driver/parlio_tx.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
FL_EXTERN_C_END

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

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_PARLIO
#endif // ESP32
