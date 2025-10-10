// esp32c3_isr_platform.cpp — ESP32-C3 platform ISR and timer setup

#if defined(ESP32)

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(ARDUINO_ESP32C3_DEV) || defined(ARDUINO_ESP32C2_DEV)

#include "fl/compiler_control.h"

FL_EXTERN_C_BEGIN

#include "esp_intr_alloc.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "hal/timer_ll.h"

FL_EXTERN_C_END

#include "fl_parallel_spi_isr_rv.h"

#define PARALLEL_SPI_TAG "parallel_spi_c3"

// Static handles
static gptimer_handle_t s_timer = nullptr;
static intr_handle_t s_intr_handle = nullptr;

/**
 * Timer alarm callback - calls the ISR
 * This runs in ISR context and must be IRAM-safe
 */
static bool timer_alarm_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    (void)timer;
    (void)edata;
    (void)user_ctx;

    // Call the actual SPI ISR
    fl_parallel_spi_isr();

    return false;  // Don't yield from ISR
}

/**
 * Platform-specific ISR setup for ESP32-C3
 * @param timer_hz Timer frequency in Hz (should be 2× target SPI bit rate)
 * @return 0 on success, ESP error code on failure
 */
FL_EXTERN_C int fl_spi_platform_isr_start(uint32_t timer_hz) {
    if (s_timer != nullptr) {
        ESP_LOGW(PARALLEL_SPI_TAG, "Timer already initialized");
        return -1;
    }

    // Create general purpose timer
    gptimer_config_t timer_config = {};
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_config.direction = GPTIMER_COUNT_UP;
    timer_config.resolution_hz = 1000000;  // 1MHz resolution (1us per tick)

    esp_err_t ret = gptimer_new_timer(&timer_config, &s_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to create timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Calculate alarm period from frequency
    // Period in microseconds = 1,000,000 / timer_hz
    uint64_t alarm_period_us = 1000000ULL / timer_hz;

    ESP_LOGI(PARALLEL_SPI_TAG, "Timer config: %lu Hz → %llu us period", timer_hz, alarm_period_us);

    // Configure alarm
    gptimer_alarm_config_t alarm_config = {};
    alarm_config.reload_count = 0;
    alarm_config.alarm_count = alarm_period_us;
    alarm_config.flags.auto_reload_on_alarm = true;

    ret = gptimer_set_alarm_action(s_timer, &alarm_config);
    if (ret != ESP_OK) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to set alarm: %s", esp_err_to_name(ret));
        gptimer_del_timer(s_timer);
        s_timer = nullptr;
        return ret;
    }

    // Register event callbacks
    gptimer_event_callbacks_t cbs = {};
    cbs.on_alarm = timer_alarm_callback;

    ret = gptimer_register_event_callbacks(s_timer, &cbs, nullptr);
    if (ret != ESP_OK) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to register callbacks: %s", esp_err_to_name(ret));
        gptimer_del_timer(s_timer);
        s_timer = nullptr;
        return ret;
    }

    // Enable timer
    ret = gptimer_enable(s_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to enable timer: %s", esp_err_to_name(ret));
        gptimer_del_timer(s_timer);
        s_timer = nullptr;
        return ret;
    }

    // Start timer
    ret = gptimer_start(s_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to start timer: %s", esp_err_to_name(ret));
        gptimer_disable(s_timer);
        gptimer_del_timer(s_timer);
        s_timer = nullptr;
        return ret;
    }

    ESP_LOGI(PARALLEL_SPI_TAG, "Timer started at %lu Hz (Level 3 IRAM-safe)", timer_hz);
    return 0;
}

/**
 * Stop ISR and timer
 */
FL_EXTERN_C void fl_spi_platform_isr_stop(void) {
    if (s_timer != nullptr) {
        gptimer_stop(s_timer);
        gptimer_disable(s_timer);
        gptimer_del_timer(s_timer);
        s_timer = nullptr;
        ESP_LOGI(PARALLEL_SPI_TAG, "Timer stopped");
    }

    if (s_intr_handle != nullptr) {
        esp_intr_free(s_intr_handle);
        s_intr_handle = nullptr;
    }
}

#endif // CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2
#endif // ESP32
