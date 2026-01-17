/*
  FastLED — ESP32 ISR Implementation (ESP-IDF 3.x)
  ------------------------------------------------
  ESP32-specific implementation of the cross-platform ISR API using legacy timer API.
  Supports ESP32 on ESP-IDF 3.3 and later 3.x versions.

  This file is included by isr_esp32.hpp when ESP_IDF_VERSION < 4.0.0

  Note: This implementation uses the legacy timer API (driver/timer.h) with
  timer_isr_register() instead of the timer_isr_callback_add() introduced in IDF 4.x.

  License: MIT (FastLED)
*/

#pragma once

// This file should only be included from isr_esp32.hpp (which does the version check)
// Do not include this file directly - use isr_esp32.hpp instead

#include "fl/isr.h"
#include "fl/compiler_control.h"

FL_EXTERN_C_BEGIN
#include "esp_intr_alloc.h"
#include "driver/timer.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

#include "fl/stl/assert.h"

namespace fl {
namespace isr {
namespace platform {

// Spinlock for protecting GPIO ISR service installation (multi-core safety)
static portMUX_TYPE gpio_isr_service_mutex_idf3 = portMUX_INITIALIZER_UNLOCKED;

// Timer allocation tracking
// ESP32 has 2 timer groups with 2 timers each = 4 timers total
static bool timer_allocated[2][2] = {{false, false}, {false, false}};
static portMUX_TYPE timer_alloc_mutex = portMUX_INITIALIZER_UNLOCKED;

// =============================================================================
// Platform-Specific Handle Storage
// =============================================================================

struct esp32_idf3_isr_handle_data {
    timer_group_t timer_group;       // Timer group (0 or 1)
    timer_idx_t timer_idx;           // Timer index within group (0 or 1)
    bool is_timer;                   // true = timer ISR, false = external ISR
    bool is_enabled;                 // Current enable state
    isr_handler_t user_handler;      // User handler function
    void* user_data;                 // User context
    uint8_t gpio_pin;                // GPIO pin number (0xFF if not GPIO)
    intr_handle_t intr_handle;       // ESP-IDF interrupt handle (for timer ISR)

    esp32_idf3_isr_handle_data()
        : timer_group(TIMER_GROUP_0)
        , timer_idx(TIMER_0)
        , is_timer(false)
        , is_enabled(true)
        , user_handler(nullptr)
        , user_data(nullptr)
        , gpio_pin(0xFF)
        , intr_handle(nullptr)
    {}
};

// Platform ID for ESP32 IDF3
constexpr uint8_t ESP32_IDF3_PLATFORM_ID = 1;

#define ESP32_IDF3_ISR_TAG "fl_isr_esp32_idf3"

// =============================================================================
// Timer Allocation Helper
// =============================================================================

/**
 * Allocate an available timer
 * Returns true if a timer was successfully allocated
 */
static bool allocate_timer(timer_group_t* out_group, timer_idx_t* out_idx) {
    taskENTER_CRITICAL(&timer_alloc_mutex);
    for (int g = 0; g < 2; g++) {
        for (int t = 0; t < 2; t++) {
            if (!timer_allocated[g][t]) {
                timer_allocated[g][t] = true;
                *out_group = static_cast<timer_group_t>(g);
                *out_idx = static_cast<timer_idx_t>(t);
                taskEXIT_CRITICAL(&timer_alloc_mutex);
                return true;
            }
        }
    }
    taskEXIT_CRITICAL(&timer_alloc_mutex);
    return false;
}

/**
 * Free a previously allocated timer
 */
static void free_timer(timer_group_t group, timer_idx_t idx) {
    taskENTER_CRITICAL(&timer_alloc_mutex);
    timer_allocated[group][idx] = false;
    taskEXIT_CRITICAL(&timer_alloc_mutex);
}

// =============================================================================
// Timer ISR Wrapper
// =============================================================================

/**
 * Timer ISR handler - calls user handler
 * This runs in ISR context and must be IRAM-safe
 *
 * Note: ESP-IDF 3.x timer ISR signature is void(*)(void*)
 */
static void FL_IRAM timer_isr_wrapper_idf3(void* user_ctx)
{
    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(user_ctx);
    if (handle_data && handle_data->user_handler) {
        // Clear interrupt status
        // In IDF 3.x, we need to manually clear the interrupt
        if (handle_data->timer_group == TIMER_GROUP_0) {
            if (handle_data->timer_idx == TIMER_0) {
                TIMERG0.int_clr_timers.t0 = 1;
            } else {
                TIMERG0.int_clr_timers.t1 = 1;
            }
        } else {
            if (handle_data->timer_idx == TIMER_0) {
                TIMERG1.int_clr_timers.t0 = 1;
            } else {
                TIMERG1.int_clr_timers.t1 = 1;
            }
        }

        // Call user handler
        handle_data->user_handler(handle_data->user_data);

        // Re-enable alarm for continuous operation (if auto-reload is disabled, timer stops after alarm)
        // The auto-reload setting handles this automatically
    }
}

// =============================================================================
// GPIO Interrupt Wrapper
// =============================================================================

/**
 * GPIO interrupt handler - calls user handler
 * This runs in ISR context
 */
static void FL_IRAM gpio_isr_wrapper_idf3(void* arg)
{
    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(arg);
    if (handle_data && handle_data->user_handler) {
        handle_data->user_handler(handle_data->user_data);
    }
}

// =============================================================================
// ESP32 IDF3 ISR Implementation (fl::platform namespace)
// =============================================================================

inline int attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: handler is null");
        return -1;  // Invalid parameter
    }

