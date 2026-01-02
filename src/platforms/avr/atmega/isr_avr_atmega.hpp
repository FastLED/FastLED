/*
  FastLED — AVR ATmega ISR Implementation
  ----------------------------------------
  ATmega-specific implementation of the cross-platform ISR API.
  Supports classic ATmega chips with Timer1 hardware.

  Hardware Details:
  - Uses Timer1 (16-bit timer) for precise timing
  - Frequency range: ~1 Hz to ~250 kHz (16 MHz CPU)
  - Available prescalers: 1, 8, 64, 256, 1024
  - CTC (Clear Timer on Compare) mode for accurate frequency generation
  - No hardware interrupt priority (AVR interrupts are equal priority)
  - External interrupts not yet implemented

  Platform Support:
  - Classic ATmega chips: ATmega328P (Uno), ATmega2560 (Mega), ATmega32U4 (Leonardo), etc.
  - NOT for megaAVR 0-series/1-series (e.g., ATmega4809/Nano Every - different timer architecture)
  - NOT for ATtiny chips (use null implementation)

  License: MIT (FastLED)
*/

#pragma once

#include "platforms/avr/is_avr.h"

// Only compile this implementation for ATmega chips with Timer1 hardware.
#if defined(FL_IS_AVR_ATMEGA)

#define FL_ISR_AVR_IMPLEMENTED  // Sentinel macro to indicate this implementation is active

#include "fl/isr.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/compiler_control.h"

// AVR libc headers
FL_EXTERN_C_BEGIN
#include <avr/interrupt.h>
#include <avr/io.h>
FL_EXTERN_C_END

// Compatibility macros for older AVR chips
// ATmega8A and similar older chips use TIMSK instead of TIMSK1
#if !defined(TIMSK1) && defined(TIMSK)
#define TIMSK1 TIMSK
#endif

namespace fl {
namespace isr {
namespace platform {

// =============================================================================
// Platform-Specific Handle Storage
// =============================================================================

struct avr_isr_handle_data {
    isr_handler_t mUserHandler;    // User handler function
    void* mUserData;                // User context
    uint32_t mFrequencyHz;          // Timer frequency in Hz
    uint8_t mGpioPin;               // GPIO pin number (0xFF if not GPIO)
    bool mIsTimer;                  // true = timer ISR, false = external ISR
    bool mIsEnabled;                // Current enable state
    uint8_t mPrescalerIndex;        // Prescaler index (0-4)
    uint16_t mOcrValue;             // OCR1A value for timer

