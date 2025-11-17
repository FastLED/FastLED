// esp32c3_isr_platform.cpp — ESP32-C3 platform ISR and timer setup
// Refactored to use cross-platform fl::isr API

// ok no namespace fl
#if defined(ESP32)

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(ARDUINO_ESP32C3_DEV) || defined(ARDUINO_ESP32C2_DEV)

#include "fl/compiler_control.h"
#include "fl/isr.h"

FL_EXTERN_C_BEGIN

#include "esp_attr.h"
#include "esp_log.h"

FL_EXTERN_C_END

#include "platforms/shared/spi_bitbang/spi_isr_engine.h"

#define PARALLEL_SPI_TAG "parallel_spi_c3"

// ISR handle for cross-platform API
static fl::isr::isr_handle_t s_isr_handle;

/**
 * Timer alarm callback - calls the ISR
 * This runs in ISR context and must be IRAM-safe
 */
static void FL_IRAM spi_isr_wrapper(void* user_ctx) {
    (void)user_ctx;

    // Call the actual SPI ISR
    fl_parallel_spi_isr();
}

/**
 * Platform-specific ISR setup for ESP32-C3
 * @param timer_hz Timer frequency in Hz (should be 2× target SPI bit rate)
 * @return 0 on success, error code on failure
 */
FL_EXTERN_C int fl_spi_platform_isr_start(uint32_t timer_hz) {
    if (s_isr_handle.is_valid()) {
        ESP_LOGW(PARALLEL_SPI_TAG, "Timer already initialized");
        return -1;
    }

    // Configure ISR using cross-platform API
    fl::isr::isr_config_t config;
    config.handler = spi_isr_wrapper;
    config.user_data = nullptr;
    config.frequency_hz = timer_hz;
    config.priority = fl::isr::ISR_PRIORITY_HIGH;  // Level 3 on ESP32-C3
    config.flags = fl::isr::ISR_FLAG_IRAM_SAFE;

    int result = fl::isr::attachTimerHandler(config, &s_isr_handle);
    if (result != 0) {
        ESP_LOGE(PARALLEL_SPI_TAG, "Failed to attach timer: %s", fl::isr::getErrorString(result));
        return result;
    }

    ESP_LOGI(PARALLEL_SPI_TAG, "Timer started at %lu Hz using fl::isr API", timer_hz);
    return 0;
}

/**
 * Stop ISR and timer
 */
FL_EXTERN_C void fl_spi_platform_isr_stop(void) {
    if (s_isr_handle.is_valid()) {
        int result = fl::isr::detachHandler(s_isr_handle);
        if (result == 0) {
            ESP_LOGI(PARALLEL_SPI_TAG, "Timer stopped using fl::isr API");
        } else {
            ESP_LOGW(PARALLEL_SPI_TAG, "Failed to detach timer: %s", fl::isr::getErrorString(result));
        }
    }
}

#endif // CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2
#endif // ESP32
