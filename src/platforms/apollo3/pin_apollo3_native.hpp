#pragma once

/// @file platforms/apollo3/pin_apollo3_native.hpp
/// Apollo3 (Ambiq) native HAL GPIO implementation (non-Arduino path)
///
/// Provides Apollo3 HAL-based GPIO functions using am_hal_gpio_* APIs.
/// Used when building without Arduino framework.
///
/// Pin mode mapping:
/// - PinMode::Input = INPUT (GPIO input enabled)
/// - PinMode::Output = OUTPUT (GPIO output push-pull)
/// - PinMode::InputPullup = INPUT_PULLUP (GPIO input with weak pullup)
/// - PinMode::InputPulldown = INPUT_PULLDOWN (GPIO input with weak pulldown)
///
/// IMPORTANT: Requires Ambiq Apollo3 HAL headers (am_hal_gpio.h).
/// The fastgpio functions used here provide high-speed GPIO access.

// External C headers from Ambiq HAL
#include "fl/compiler_control.h"

FL_EXTERN_C_BEGIN
#include "am_mcu_apollo.h"
#include "am_hal_gpio.h"
#include "am_hal_adc.h"
#include "am_hal_ctimer.h"
FL_EXTERN_C_END

#include "fl/warn.h"
#include "fl/dbg.h"
#include "fl/pin.h"

namespace fl {
namespace platform {

// ============================================================================
// Internal State for ADC and PWM
// ============================================================================

namespace apollo3_internal {

// ADC state - initialized on first use
struct ADCState {
    void* handle = nullptr;
    bool initialized = false;
    am_hal_adc_refsel_e reference = AM_HAL_ADC_REFSEL_INT_1P5;  // Default 1.5V
};

static ADCState g_adc_state;

// PWM state - track which pins are configured for PWM
struct PWMState {
    uint8_t timer_num;
    uint8_t segment;
    uint32_t period;
    bool active;
};

// Apollo3 has up to 50 pads, track PWM state for each
static PWMState g_pwm_state[AM_HAL_GPIO_MAX_PADS] = {};

}  // namespace apollo3_internal

// ============================================================================
// Digital Pin Functions
// ============================================================================

inline void pinMode(int pin, PinMode mode) {
    if (pin < 0 || pin >= AM_HAL_GPIO_MAX_PADS) {
        FL_WARN("Apollo3: Invalid pin " << pin);
        return;
    }

    am_hal_gpio_pincfg_t pin_config = {0};

    switch (mode) {
        case PinMode::Input:
            pin_config.uFuncSel = 3;  // GPIO function
            pin_config.eGPInput = AM_HAL_GPIO_PIN_INPUT_ENABLE;
            pin_config.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_DISABLE;
            pin_config.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
            break;

        case PinMode::Output:
            pin_config.uFuncSel = 3;  // GPIO function
            pin_config.eGPInput = AM_HAL_GPIO_PIN_INPUT_NONE;
            pin_config.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;
            pin_config.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
            pin_config.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
            break;

        case PinMode::InputPullup:
            pin_config.uFuncSel = 3;  // GPIO function
            pin_config.eGPInput = AM_HAL_GPIO_PIN_INPUT_ENABLE;
            pin_config.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_DISABLE;
            pin_config.ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK;  // ~1.5K pullup
            break;

        case PinMode::InputPulldown:
            pin_config.uFuncSel = 3;  // GPIO function
            pin_config.eGPInput = AM_HAL_GPIO_PIN_INPUT_ENABLE;
            pin_config.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_DISABLE;
            pin_config.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;  // Apollo3 doesn't have internal pulldown
            FL_WARN("Apollo3: InputPulldown mode not supported (no internal pulldown), using Input mode");
            break;
    }

    uint32_t result = am_hal_gpio_pinconfig(pin, pin_config);
    if (result != AM_HAL_STATUS_SUCCESS) {
        FL_WARN("Apollo3: Failed to configure pin " << pin);
    }
}

inline void digitalWrite(int pin, PinValue val) {
    if (pin < 0 || pin >= AM_HAL_GPIO_MAX_PADS) {
        return;  // Invalid pin
    }

    // Use am_hal_gpio_state_write for standard GPIO writes
    am_hal_gpio_write_type_e write_type = (val == PinValue::High) ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR;
    am_hal_gpio_state_write(pin, write_type);
}

inline PinValue digitalRead(int pin) {
    if (pin < 0 || pin >= AM_HAL_GPIO_MAX_PADS) {
        return PinValue::Low;  // Invalid pin
    }

    uint32_t read_state = 0;
    uint32_t result = am_hal_gpio_state_read(pin, AM_HAL_GPIO_INPUT_READ, &read_state);

    if (result != AM_HAL_STATUS_SUCCESS) {
        return PinValue::Low;
    }

    return read_state ? PinValue::High : PinValue::Low;
}

// ============================================================================
// Analog Pin Functions
// ============================================================================

inline uint16_t analogRead(int pin) {
    // Apollo3 ADC implementation using HAL APIs
    // Maps Arduino pin numbers to ADC channels and performs 12-bit conversion
    // Returns 10-bit value (0-1023) for Arduino compatibility

    using namespace apollo3_internal;

    // Pin to ADC channel mapping (based on Apollo3 datasheet)
    // Single-ended channels: ADCSE0-ADCSE9
    am_hal_adc_slot_chan_e adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE0;

    // Map common Apollo3/Artemis pins to ADC channels
    // Note: Exact mapping depends on specific board/module variant
    switch (pin) {
        case 16: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE0; break;
        case 29: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE1; break;
        case 11: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE2; break;
        case 13: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE3; break;
        case 31: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE4; break;
        case 32: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE5; break;
        case 33: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE6; break;
        case 34: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE7; break;
        case 35: adc_channel = AM_HAL_ADC_SLOT_CHSEL_SE8; break;
        default:
            FL_WARN("Apollo3: Pin " << pin << " does not support ADC");
            return 0;
    }

    // Initialize ADC on first use
    if (!g_adc_state.initialized) {
        uint32_t status = am_hal_adc_initialize(0, &g_adc_state.handle);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC initialization failed");
            return 0;
        }

        // Power on ADC
        status = am_hal_adc_power_control(g_adc_state.handle, AM_HAL_SYSCTRL_WAKE, false);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC power control failed");
            am_hal_adc_deinitialize(g_adc_state.handle);
            g_adc_state.handle = nullptr;
            return 0;
        }