    avr_isr_handle_data()
        : mUserHandler(nullptr)
        , mUserData(nullptr)
        , mFrequencyHz(0)
        , mGpioPin(0xFF)
        , mIsTimer(false)
        , mIsEnabled(false)
        , mPrescalerIndex(0)
        , mOcrValue(0)
    {}
};

// Platform ID for AVR
constexpr uint8_t AVR_PLATFORM_ID = 2;

// Global timer handle data (AVR Timer1 limitation: only one timer can be active)
static avr_isr_handle_data* g_avr_timer_data = nullptr;

// Prescaler values and corresponding CS1x bits
struct prescaler_config_t {
    uint16_t value;
    uint8_t cs_bits;  // CS12:CS11:CS10 bits
};

static const prescaler_config_t PRESCALERS[] = {
    {1,    (0 << CS12) | (0 << CS11) | (1 << CS10)},  // No prescaler
    {8,    (0 << CS12) | (1 << CS11) | (0 << CS10)},  // /8
    {64,   (0 << CS12) | (1 << CS11) | (1 << CS10)},  // /64
    {256,  (1 << CS12) | (0 << CS11) | (0 << CS10)},  // /256
    {1024, (1 << CS12) | (0 << CS11) | (1 << CS10)},  // /1024
};
constexpr uint8_t NUM_PRESCALERS = sizeof(PRESCALERS) / sizeof(PRESCALERS[0]);

// =============================================================================
// Timer Calculation Helpers
// =============================================================================

/**
 * Calculate optimal prescaler and OCR1A value for target frequency.
 * Formula: OCR1A = (F_CPU / (prescaler * frequency)) - 1
 * Returns true on success, false if frequency is out of range.
 */
static bool calculate_timer_config(uint32_t target_freq_hz, uint8_t& prescaler_idx, uint16_t& ocr_value) {
    // Try each prescaler, starting with smallest for best precision
    for (uint8_t i = 0; i < NUM_PRESCALERS; i++) {
        uint32_t prescaler = PRESCALERS[i].value;
        // Calculate OCR value: (F_CPU / (prescaler * freq)) - 1
        uint32_t ocr_calc = (F_CPU / (prescaler * target_freq_hz)) - 1;

        // OCR1A is 16-bit, so must fit in 0-65535
        if (ocr_calc <= 65535) {
            prescaler_idx = i;
            ocr_value = static_cast<uint16_t>(ocr_calc);
            return true;
        }
    }

    return false;  // Frequency too low for available prescalers
}

/**
 * Calculate actual frequency achieved with given prescaler and OCR value.
 * Formula: freq = F_CPU / (prescaler * (OCR1A + 1))
 */
static uint32_t calculate_actual_frequency(uint8_t prescaler_idx, uint16_t ocr_value) {
    uint32_t prescaler = PRESCALERS[prescaler_idx].value;
    return F_CPU / (prescaler * (static_cast<uint32_t>(ocr_value) + 1));
}

// =============================================================================
// Timer1 ISR (TIMER1_COMPA_vect)
// =============================================================================

/**
 * Timer1 Compare Match A ISR
 * This is called automatically when TCNT1 == OCR1A in CTC mode
 */
ISR(TIMER1_COMPA_vect) {
    if (g_avr_timer_data && g_avr_timer_data->mUserHandler) {
        g_avr_timer_data->mUserHandler(g_avr_timer_data->mUserData);
    }
}

// =============================================================================
// AVR ATmega ISR Implementation (fl::isr::platform namespace)
// =============================================================================

int attach_timer_handler(const isr_config_t& config, isr_handle_t* out_handle) {
    if (!config.handler) {
        FL_WARN("AVR ISR: handler is null");
        return -1;  // Invalid parameter
    }

    if (config.frequency_hz == 0) {
        FL_WARN("AVR ISR: frequency_hz is 0");
        return -2;  // Invalid frequency
    }

    // Check if timer is already in use
    if (g_avr_timer_data != nullptr) {
        FL_WARN("AVR ISR: Timer1 already in use (only one timer supported)");
        return -16;  // Timer already in use
    }

    // Allocate handle data
    avr_isr_handle_data* handle_data = new avr_isr_handle_data();
    if (!handle_data) {
        FL_WARN("AVR ISR: failed to allocate handle data");
        return -3;  // Out of memory
    }

    // Calculate timer configuration
    uint8_t prescaler_idx;
    uint16_t ocr_value;
    if (!calculate_timer_config(config.frequency_hz, prescaler_idx, ocr_value)) {
        FL_WARN("AVR ISR: frequency " << config.frequency_hz << " Hz out of range");
        delete handle_data;
        return -2;  // Invalid frequency (out of range)
    }

    // Calculate actual achieved frequency
    uint32_t actual_freq = calculate_actual_frequency(prescaler_idx, ocr_value);

    // Warn if frequency error is >5%
    int32_t freq_error = static_cast<int32_t>(actual_freq) - static_cast<int32_t>(config.frequency_hz);
    int32_t error_pct = (freq_error * 100) / static_cast<int32_t>(config.frequency_hz);
    if (error_pct > 5 || error_pct < -5) {
        FL_WARN("AVR ISR: frequency error " << error_pct << "% (requested "
                << config.frequency_hz << " Hz, actual " << actual_freq << " Hz)");
    }

    FL_DBG("AVR ISR: Timer1 config: prescaler=" << PRESCALERS[prescaler_idx].value
           << ", OCR1A=" << ocr_value << ", actual_freq=" << actual_freq << " Hz");

    // Store configuration
    handle_data->mIsTimer = true;
    handle_data->mUserHandler = config.handler;
    handle_data->mUserData = config.user_data;
    handle_data->mFrequencyHz = actual_freq;
    handle_data->mPrescalerIndex = prescaler_idx;
    handle_data->mOcrValue = ocr_value;

    // Store global pointer for ISR access
    g_avr_timer_data = handle_data;

    // Disable interrupts during timer configuration
    uint8_t oldSREG = SREG;
    cli();

    // Configure Timer1 for CTC mode (Clear Timer on Compare)
    // WGM13:WGM12:WGM11:WGM10 = 0100 (CTC mode, TOP = OCR1A)
    TCCR1A = 0;  // Normal port operation, WGM11:WGM10 = 00
    TCCR1B = (1 << WGM12) | PRESCALERS[prescaler_idx].cs_bits;  // CTC mode, set prescaler

    // Set compare value
    OCR1A = ocr_value;

    // Reset counter
    TCNT1 = 0;

    // Enable Timer1 Compare Match A interrupt
    TIMSK1 |= (1 << OCIE1A);

    // Restore interrupts
    SREG = oldSREG;

    handle_data->mIsEnabled = true;

    FL_DBG("AVR ISR: Timer1 started at " << actual_freq << " Hz");

    // Populate output handle
    if (out_handle) {
        out_handle->platform_handle = handle_data;
        out_handle->handler = config.handler;
        out_handle->user_data = config.user_data;
        out_handle->platform_id = AVR_PLATFORM_ID;
    }

    return 0;  // Success
}

int attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* out_handle) {
    // External interrupts not yet implemented
    // AVR supports external interrupts via INT0/INT1 and Pin Change Interrupts
    // Implementation would use Arduino's attachInterrupt() or direct register manipulation
    (void)pin;
    (void)config;
    (void)out_handle;
    FL_WARN("AVR ISR: external interrupts not yet implemented");
    return -100;  // Not implemented
}

int detach_handler(isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != AVR_PLATFORM_ID) {
        FL_WARN("AVR ISR: invalid handle");
        return -1;  // Invalid handle
    }