    if (config.frequency_hz == 0) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: frequency_hz is 0");
        return -2;  // Invalid frequency
    }

    // Allocate handle data
    esp32_idf3_isr_handle_data* handle_data = new esp32_idf3_isr_handle_data();
    if (!handle_data) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: failed to allocate handle data");
        return -3;  // Out of memory
    }

    // Allocate a timer
    if (!allocate_timer(&handle_data->timer_group, &handle_data->timer_idx)) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: no free timers available");
        delete handle_data;
        return -4;  // No timers available
    }

    handle_data->is_timer = true;
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Calculate timer divider and alarm value
    // ESP32 APB clock is typically 80MHz
    // Timer clock = APB_CLK / divider
    // We want: alarm_value = timer_clock / frequency_hz
    //
    // For flexibility, use a divider that gives good resolution
    // Divider range: 2 to 65536
    // Using divider=80 gives 1MHz timer clock (1us resolution)
    // Using divider=8 gives 10MHz timer clock (0.1us resolution)

    uint16_t divider;
    uint64_t alarm_value;

    if (config.frequency_hz > 1000000) {
        // For high frequencies, use smaller divider for better resolution
        divider = 8;  // 10MHz timer clock
        uint32_t timer_clock = 80000000 / divider;  // 10MHz
        alarm_value = timer_clock / config.frequency_hz;
    } else {
        // For lower frequencies, use divider=80 for 1MHz (1us resolution)
        divider = 80;  // 1MHz timer clock
        uint32_t timer_clock = 80000000 / divider;  // 1MHz
        alarm_value = timer_clock / config.frequency_hz;
    }

    // Ensure alarm_value is at least 1
    if (alarm_value == 0) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: frequency too high (%lu Hz)",
                 (unsigned long)config.frequency_hz);
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -2;  // Invalid frequency
    }

    // Configure timer
    timer_config_t timer_config = {};
    timer_config.divider = divider;
    timer_config.counter_dir = TIMER_COUNT_UP;
    timer_config.counter_en = TIMER_PAUSE;
    timer_config.alarm_en = TIMER_ALARM_EN;
    timer_config.auto_reload = (config.flags & isr::ISR_FLAG_ONE_SHOT) ? TIMER_AUTORELOAD_DIS : TIMER_AUTORELOAD_EN;
    timer_config.intr_type = TIMER_INTR_LEVEL;

    esp_err_t ret = timer_init(handle_data->timer_group, handle_data->timer_idx, &timer_config);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_init failed: %s", esp_err_to_name(ret));
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -4;  // Timer init failed
    }

    // Set counter value to 0
    ret = timer_set_counter_value(handle_data->timer_group, handle_data->timer_idx, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_set_counter_value failed: %s", esp_err_to_name(ret));
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -5;  // Counter value set failed
    }

    // Set alarm value
    ret = timer_set_alarm_value(handle_data->timer_group, handle_data->timer_idx, alarm_value);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_set_alarm_value failed: %s", esp_err_to_name(ret));
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -5;  // Alarm value set failed
    }

    // Enable timer interrupt
    ret = timer_enable_intr(handle_data->timer_group, handle_data->timer_idx);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_enable_intr failed: %s", esp_err_to_name(ret));
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -6;  // Enable intr failed
    }

    // Register ISR handler using IDF 3.x API
    int intr_flags = (config.flags & isr::ISR_FLAG_IRAM_SAFE) ? ESP_INTR_FLAG_IRAM : 0;
    ret = timer_isr_register(handle_data->timer_group, handle_data->timer_idx,
                             timer_isr_wrapper_idf3, handle_data, intr_flags, &handle_data->intr_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_isr_register failed: %s", esp_err_to_name(ret));
        timer_disable_intr(handle_data->timer_group, handle_data->timer_idx);
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -6;  // ISR registration failed
    }

    // Start timer
    ret = timer_start(handle_data->timer_group, handle_data->timer_idx);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachTimerHandler: timer_start failed: %s", esp_err_to_name(ret));
        esp_intr_free(handle_data->intr_handle);
        timer_disable_intr(handle_data->timer_group, handle_data->timer_idx);
        free_timer(handle_data->timer_group, handle_data->timer_idx);
        delete handle_data;
        return -8;  // Timer start failed
    }

    ESP_LOGD(ESP32_IDF3_ISR_TAG, "Timer started: group=%d, idx=%d, freq=%lu Hz, alarm=%llu",
             handle_data->timer_group, handle_data->timer_idx,
             (unsigned long)config.frequency_hz, (unsigned long long)alarm_value);

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = ESP32_IDF3_PLATFORM_ID;
    }

    return 0;  // Success
}