        // Configure ADC for single-shot operation
        am_hal_adc_config_t adc_config = {};
        adc_config.eClock = AM_HAL_ADC_CLKSEL_HFRC;  // Use high-frequency RC oscillator
        adc_config.ePolarity = AM_HAL_ADC_TRIGPOL_RISING;
        adc_config.eTrigger = AM_HAL_ADC_TRIGSEL_SOFTWARE;
        adc_config.eReference = g_adc_state.reference;  // Use configured reference
        adc_config.eClockMode = AM_HAL_ADC_CLKMODE_LOW_LATENCY;
        adc_config.ePowerMode = AM_HAL_ADC_LPMODE0;
        adc_config.eRepeat = AM_HAL_ADC_SINGLE_SCAN;

        status = am_hal_adc_configure(g_adc_state.handle, &adc_config);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC configuration failed");
            am_hal_adc_power_control(g_adc_state.handle, AM_HAL_SYSCTRL_DEEPSLEEP, false);
            am_hal_adc_deinitialize(g_adc_state.handle);
            g_adc_state.handle = nullptr;
            return 0;
        }

        // Enable ADC
        status = am_hal_adc_enable(g_adc_state.handle);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC enable failed");
            am_hal_adc_power_control(g_adc_state.handle, AM_HAL_SYSCTRL_DEEPSLEEP, false);
            am_hal_adc_deinitialize(g_adc_state.handle);
            g_adc_state.handle = nullptr;
            return 0;
        }

        g_adc_state.initialized = true;
    }

    // Configure ADC slot for this conversion
    am_hal_adc_slot_config_t slot_config = {};
    slot_config.eMeasToAvg = AM_HAL_ADC_SLOT_AVG_1;  // No averaging for single read
    slot_config.ePrecisionMode = AM_HAL_ADC_SLOT_12BIT;  // 12-bit precision
    slot_config.eChannel = adc_channel;
    slot_config.bWindowCompare = false;
    slot_config.bEnabled = true;

    uint32_t status = am_hal_adc_configure_slot(g_adc_state.handle, 0, &slot_config);
    if (status != AM_HAL_STATUS_SUCCESS) {
        FL_WARN("Apollo3: ADC slot configuration failed");
        return 0;
    }

    // Trigger ADC conversion
    status = am_hal_adc_sw_trigger(g_adc_state.handle);
    if (status != AM_HAL_STATUS_SUCCESS) {
        FL_WARN("Apollo3: ADC trigger failed");
        return 0;
    }

    // Wait for conversion to complete (poll for completion)
    // Typical conversion time is ~5-10 microseconds
    uint32_t timeout = 10000;  // 10ms timeout
    am_hal_adc_sample_t sample;
    uint32_t sample_count = 0;

    while (timeout-- > 0) {
        sample_count = 1;
        status = am_hal_adc_samples_read(g_adc_state.handle, false, nullptr, &sample_count, &sample);

        if (status == AM_HAL_STATUS_SUCCESS && sample_count > 0) {
            // Conversion complete - extract 12-bit value
            uint16_t adc_value = AM_HAL_ADC_FIFO_SAMPLE(sample.ui32Sample);

            // Scale 12-bit (0-4095) to 10-bit (0-1023) for Arduino compatibility
            return static_cast<uint16_t>(adc_value >> 2);
        }

        // Small delay before retry
        for (volatile int i = 0; i < 10; i++);
    }

    FL_WARN("Apollo3: ADC conversion timeout");
    return 0;
}

