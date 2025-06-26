#pragma once

#include "fl/sketch_macros.h"

// ESP32 Logging Control for FastLED
// 
// This header provides fine-grained control over ESP32 logging to reduce binary size
// by eliminating the _vfprintf_r function that is pulled in by ESP_LOG macros.
//
// Logging behavior:
// - DISABLED by default on all ESP32 platforms to reduce binary size
// - ENABLED automatically if SKETCH_HAS_LOTS_OF_MEMORY is true (ESP32 platforms)
// - Can be explicitly ENABLED by defining FASTLED_ESP32_ENABLE_LOGGING=1
// - Can be explicitly DISABLED by defining FASTLED_ESP32_ENABLE_LOGGING=0
//
// Usage:
//   #include "esp_log_control.h"  // Include this BEFORE esp_log.h
//   #include "esp_log.h"          // Now ESP_LOG macros are controlled

#ifndef FASTLED_ESP32_ENABLE_LOGGING
    // Default behavior: Enable logging only if we have lots of memory
    #if SKETCH_HAS_LOTS_OF_MEMORY
        #define FASTLED_ESP32_ENABLE_LOGGING 1
    #else
        #define FASTLED_ESP32_ENABLE_LOGGING 0
    #endif
#endif

#if !FASTLED_ESP32_ENABLE_LOGGING
    // Disable ESP logging by setting the log level to NONE before including esp_log.h
    #ifndef LOG_LOCAL_LEVEL
        #define LOG_LOCAL_LEVEL ESP_LOG_NONE
    #endif
    
    // Also define the main ESP_LOG_LEVEL to NONE if not already set
    #ifndef CONFIG_LOG_DEFAULT_LEVEL
        #define CONFIG_LOG_DEFAULT_LEVEL 0  // ESP_LOG_NONE
    #endif
    
    // For extra safety, we can also override the ESP_LOG macros to be no-ops
    // This ensures that even if esp_log.h is included without proper level setup,
    // the logging functions won't be called.
    #define FASTLED_ESP_LOGGING_DISABLED 1
#else
    #define FASTLED_ESP_LOGGING_DISABLED 0
#endif

// Helper macro to provide conditional logging that users can call
// This allows FastLED code to use logging conditionally without pulling in printf
#if FASTLED_ESP32_ENABLE_LOGGING
    #define FASTLED_ESP_LOGI(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
    #define FASTLED_ESP_LOGW(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
    #define FASTLED_ESP_LOGE(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
    #define FASTLED_ESP_LOGD(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
    #define FASTLED_ESP_LOGV(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)
#else
    // No-op versions that compile away completely
    #define FASTLED_ESP_LOGI(tag, format, ...) ((void)0)
    #define FASTLED_ESP_LOGW(tag, format, ...) ((void)0)
    #define FASTLED_ESP_LOGE(tag, format, ...) ((void)0)
    #define FASTLED_ESP_LOGD(tag, format, ...) ((void)0)  
    #define FASTLED_ESP_LOGV(tag, format, ...) ((void)0)
#endif

// Optional: Override ESP_ERROR_CHECK to avoid logging on error
// This can be enabled if users want to eliminate all ESP logging
#ifdef FASTLED_ESP32_MINIMAL_ERROR_HANDLING
    #if !FASTLED_ESP32_ENABLE_LOGGING
        #undef ESP_ERROR_CHECK
        #define ESP_ERROR_CHECK(x) do { \
            esp_err_t rc = (x); \
            if (rc != ESP_OK) { \
                abort(); \
            } \
        } while(0)
    #endif
#endif