inline int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachExternalHandler: handler is null");
        return -1;  // Invalid parameter
    }

    // Allocate handle data
    esp32_idf3_isr_handle_data* handle_data = new esp32_idf3_isr_handle_data();
    if (!handle_data) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachExternalHandler: failed to allocate handle data");
        return -3;  // Out of memory
    }

    handle_data->is_timer = false;
    handle_data->user_handler = config.handler;
    handle_data->user_data = config.user_data;

    // Configure GPIO
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    // Set interrupt type based on flags
    if (config.flags & isr::ISR_FLAG_EDGE_RISING) {
        io_conf.intr_type = GPIO_INTR_POSEDGE;
    } else if (config.flags & isr::ISR_FLAG_EDGE_FALLING) {
        io_conf.intr_type = GPIO_INTR_NEGEDGE;
    } else if (config.flags & isr::ISR_FLAG_LEVEL_HIGH) {
        io_conf.intr_type = GPIO_INTR_HIGH_LEVEL;
    } else if (config.flags & isr::ISR_FLAG_LEVEL_LOW) {
        io_conf.intr_type = GPIO_INTR_LOW_LEVEL;
    } else {
        // Default to any edge
        io_conf.intr_type = GPIO_INTR_ANYEDGE;
    }

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachExternalHandler: gpio_config failed: %s", esp_err_to_name(ret));
        delete handle_data;
        return -9;  // GPIO config failed
    }

    // Install GPIO ISR service if not already installed
    // Use critical section for multi-core safety
    static bool gpio_isr_service_installed = false;
    if (!gpio_isr_service_installed) {
        taskENTER_CRITICAL(&gpio_isr_service_mutex_idf3);
        // Double-check after acquiring lock (classic double-checked locking pattern)
        if (!gpio_isr_service_installed) {
            ret = gpio_install_isr_service(0);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                taskEXIT_CRITICAL(&gpio_isr_service_mutex_idf3);
                ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachExternalHandler: gpio_install_isr_service failed: %s", esp_err_to_name(ret));
                delete handle_data;
                return -10;  // ISR service installation failed
            }
            gpio_isr_service_installed = true;
        }
        taskEXIT_CRITICAL(&gpio_isr_service_mutex_idf3);
    }

    // Add ISR handler for specific GPIO pin
    ret = gpio_isr_handler_add(static_cast<gpio_num_t>(pin), gpio_isr_wrapper_idf3, handle_data);
    if (ret != ESP_OK) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "attachExternalHandler: gpio_isr_handler_add failed: %s", esp_err_to_name(ret));
        delete handle_data;
        return -11;  // ISR handler add failed
    }

    // Store GPIO pin for cleanup
    handle_data->gpio_pin = pin;

    ESP_LOGD(ESP32_IDF3_ISR_TAG, "GPIO interrupt attached on pin %d", pin);

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = ESP32_IDF3_PLATFORM_ID;
    }

    return 0;  // Success
}

