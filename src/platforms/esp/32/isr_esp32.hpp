/*
  FastLED — ESP32 ISR Implementation
  -----------------------------------
  ESP32-specific implementation of the cross-platform ISR API.
  Supports ESP32, ESP32-S2, ESP32-S3 (Xtensa) and ESP32-C3, ESP32-C6 (RISC-V).

  Note: This implementation requires ESP-IDF 5.0+ for the gptimer API.
        On older ESP-IDF versions, the null ISR implementation is used instead.

  License: MIT (FastLED)
*/

#pragma once

#if defined(ESP32)

#include "fl/isr.h"
#include "fl/compiler_control.h"
// Include ESP-IDF headers to get version macros
FL_EXTERN_C_BEGIN
#include "esp_attr.h"
FL_EXTERN_C_END

// Only compile this implementation for ESP-IDF 5.0+
// The gptimer API is not available in ESP-IDF 4.x
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

FL_EXTERN_C_BEGIN
#include "esp_intr_alloc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "driver/gpio.h"
FL_EXTERN_C_END

#include "fl/stl/assert.h"
#include "fl/dbg.h"

namespace fl {
namespace isr {
namespace platform {

// =============================================================================
// Platform-Specific Handle Storage
// =============================================================================

struct esp32_isr_handle_data {
    gptimer_handle_t timer_handle;   // For timer-based ISRs
    intr_handle_t intr_handle;       // For external/GPIO interrupts
    bool is_timer;                   // true = timer ISR, false = external ISR
    bool is_enabled;                 // Current enable state
    isr_handler_t user_handler;      // User handler function
    void* user_data;                 // User context
    uint8_t gpio_pin;                // GPIO pin number (0xFF if not GPIO)

    esp32_isr_handle_data()
        : timer_handle(nullptr)
        , intr_handle(nullptr)
        , is_timer(false)
        , is_enabled(true)
        , user_handler(nullptr)
        , user_data(nullptr)
        , gpio_pin(0xFF)
    {}
};

// Platform ID for ESP32
constexpr uint8_t ESP32_PLATFORM_ID = 1;

#define ESP32_ISR_TAG "fl_isr_esp32"

// =============================================================================
// Timer Callback Wrapper
// =============================================================================

/**
 * Timer alarm callback - calls user handler
 * This runs in ISR context and must be IRAM-safe
 */
static bool FL_IRAM timer_alarm_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t* edata,
    void* user_ctx)
{
    (void)timer;
    (void)edata;

    esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(user_ctx);
    if (handle_data && handle_data->user_handler) {
        handle_data->user_handler(handle_data->user_data);
    }

    return false;  // Don't yield from ISR
}

// =============================================================================
// GPIO Interrupt Wrapper
// =============================================================================

/**
 * GPIO interrupt handler - calls user handler
 * This runs in ISR context
 */
static void FL_IRAM gpio_isr_wrapper(void* arg)
{
    esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(arg);
    if (handle_data && handle_data->user_handler) {
        handle_data->user_handler(handle_data->user_data);
    }
}

// =============================================================================
// ESP32 ISR Implementation (fl::platform namespace)
// =============================================================================

int attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
        if (!config.handler) {
            FL_WARN("attachTimerHandler: handler is null");
            return -1;  // Invalid parameter
        }

        if (config.frequency_hz == 0) {
            FL_WARN("attachTimerHandler: frequency_hz is 0");
            return -2;  // Invalid frequency
        }

        // Allocate handle data
        esp32_isr_handle_data* handle_data = new esp32_isr_handle_data();
        if (!handle_data) {
            FL_WARN("attachTimerHandler: failed to allocate handle data");
            return -3;  // Out of memory
        }

        handle_data->is_timer = true;
        handle_data->user_handler = config.handler;
        handle_data->user_data = config.user_data;

        // Create general purpose timer
        // For high frequencies (>1MHz), we need higher resolution to avoid rounding to 0
        // Choose resolution dynamically based on requested frequency
        //
        // IMPORTANT: ESP32 timer hardware requires clock divider >= 2
        // With 80MHz source clock, max resolution is 40MHz (80MHz / 2 = 40MHz)
        uint32_t timer_resolution_hz;
        uint64_t alarm_count;

        if (config.frequency_hz > 1000000) {
            // For frequencies > 1MHz, use higher resolution
            // Cap at 40MHz to ensure divider >= 2 (80MHz / 40MHz = 2)
            timer_resolution_hz = 40000000;
            alarm_count = timer_resolution_hz / config.frequency_hz;
        } else {
            // For lower frequencies, 1MHz resolution is sufficient
            timer_resolution_hz = 1000000;
            alarm_count = timer_resolution_hz / config.frequency_hz;
        }

