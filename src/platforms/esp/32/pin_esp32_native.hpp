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
#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#if ESP_IDF_VERSION_5_OR_HIGHER
#include "esp_adc/adc_oneshot.h"
#else
// Legacy ADC API for IDF v4.x
#include "driver/adc.h"
#endif

FL_EXTERN_C_END

namespace fl {
namespace platforms {

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

inline u16 analogRead(int pin) {
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
    return static_cast<u16>(raw_value);
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

inline u16 analogRead(int pin) {
    initADC1();

    // Note: This is a stub implementation. Real implementation would need
    // proper GPIO-to-ADC channel mapping based on ESP32 variant
    adc1_channel_t channel = ADC1_CHANNEL_0;  // Placeholder
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);

    int raw_value = adc1_get_raw(channel);
    if (raw_value < 0) {
        return 0;
    }
    return static_cast<u16>(raw_value);
}

#endif  // ESP_IDF_VERSION_5_OR_HIGHER

inline void analogWrite(int pin, u16 val) {
    // ESP-IDF does not provide a simple analogWrite API like Arduino
    // This would require LEDC (LED PWM Controller) setup
    // Stub implementation - no-op
    (void)pin;
    (void)val;
}

inline void setPwm16(int pin, u16 val) {
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

// ============================================================================
// PWM Frequency Control (LEDC hardware backend)
// ============================================================================

// LEDC speed mode: original ESP32 has both high-speed and low-speed modes,
// all other variants (S2, S3, C3, C6, H2, P4, etc.) only have low-speed mode.
#if defined(SOC_LEDC_SUPPORT_HS_MODE) && SOC_LEDC_SUPPORT_HS_MODE
#define FL_LEDC_SPEED_MODE LEDC_HIGH_SPEED_MODE
#else
#define FL_LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#endif

// Maximum number of LEDC channels we manage
// ESP32-C6 and ESP32-H2 have 6 channels, others have 8
#if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2)
#define FL_LEDC_MAX_CHANNELS 6
#else
#define FL_LEDC_MAX_CHANNELS 8
#endif

struct LedcPinAlloc {
    int pin;                    // -1 = free
    ledc_channel_t channel;
    ledc_timer_t timer;
    u32 frequency_hz;
};

// okay static in header
static LedcPinAlloc g_ledc_alloc[FL_LEDC_MAX_CHANNELS] = {
    {-1, LEDC_CHANNEL_0, LEDC_TIMER_0, 0},
    {-1, LEDC_CHANNEL_1, LEDC_TIMER_0, 0},
    {-1, LEDC_CHANNEL_2, LEDC_TIMER_1, 0},
    {-1, LEDC_CHANNEL_3, LEDC_TIMER_1, 0},
    {-1, LEDC_CHANNEL_4, LEDC_TIMER_2, 0},
    {-1, LEDC_CHANNEL_5, LEDC_TIMER_2, 0},
#if FL_LEDC_MAX_CHANNELS >= 8
    {-1, LEDC_CHANNEL_6, LEDC_TIMER_3, 0},
    {-1, LEDC_CHANNEL_7, LEDC_TIMER_3, 0},
#endif
};

inline bool needsPwmIsrFallback(int /*pin*/, u32 frequency_hz) {
    // LEDC peripheral supports roughly 10 Hz to 40 MHz.
    // Anything outside that range needs the ISR software fallback.
    if (frequency_hz < 10 || frequency_hz > 40000000UL) {
        return true;
    }
    return false;
}

inline int setPwmFrequencyNative(int pin, u32 frequency_hz) {
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return -1;  // Invalid pin
    }

    // Check if this pin is already allocated — reconfigure it
    int slot = -1;
    for (int i = 0; i < FL_LEDC_MAX_CHANNELS; i++) {
        if (g_ledc_alloc[i].pin == pin) {
            slot = i;
            break;
        }
    }

    // If not found, allocate a free slot
    if (slot < 0) {
        for (int i = 0; i < FL_LEDC_MAX_CHANNELS; i++) {
            if (g_ledc_alloc[i].pin < 0) {
                slot = i;
                break;
            }
        }
    }

    if (slot < 0) {
        return -2;  // No free LEDC channels
    }

    LedcPinAlloc& alloc = g_ledc_alloc[slot];

    // Determine the best duty resolution for the requested frequency.
    // LEDC supports 1-20 bit resolution depending on the clock and frequency.
#if ESP_IDF_VERSION_5_OR_HIGHER
    // ESP-IDF v5+ provides a helper to find the best resolution
    // For ESP32 with LEDC_AUTO_CLK, typical source clock is 80MHz APB clock
    const u32 ledc_src_clk_hz = 80000000;  // 80MHz APB clock
    u32 resolution = ledc_find_suitable_duty_resolution(
        ledc_src_clk_hz, static_cast<u32>(frequency_hz));
    if (resolution == 0) {
        return -3;  // Could not find a suitable resolution
    }
#else
    // ESP-IDF v4.x: calculate manually.
    // resolution = floor(log2(APB_CLK_FREQ / frequency))
    // APB_CLK_FREQ is typically 80 MHz.
    u32 ratio = APB_CLK_FREQ / frequency_hz;
    u32 resolution = 0;
    while (ratio > 1) {
        ratio >>= 1;
        resolution++;
    }
    // Clamp resolution to valid range [1, 16] for v4.x
    if (resolution < 1) {
        resolution = 1;
    }
    if (resolution > 16) {
        resolution = 16;
    }
#endif

    // Configure the LEDC timer
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = FL_LEDC_SPEED_MODE;
    timer_cfg.timer_num = alloc.timer;
    timer_cfg.duty_resolution = static_cast<ledc_timer_bit_t>(resolution);
    timer_cfg.freq_hz = frequency_hz;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        return -4;  // Timer configuration failed
    }

    // Configure the LEDC channel
    ledc_channel_config_t ch_cfg = {};
    ch_cfg.speed_mode = FL_LEDC_SPEED_MODE;
    ch_cfg.channel = alloc.channel;
    ch_cfg.timer_sel = alloc.timer;
    ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ch_cfg.gpio_num = pin;
    ch_cfg.duty = 0;
    ch_cfg.hpoint = 0;

    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        return -5;  // Channel configuration failed
    }

    // Record the allocation
    alloc.pin = pin;
    alloc.frequency_hz = frequency_hz;

    return 0;
}

inline u32 getPwmFrequencyNative(int pin) {
    for (int i = 0; i < FL_LEDC_MAX_CHANNELS; i++) {
        if (g_ledc_alloc[i].pin == pin) {
            return g_ledc_alloc[i].frequency_hz;
        }
    }
    return 0;  // Not configured
}

}  // namespace platforms
}  // namespace fl