inline int detach_handler(isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != ESP32_IDF3_PLATFORM_ID) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "detachHandler: invalid handle");
        return -1;  // Invalid handle
    }

    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "detachHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        // Stop and cleanup timer
        timer_pause(handle_data->timer_group, handle_data->timer_idx);
        timer_disable_intr(handle_data->timer_group, handle_data->timer_idx);
        // Free the interrupt handle (IDF 3.x)
        if (handle_data->intr_handle) {
            esp_intr_free(handle_data->intr_handle);
        }
        free_timer(handle_data->timer_group, handle_data->timer_idx);
    } else {
        // Cleanup GPIO interrupt
        if (handle_data->gpio_pin != 0xFF) {
            gpio_isr_handler_remove(static_cast<gpio_num_t>(handle_data->gpio_pin));
        }
    }

    delete handle_data;
    handle.platform_handle = nullptr;
    handle.platform_id = 0;

    ESP_LOGD(ESP32_IDF3_ISR_TAG, "Handler detached");
    return 0;  // Success
}

inline int enable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != ESP32_IDF3_PLATFORM_ID) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "enableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "enableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        esp_err_t ret = timer_start(handle_data->timer_group, handle_data->timer_idx);
        if (ret != ESP_OK) {
            ESP_LOGW(ESP32_IDF3_ISR_TAG, "enableHandler: timer_start failed: %s", esp_err_to_name(ret));
            return -12;  // Enable failed
        }
        handle_data->is_enabled = true;
    } else if (handle_data->gpio_pin != 0xFF) {
        esp_err_t ret = gpio_intr_enable(static_cast<gpio_num_t>(handle_data->gpio_pin));
        if (ret != ESP_OK) {
            ESP_LOGW(ESP32_IDF3_ISR_TAG, "enableHandler: gpio_intr_enable failed: %s", esp_err_to_name(ret));
            return -14;
        }
        handle_data->is_enabled = true;
    }

    return 0;  // Success
}

inline int disable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != ESP32_IDF3_PLATFORM_ID) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "disableHandler: invalid handle");
        return -1;  // Invalid handle
    }

    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        ESP_LOGW(ESP32_IDF3_ISR_TAG, "disableHandler: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->is_timer) {
        esp_err_t ret = timer_pause(handle_data->timer_group, handle_data->timer_idx);
        if (ret != ESP_OK) {
            ESP_LOGW(ESP32_IDF3_ISR_TAG, "disableHandler: timer_pause failed: %s", esp_err_to_name(ret));
            return -13;  // Disable failed
        }
        handle_data->is_enabled = false;
    } else if (handle_data->gpio_pin != 0xFF) {
        esp_err_t ret = gpio_intr_disable(static_cast<gpio_num_t>(handle_data->gpio_pin));
        if (ret != ESP_OK) {
            ESP_LOGW(ESP32_IDF3_ISR_TAG, "disableHandler: gpio_intr_disable failed: %s", esp_err_to_name(ret));
            return -15;
        }
        handle_data->is_enabled = false;
    }

    return 0;  // Success
}

inline bool is_handler_enabled(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != ESP32_IDF3_PLATFORM_ID) {
        return false;
    }

    esp32_idf3_isr_handle_data* handle_data = static_cast<esp32_idf3_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        return false;
    }

    return handle_data->is_enabled;
}

inline const char* get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "Invalid parameter";
        case -2: return "Invalid frequency";
        case -3: return "Out of memory";
        case -4: return "Timer init failed / no timers available";
        case -5: return "Timer config failed";
        case -6: return "Callback registration failed";
        case -7: return "Timer enable failed";
        case -8: return "Timer start failed";
        case -9: return "GPIO config failed";
        case -10: return "ISR service installation failed";
        case -11: return "ISR handler add failed";
        case -12: return "Enable failed";
        case -13: return "Disable failed";
        case -14: return "GPIO enable failed";
        case -15: return "GPIO disable failed";
        default: return "Unknown error";
    }
}

inline const char* get_platform_name() {
    return "ESP32 (IDF3)";
}

inline uint32_t get_max_timer_frequency() {
    // With divider=8, timer clock is 10MHz
    // Minimum alarm value of 1 gives max frequency of 10MHz
    return 10000000;  // 10 MHz
}

inline uint32_t get_min_timer_frequency() {
    return 1;  // 1 Hz
}

inline uint8_t get_max_priority() {
    // Xtensa: Priority 1-3 (official), 4-5 (experimental, requires assembly)
    return 5;
}

inline bool requires_assembly_handler(uint8_t priority) {
    // Xtensa: Priority 4+ requires assembly handlers
    return priority >= 4;
}

} // namespace platform
} // namespace isr

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts)
// =============================================================================

/// Disable interrupts on ESP32
inline void interruptsDisable() {
    portDISABLE_INTERRUPTS();
}

/// Enable interrupts on ESP32
inline void interruptsEnable() {
    portENABLE_INTERRUPTS();
}

} // namespace fl