        // Ensure alarm_count is at least 1 to avoid ESP_ERR_INVALID_ARG
        if (alarm_count == 0) {
            FL_WARN("attachTimerHandler: frequency too high (" << config.frequency_hz
                    << " Hz), maximum is " << timer_resolution_hz << " Hz");
            delete handle_data;
            return -2;  // Invalid frequency
        }

        gptimer_config_t timer_config = {};
        timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
        timer_config.direction = GPTIMER_COUNT_UP;
        timer_config.resolution_hz = timer_resolution_hz;

        esp_err_t ret = gptimer_new_timer(&timer_config, &handle_data->timer_handle);
        if (ret != ESP_OK) {
            FL_WARN("attachTimerHandler: gptimer_new_timer failed: " << esp_err_to_name(ret));
            delete handle_data;
            return -4;  // Timer creation failed
        }

        FL_DBG("Timer config: " << config.frequency_hz << " Hz using "
               << timer_resolution_hz << " Hz resolution → " << alarm_count << " ticks");

        // Configure alarm
        gptimer_alarm_config_t alarm_config = {};
        alarm_config.reload_count = 0;
        alarm_config.alarm_count = alarm_count;
        alarm_config.flags.auto_reload_on_alarm = !(config.flags & isr::ISR_FLAG_ONE_SHOT);

        ret = gptimer_set_alarm_action(handle_data->timer_handle, &alarm_config);
        if (ret != ESP_OK) {
            FL_WARN("attachTimerHandler: gptimer_set_alarm_action failed: " << esp_err_to_name(ret));
            gptimer_del_timer(handle_data->timer_handle);
            delete handle_data;
            return -5;  // Alarm config failed
        }

        // Register event callbacks
        gptimer_event_callbacks_t cbs = {};
        cbs.on_alarm = timer_alarm_callback;

        ret = gptimer_register_event_callbacks(handle_data->timer_handle, &cbs, handle_data);
        if (ret != ESP_OK) {
            FL_WARN("attachTimerHandler: gptimer_register_event_callbacks failed: " << esp_err_to_name(ret));
            gptimer_del_timer(handle_data->timer_handle);
            delete handle_data;
            return -6;  // Callback registration failed
        }

        // Enable timer
        ret = gptimer_enable(handle_data->timer_handle);
        if (ret != ESP_OK) {
            FL_WARN("attachTimerHandler: gptimer_enable failed: " << esp_err_to_name(ret));
            gptimer_del_timer(handle_data->timer_handle);
            delete handle_data;
            return -7;  // Timer enable failed
        }

        // Start timer
        ret = gptimer_start(handle_data->timer_handle);
        if (ret != ESP_OK) {
            FL_WARN("attachTimerHandler: gptimer_start failed: " << esp_err_to_name(ret));
            gptimer_disable(handle_data->timer_handle);
            gptimer_del_timer(handle_data->timer_handle);
            delete handle_data;
            return -8;  // Timer start failed
        }

        FL_DBG("Timer started at " << config.frequency_hz << " Hz");

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = ESP32_PLATFORM_ID;
        }

        return 0;  // Success
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
        if (!config.handler) {
            FL_WARN("attachExternalHandler: handler is null");
            return -1;  // Invalid parameter
        }

        // Allocate handle data
        esp32_isr_handle_data* handle_data = new esp32_isr_handle_data();
        if (!handle_data) {
            FL_WARN("attachExternalHandler: failed to allocate handle data");
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
            FL_WARN("attachExternalHandler: gpio_config failed: " << esp_err_to_name(ret));
            delete handle_data;
            return -9;  // GPIO config failed
        }

        // Install GPIO ISR service if not already installed
        static bool gpio_isr_service_installed = false;
        if (!gpio_isr_service_installed) {
            ret = gpio_install_isr_service(0);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                FL_WARN("attachExternalHandler: gpio_install_isr_service failed: " << esp_err_to_name(ret));
                delete handle_data;
                return -10;  // ISR service installation failed
            }
            gpio_isr_service_installed = true;
        }

        // Add ISR handler for specific GPIO pin
        ret = gpio_isr_handler_add(static_cast<gpio_num_t>(pin), gpio_isr_wrapper, handle_data);
        if (ret != ESP_OK) {
            FL_WARN("attachExternalHandler: gpio_isr_handler_add failed: " << esp_err_to_name(ret));
            delete handle_data;
            return -11;  // ISR handler add failed
        }

        // Store GPIO pin for cleanup
        handle_data->gpio_pin = pin;

        FL_DBG("GPIO interrupt attached on pin " << pin);

        // Populate output handle
        if (out_handle) {
            out_handle->platform_handle = handle_data;
            out_handle->handler = config.handler;
            out_handle->user_data = config.user_data;
            out_handle->platform_id = ESP32_PLATFORM_ID;
        }

        return 0;  // Success
}