inline void analogWrite(int pin, uint16_t val) {
    // Apollo3 PWM implementation using CTIMER HAL APIs
    // Generates PWM output at ~490Hz (Arduino default) with 8-bit duty cycle

    using namespace apollo3_internal;

    // Validate pin range
    if (pin < 0 || pin >= AM_HAL_GPIO_MAX_PADS) {
        FL_WARN("Apollo3: Invalid pin " << pin);
        return;
    }

    // Clamp value to 8-bit range (0-255) for Arduino compatibility
    uint16_t clamped_val = val;
    if (clamped_val > 255) clamped_val = 255;

    // Pin to CTIMER mapping (based on Apollo3 capabilities)
    // Each pin can map to different timers/segments via alternate functions
    // This is a simplified mapping - full implementation would query pinmux
    uint8_t timer_num = 0;
    uint8_t segment = 0;
    uint32_t output_cfg = 0;

    // Map common pins to timers (simplified - actual mapping more complex)
    // Timer segment: 0 = A, 1 = B
    switch (pin) {
        case 5:  timer_num = 0; segment = AM_HAL_CTIMER_TIMERB; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        case 6:  timer_num = 1; segment = AM_HAL_CTIMER_TIMERA; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        case 12: timer_num = 1; segment = AM_HAL_CTIMER_TIMERB; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        case 13: timer_num = 2; segment = AM_HAL_CTIMER_TIMERA; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        case 18: timer_num = 2; segment = AM_HAL_CTIMER_TIMERB; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        case 19: timer_num = 3; segment = AM_HAL_CTIMER_TIMERA; output_cfg = AM_HAL_CTIMER_OUTPUT_NORMAL; break;
        default:
            // Pin doesn't support PWM - fall back to digital write
            fl::platform::pinMode(pin, PinMode::Output);
            fl::platform::digitalWrite(pin, clamped_val >= 128 ? PinValue::High : PinValue::Low);
            return;
    }

    // Calculate PWM period for 490Hz (Arduino default)
    // Apollo3 runs at 48MHz, CTIMER can use HFRC (48MHz) with prescaler
    // Period = Clock / Frequency
    const uint32_t PWM_FREQUENCY = 490;  // Hz
    const uint32_t TIMER_CLOCK = 48000000 / 16;  // 3MHz with prescaler
    uint32_t period = (TIMER_CLOCK / PWM_FREQUENCY) - 1;

    // Calculate duty cycle (on-time) from 8-bit value
    uint32_t on_time = (clamped_val * period) / 255;

    // Configure timer if not already active for this pin
    if (!g_pwm_state[pin].active || g_pwm_state[pin].period != period) {
        // Configure pin as CTIMER output
        am_hal_gpio_pincfg_t pin_config = {0};
        pin_config.uFuncSel = 2;  // CT function (timer output)
        pin_config.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA;
        pin_config.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;

        uint32_t status = am_hal_gpio_pinconfig(pin, pin_config);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: Failed to configure pin " << pin << " for PWM");
            return;
        }

        // Configure timer for PWM repeated mode
        uint32_t timer_config = (AM_HAL_CTIMER_FN_PWM_REPEAT |
                                 AM_HAL_CTIMER_HFRC_3MHZ);

        am_hal_ctimer_config_single(timer_num, segment, timer_config);

        // Configure output on the pin
        am_hal_ctimer_output_config(timer_num, segment, pin,
                                    output_cfg,
                                    AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA);

        // Set PWM period and initial duty cycle
        am_hal_ctimer_period_set(timer_num, segment, period, on_time);

        // Start timer
        am_hal_ctimer_start(timer_num, segment);

        // Update state tracking
        g_pwm_state[pin].timer_num = timer_num;
        g_pwm_state[pin].segment = segment;
        g_pwm_state[pin].period = period;
        g_pwm_state[pin].active = true;
    } else {
        // Timer already running, just update duty cycle
        am_hal_ctimer_compare_set(timer_num, segment, 0, on_time);
    }
}

