#pragma once

/// @file platforms/esp/32/pin_esp32_native.hpp
/// ESP32 ESP-IDF native GPIO driver implementation
///
/// Provides pin control functions using ESP-IDF GPIO APIs.
/// This file is used when building without Arduino framework.
///
/// Mode mapping:
/// - PinMode::Input (0) = INPUT (GPIO_MODE_INPUT)
/// - PinMode::Output (1) = OUTPUT (GPIO_MODE_OUTPUT)
/// - PinMode::InputPullup (2) = INPUT_PULLUP (GPIO_MODE_INPUT with pull-up)
/// - PinMode::InputPulldown (3) = INPUT_PULLDOWN (GPIO_MODE_INPUT with pull-down)

#include "fl/compiler_control.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "esp_err.h"

#if ESP_IDF_VERSION_5_OR_HIGHER
#include "esp_adc/adc_oneshot.h"
#else
// Legacy ADC API for IDF v4.x
#include "driver/adc.h"
#endif

FL_EXTERN_C_END

namespace fl {
namespace platform {

// ============================================================================
// Digital Pin Functions
// ============================================================================

inline void pinMode(int pin, PinMode mode) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return;  // Invalid pin
    }

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.intr_type = GPIO_INTR_DISABLE;

    switch (mode) {
        case PinMode::Input:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case PinMode::Output:
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case PinMode::InputPullup:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case PinMode::InputPulldown:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
        default:
            return;  // Unknown mode
    }

    gpio_config(&io_conf);
}

inline void digitalWrite(int pin, PinValue val) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return;  // Invalid pin
    }
    gpio_set_level(static_cast<gpio_num_t>(pin), 
                   val == PinValue::High ? 1 : 0);
}

inline PinValue digitalRead(int pin) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return PinValue::Low;  // Invalid pin
    }
    int raw = gpio_get_level(static_cast<gpio_num_t>(pin));
    return raw ? PinValue::High : PinValue::Low;
}

// ============================================================================
// Analog Pin Functions
// ============================================================================

#if ESP_IDF_VERSION_5_OR_HIGHER

namespace {
    // ADC handle for analog reads (lazy initialization)
    adc_oneshot_unit_handle_t adc1_handle = nullptr;

    void initADC1() {
        if (adc1_handle != nullptr) {
            return;  // Already initialized
        }

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        adc_oneshot_new_unit(&init_config, &adc1_handle);
    }
}

inline uint16_t analogRead(int pin) {
    // Map GPIO pin to ADC channel (ESP32-specific mapping)
    // This is a simplified mapping - real implementation would need
    // platform-specific channel detection
    initADC1();

    if (adc1_handle == nullptr) {
        return 0;  // ADC initialization failed
    }

    // Configure channel (simplified - assumes ADC1 channel 0)
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    // Note: This is a stub implementation. Real implementation would need
    // proper GPIO-to-ADC channel mapping based on ESP32 variant
    adc_channel_t channel = ADC_CHANNEL_0;  // Placeholder
    adc_oneshot_config_channel(adc1_handle, channel, &config);

    int raw_value = 0;
    adc_oneshot_read(adc1_handle, channel, &raw_value);
    return static_cast<uint16_t>(raw_value);
}

#else  // ESP-IDF v4.x legacy ADC API

namespace {
    bool adc1_initialized = false;

    void initADC1() {
        if (adc1_initialized) {
            return;
        }
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_initialized = true;
    }
}

inline uint16_t analogRead(int pin) {
    initADC1();

    // Note: This is a stub implementation. Real implementation would need
    // proper GPIO-to-ADC channel mapping based on ESP32 variant
    adc1_channel_t channel = ADC1_CHANNEL_0;  // Placeholder
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);

    int raw_value = adc1_get_raw(channel);
    if (raw_value < 0) {
        return 0;
    }
    return static_cast<uint16_t>(raw_value);
}

#endif  // ESP_IDF_VERSION_5_OR_HIGHER

inline void analogWrite(int pin, uint16_t val) {
    // ESP-IDF does not provide a simple analogWrite API like Arduino
    // This would require LEDC (LED PWM Controller) setup
    // Stub implementation - no-op
    (void)pin;
    (void)val;
}

inline void setPwm16(int pin, uint16_t val) {
    // ESP-IDF native: Full LEDC 16-bit PWM would require:
    // 1. Channel allocation (16 channels available)
    // 2. ledc_timer_config for 16-bit resolution
    // 3. ledc_channel_config to attach pin to channel
    // 4. ledc_set_duty + ledc_update_duty to set value
    // Stub implementation for now - no-op
    (void)pin;
    (void)val;
}

inline void setAdcRange(AdcRange range) {
    // ESP32 uses ADC attenuation instead of reference voltage
    // This would need to be implemented per-channel in analogRead
    // For now, this is a stub implementation - no-op
    (void)range;
}

}  // namespace platform
}  // namespace fl