int detach_handler(isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != ESP32_PLATFORM_ID) {
            FL_WARN("detachHandler: invalid handle");
            return -1;  // Invalid handle
        }

        esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            FL_WARN("detachHandler: null handle data");
            return -1;  // Invalid handle
        }

        esp_err_t ret = ESP_OK;

        if (handle_data->is_timer) {
            // Stop and cleanup timer
            if (handle_data->timer_handle) {
                gptimer_stop(handle_data->timer_handle);
                gptimer_disable(handle_data->timer_handle);
                ret = gptimer_del_timer(handle_data->timer_handle);
                if (ret != ESP_OK) {
                    FL_WARN("detachHandler: gptimer_del_timer failed: " << esp_err_to_name(ret));
                }
            }
        } else {
            // Cleanup GPIO interrupt
            if (handle_data->gpio_pin != 0xFF) {
                gpio_isr_handler_remove(static_cast<gpio_num_t>(handle_data->gpio_pin));
            }
        }

        delete handle_data;
        handle.platform_handle = nullptr;
        handle.platform_id = 0;

        FL_DBG("Handler detached");
        return 0;  // Success
}

int enable_handler(const isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != ESP32_PLATFORM_ID) {
            FL_WARN("enableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            FL_WARN("enableHandler: null handle data");
            return -1;  // Invalid handle
        }

        if (handle_data->is_timer && handle_data->timer_handle) {
            esp_err_t ret = gptimer_start(handle_data->timer_handle);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                FL_WARN("enableHandler: gptimer_start failed: " << esp_err_to_name(ret));
                return -12;  // Enable failed
            }
            handle_data->is_enabled = true;
        } else if (!handle_data->is_timer && handle_data->gpio_pin != 0xFF) {
            esp_err_t ret = gpio_intr_enable(static_cast<gpio_num_t>(handle_data->gpio_pin));
            if (ret != ESP_OK) {
                FL_WARN("enableHandler: gpio_intr_enable failed: " << esp_err_to_name(ret));
                return -14;
            }
            handle_data->is_enabled = true;
        }

        return 0;  // Success
}

int disable_handler(const isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != ESP32_PLATFORM_ID) {
            FL_WARN("disableHandler: invalid handle");
            return -1;  // Invalid handle
        }

        esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            FL_WARN("disableHandler: null handle data");
            return -1;  // Invalid handle
        }

        if (handle_data->is_timer && handle_data->timer_handle) {
            esp_err_t ret = gptimer_stop(handle_data->timer_handle);
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                FL_WARN("disableHandler: gptimer_stop failed: " << esp_err_to_name(ret));
                return -13;  // Disable failed
            }
            handle_data->is_enabled = false;
        } else if (!handle_data->is_timer && handle_data->gpio_pin != 0xFF) {
            esp_err_t ret = gpio_intr_disable(static_cast<gpio_num_t>(handle_data->gpio_pin));
            if (ret != ESP_OK) {
                FL_WARN("disableHandler: gpio_intr_disable failed: " << esp_err_to_name(ret));
                return -15;
            }
            handle_data->is_enabled = false;
        }

        return 0;  // Success
}

bool is_handler_enabled(const isr_handle_t& handle) {
        if (!handle.is_valid() || handle.platform_id != ESP32_PLATFORM_ID) {
            return false;
        }

        esp32_isr_handle_data* handle_data = static_cast<esp32_isr_handle_data*>(handle.platform_handle);
        if (!handle_data) {
            return false;
        }

        return handle_data->is_enabled;
}

const char* get_error_string(int error_code) {
        switch (error_code) {
            case 0: return "Success";
            case -1: return "Invalid parameter";
            case -2: return "Invalid frequency";
            case -3: return "Out of memory";
            case -4: return "Timer creation failed";
            case -5: return "Alarm config failed";
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

const char* get_platform_name() {
#if defined(CONFIG_IDF_TARGET_ESP32)
    return "ESP32";
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    return "ESP32-S2";
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    return "ESP32-S3";
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    return "ESP32-C3";
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    return "ESP32-C6";
#else
    return "ESP32 (unknown)";
#endif
}

uint32_t get_max_timer_frequency() {
    return 40000000;  // 40 MHz (limited by hardware divider >= 2 requirement)
}

uint32_t get_min_timer_frequency() {
    return 1;  // 1 Hz
}

uint8_t get_max_priority() {
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    // RISC-V: Priority 1-7 (but 4-7 may have limitations)
    return 7;
#else
    // Xtensa: Priority 1-3 (official), 4-5 (experimental, requires assembly)
    return 5;
#endif
}

bool requires_assembly_handler(uint8_t priority) {
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    // RISC-V: All priority levels can use C handlers
    return false;
#else
    // Xtensa: Priority 4+ requires assembly handlers
    return priority >= 4;
#endif
}

} // namespace platform
} // namespace isr
} // namespace fl

#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

#endif // ESP32