inline void setPwm16(int pin, uint16_t val) {
    // Apollo3 16-bit PWM implementation using CTIMER
    // Hardware supports 32-bit counters, capable of full 16-bit PWM
    // Scale 16-bit value down to 8-bit for simplified implementation
    // Full 16-bit would set period=65535 instead of current ~6121
    analogWrite(pin, val >> 8);
}

inline void setAdcRange(AdcRange range) {
    // Apollo3 ADC reference voltage configuration
    // Supports internal 1.5V, 2.0V, and external references
    //
    // fl::AdcRange modes:
    // - Default: Use default reference (1.5V internal)
    // - Range0_1V1: Use internal reference (1.5V - closest available)
    // - Range0_1V5: Use internal 1.5V reference
    // - Range0_2V2: Use internal 2.0V reference (closest to 2.2V)
    // - Range0_3V3/Range0_5V: Use internal 2.0V reference (max internal)
    // - External: Use external reference on VREF pin

    using namespace apollo3_internal;

    // Determine Apollo3 ADC reference from fl::AdcRange
    am_hal_adc_refsel_e adc_ref = AM_HAL_ADC_REFSEL_INT_1P5;

    switch (range) {
        case AdcRange::Default:
        case AdcRange::Range0_1V1:
        case AdcRange::Range0_1V5:
            adc_ref = AM_HAL_ADC_REFSEL_INT_1P5;  // 1.5V internal
            break;
        case AdcRange::Range0_2V2:
        case AdcRange::Range0_3V3:
        case AdcRange::Range0_5V:
            adc_ref = AM_HAL_ADC_REFSEL_INT_2P0;  // 2.0V internal
            break;
        case AdcRange::External:
            adc_ref = AM_HAL_ADC_REFSEL_EXT_2P0;  // External reference
            break;
    }

    // If ADC is already initialized, reconfigure with new reference
    if (g_adc_state.initialized) {
        // Disable ADC before reconfiguration
        am_hal_adc_disable(g_adc_state.handle);

        // Reconfigure with new reference voltage
        am_hal_adc_config_t adc_config = {};
        adc_config.eClock = AM_HAL_ADC_CLKSEL_HFRC;
        adc_config.ePolarity = AM_HAL_ADC_TRIGPOL_RISING;
        adc_config.eTrigger = AM_HAL_ADC_TRIGSEL_SOFTWARE;
        adc_config.eReference = adc_ref;  // Apply new reference
        adc_config.eClockMode = AM_HAL_ADC_CLKMODE_LOW_LATENCY;
        adc_config.ePowerMode = AM_HAL_ADC_LPMODE0;
        adc_config.eRepeat = AM_HAL_ADC_SINGLE_SCAN;

        uint32_t status = am_hal_adc_configure(g_adc_state.handle, &adc_config);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC reconfiguration failed");
            return;
        }

        // Re-enable ADC
        status = am_hal_adc_enable(g_adc_state.handle);
        if (status != AM_HAL_STATUS_SUCCESS) {
            FL_WARN("Apollo3: ADC re-enable failed");
            return;
        }

        FL_DBG("Apollo3: ADC reference changed");
    }

    // Store reference for future use (will be applied on next ADC init if not yet initialized)
    g_adc_state.reference = adc_ref;
}

}  // namespace platform
}  // namespace fl