    avr_isr_handle_data* handle_data = static_cast<avr_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("AVR ISR: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->mIsTimer) {
        // Disable Timer1 interrupts
        uint8_t oldSREG = SREG;
        cli();

        TIMSK1 &= ~(1 << OCIE1A);  // Disable compare match interrupt
        TCCR1B = 0;  // Stop timer (no clock source)
        TCCR1A = 0;  // Reset control register

        SREG = oldSREG;

        // Clear global pointer if this is the active timer
        if (g_avr_timer_data == handle_data) {
            g_avr_timer_data = nullptr;
        }
    }

    delete handle_data;
    handle.platform_handle = nullptr;
    handle.platform_id = 0;

    FL_DBG("AVR ISR: handler detached");
    return 0;  // Success
}

int enable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != AVR_PLATFORM_ID) {
        FL_WARN("AVR ISR: invalid handle");
        return -1;  // Invalid handle
    }

    avr_isr_handle_data* handle_data = static_cast<avr_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("AVR ISR: null handle data");
        return -1;  // Invalid handle
    }

    if (handle_data->mIsEnabled) {
        return 0;  // Already enabled
    }

    if (handle_data->mIsTimer) {
        uint8_t oldSREG = SREG;
        cli();

        // Re-enable timer with stored configuration
        TCCR1A = 0;
        TCCR1B = (1 << WGM12) | PRESCALERS[handle_data->mPrescalerIndex].cs_bits;
        OCR1A = handle_data->mOcrValue;
        TCNT1 = 0;
        TIMSK1 |= (1 << OCIE1A);

        SREG = oldSREG;

        handle_data->mIsEnabled = true;
    }

    return 0;  // Success
}

int disable_handler(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != AVR_PLATFORM_ID) {
        FL_WARN("AVR ISR: invalid handle");
        return -1;  // Invalid handle
    }

    avr_isr_handle_data* handle_data = static_cast<avr_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        FL_WARN("AVR ISR: null handle data");
        return -1;  // Invalid handle
    }

    if (!handle_data->mIsEnabled) {
        return 0;  // Already disabled
    }

    if (handle_data->mIsTimer) {
        uint8_t oldSREG = SREG;
        cli();

        TIMSK1 &= ~(1 << OCIE1A);  // Disable interrupt
        TCCR1B = 0;  // Stop timer

        SREG = oldSREG;

        handle_data->mIsEnabled = false;
    }

    return 0;  // Success
}

bool is_handler_enabled(const isr_handle_t& handle) {
    if (!handle.is_valid() || handle.platform_id != AVR_PLATFORM_ID) {
        return false;
    }

    avr_isr_handle_data* handle_data = static_cast<avr_isr_handle_data*>(handle.platform_handle);
    if (!handle_data) {
        return false;
    }

    return handle_data->mIsEnabled;
}

const char* get_error_string(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "Invalid parameter or handle";
        case -2: return "Invalid frequency (out of range)";
        case -3: return "Out of memory";
        case -16: return "Timer already in use (only one timer supported)";
        case -100: return "Not implemented (external interrupts)";
        default: return "Unknown error";
    }
}

const char* get_platform_name() {
#if defined(__AVR_ATmega328P__)
    return "AVR ATmega328P (Uno/Nano)";
#elif defined(__AVR_ATmega2560__)
    return "AVR ATmega2560 (Mega)";
#elif defined(__AVR_ATmega32U4__)
    return "AVR ATmega32U4 (Leonardo/Micro)";
#elif defined(__AVR_ATmega1280__)
    return "AVR ATmega1280";
#elif defined(__AVR_ATmega168__)
    return "AVR ATmega168";
#elif defined(FL_IS_AVR_ATMEGA)
    return "AVR ATmega (generic)";
#else
    return "AVR ATmega (unknown variant)";
#endif
}

uint32_t get_max_timer_frequency() {
    // Maximum frequency with prescaler=1 and OCR1A=1
    // freq = F_CPU / (1 * (1 + 1)) = F_CPU / 2
    // For 16MHz: 8MHz theoretical, but ~250kHz is more practical
    return F_CPU / 64;  // Conservative estimate (250kHz @ 16MHz)
}

uint32_t get_min_timer_frequency() {
    // Minimum frequency with prescaler=1024 and OCR1A=65535
    // freq = F_CPU / (1024 * (65535 + 1))
    // For 16MHz: ~0.238 Hz
    return 1;  // 1 Hz minimum (well within capability)
}

uint8_t get_max_priority() {
    // AVR has no hardware interrupt priority levels
    // All interrupts are equal priority (can't nest by default)
    // Return 0 to indicate no priority support
    return 0;
}

bool requires_assembly_handler(uint8_t priority) {
    (void)priority;
    // AVR interrupts use ISR() macro - no assembly required
    return false;
}

} // namespace platform
} // namespace isr
} // namespace fl

#endif // defined(FL_IS_AVR_ATMEGA)
