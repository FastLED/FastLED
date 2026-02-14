/// @file fl/pin.cpp
/// Compilation boundary for platform-independent pin API
///
/// IMPORTANT: This file acts as a compilation boundary to prevent header pollution.
///
/// Architecture:
/// - fl/pin.h: Minimal public interface (enum declarations + function signatures)
///   → Users include this and get ONLY the interface (no Arduino.h, no platform headers)
///
/// - fl/pin.cpp (this file): Compilation boundary
///   → Includes platforms/pin.h which pulls in platform-specific implementations
///   → Prevents platform headers from leaking into user code
///   → Provides non-inline wrapper functions that forward to platform implementations
///
/// - platforms/pin.h: Trampoline dispatcher
///   → Includes the appropriate platform .hpp file based on compile-time detection
///
/// - platforms/*/pin.hpp: Platform implementations (header-only)
///   → Provides inline/constexpr implementations for zero-overhead
///   → Split into Arduino path (#ifdef ARDUINO) and non-Arduino path
///
/// Why this matters:
/// - Users can safely `#include "fl/pin.h"` without pulling in Arduino.h
/// - Platform detection and implementation selection happens at this boundary
/// - Clean separation between interface and implementation

#include "fl/pin.h"

// Include platform-specific implementations (must come after fl/pin.h for proper type resolution)
#include "platforms/pin.h"

// Additional includes for PWM implementation
#include "fl/isr.h"
#include "fl/log.h"
#include "fl/compiler_control.h"
#include "fl/singleton.h"

namespace fl {

// ============================================================================
// Non-inline wrapper functions (basic I/O)
// ============================================================================
// These wrappers are necessary because the platform-specific implementations
// are inline functions. The linker needs actual symbols to link against when
// other translation units call these functions.
//
// NOTE: pinMode() is defined later, after PWM state management, because it
// needs to release PWM channels.

void digitalWrite(int pin, PinValue val) {
    platforms::digitalWrite(pin, val);
}

PinValue digitalRead(int pin) {
    return platforms::digitalRead(pin);
}

u16 analogRead(int pin) {
    return platforms::analogRead(pin);
}

void setAdcRange(AdcRange range) {
    platforms::setAdcRange(range);
}

// ============================================================================
// Unified PWM State Management
// ============================================================================

namespace pwm_state {

enum class PwmBackend : u8 {
    None,           // No setPwmFrequency called — use platform default
    Native,         // Platform hardware PWM handles frequency
    IsrSoftware     // ISR-based software PWM
};

constexpr u8 MAX_PWM_CHANNELS = 8;
constexpr u32 ISR_FREQUENCY_HZ = 128000;  // 128 kHz for 8-bit @ 500 Hz
constexpr u32 MAX_ISR_PWM_FREQUENCY = 500;

struct PwmPinState {
    int pin;                // GPIO pin (-1 = unused)
    u32 frequency_hz;       // Configured PWM frequency
    PwmBackend backend;     // Which backend handles this pin
    u8 duty_cycle;          // 0-255 (ISR backend duty)
    // ISR-only fields:
    u16 period_ticks;       // ISR ticks per PWM period
    u16 high_ticks;         // Ticks to stay HIGH
    u16 tick_counter;       // Current tick (0 to period_ticks-1)
    bool pin_state;         // Current GPIO state (ISR only)

    PwmPinState() : pin(-1), frequency_hz(0), backend(PwmBackend::None),
        duty_cycle(0), period_ticks(0), high_ticks(0),
        tick_counter(0), pin_state(false) {}
};

// Singleton state container for PWM management
struct PwmStateData {
    PwmPinState channels[MAX_PWM_CHANNELS];
    fl::isr::isr_handle_t isr_handle;
    bool isr_active;

    PwmStateData() : isr_active(false) {}
};

// Access singleton state
inline PwmStateData& state() {
    return fl::Singleton<PwmStateData>::instance();
}

// ISR handler — services only ISR-backend entries
void FL_IRAM pwm_isr_handler(void* user_data) {
    (void)user_data;
    PwmStateData& st = state();

    for (u8 i = 0; i < MAX_PWM_CHANNELS; i++) {
        PwmPinState& ch = st.channels[i];
        if (ch.pin < 0 || ch.backend != PwmBackend::IsrSoftware) continue;

        ch.tick_counter++;

        // Turn LOW at duty cycle boundary
        if (ch.tick_counter == ch.high_ticks && ch.pin_state) {
            fl::digitalWrite(ch.pin, fl::PinValue::Low);
            ch.pin_state = false;
        }
        // Start new period: reset counter, turn HIGH if duty > 0
        else if (ch.tick_counter >= ch.period_ticks) {
            ch.tick_counter = 0;
            if (ch.high_ticks > 0) {
                fl::digitalWrite(ch.pin, fl::PinValue::High);
                ch.pin_state = true;
            }
        }
    }
}

// Find channel by pin number
PwmPinState* findByPin(int pin) {
    PwmStateData& st = state();
    for (u8 i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (st.channels[i].pin == pin) {
            return &st.channels[i];
        }
    }
    return nullptr;
}

// Allocate a free channel
PwmPinState* allocate() {
    PwmStateData& st = state();
    for (u8 i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (st.channels[i].pin < 0) {
            return &st.channels[i];
        }
    }
    return nullptr;
}

// Count active ISR-backend channels
u8 countIsrChannels() {
    PwmStateData& st = state();
    u8 count = 0;
    for (u8 i = 0; i < MAX_PWM_CHANNELS; i++) {
        if (st.channels[i].pin >= 0 && st.channels[i].backend == PwmBackend::IsrSoftware) {
            count++;
        }
    }
    return count;
}

// Ensure ISR timer is running (lazy init)
int ensureIsrActive() {
    PwmStateData& st = state();
    if (st.isr_active) return 0;

    fl::isr::isr_config_t cfg;
    cfg.handler = pwm_isr_handler;
    cfg.frequency_hz = ISR_FREQUENCY_HZ;
    cfg.priority = fl::isr::ISR_PRIORITY_MEDIUM;
    cfg.flags = fl::isr::ISR_FLAG_IRAM_SAFE;

    int result = fl::isr::attachTimerHandler(cfg, &st.isr_handle);
    if (result != 0) {
        FL_WARN("PWM: ISR attach failed: " << fl::isr::getErrorString(result));
        return result;
    }
    st.isr_active = true;
    return 0;
}

// Shutdown ISR if no ISR-backend channels remain
void maybeShutdownIsr() {
    PwmStateData& st = state();
    if (!st.isr_active) return;
    if (countIsrChannels() > 0) return;

    fl::isr::detachHandler(st.isr_handle);
    st.isr_active = false;
}

// Release a channel and cleanup
void releaseChannel(PwmPinState* ch) {
    if (!ch || ch->pin < 0) return;

    fl::digitalWrite(ch->pin, fl::PinValue::Low);

    {
        fl::isr::CriticalSection cs;
        ch->pin = -1;
        ch->backend = PwmBackend::None;
        ch->frequency_hz = 0;
        ch->duty_cycle = 0;
    }

    maybeShutdownIsr();
}

}  // namespace pwm_state

// ============================================================================
// analogWrite / setPwm16 — route through PWM state when configured
// ============================================================================

void analogWrite(int pin, u16 val) {
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);

    if (ch && ch->backend == pwm_state::PwmBackend::IsrSoftware) {
        // Route to ISR duty cycle update (scale 8-bit val to duty)
        u8 duty = (val > 255) ? 255 : static_cast<u8>(val);
        fl::isr::CriticalSection cs;
        ch->duty_cycle = duty;
        ch->high_ticks = (static_cast<u32>(ch->period_ticks) * duty) / 256;
        return;
    }

    if (ch && ch->backend == pwm_state::PwmBackend::Native) {
        // Native backend — forward to platform (frequency already configured)
        platforms::analogWrite(pin, val);
        return;
    }

    // No setPwmFrequency called — forward to platform as before
    platforms::analogWrite(pin, val);
}

void setPwm16(int pin, u16 val) {
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);

    if (ch && ch->backend == pwm_state::PwmBackend::IsrSoftware) {
        // Route to ISR duty cycle update (scale 16-bit to 8-bit)
        u8 duty = static_cast<u8>(val >> 8);
        fl::isr::CriticalSection cs;
        ch->duty_cycle = duty;
        ch->high_ticks = (static_cast<u32>(ch->period_ticks) * duty) / 256;
        return;
    }

    if (ch && ch->backend == pwm_state::PwmBackend::Native) {
        platforms::setPwm16(pin, val);
        return;
    }

    // No setPwmFrequency called — forward to platform as before
    platforms::setPwm16(pin, val);
}

// ============================================================================
// PWM Frequency API
// ============================================================================

int setPwmFrequency(int pin, u32 frequency_hz) {
    // Check if pin already has a channel
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);

    if (ch) {
        // Pin already configured — release and reconfigure
        pwm_state::releaseChannel(ch);
        ch = nullptr;
    }

    // Validate frequency
    if (frequency_hz == 0) {
        FL_WARN("setPwmFrequency: Frequency must be > 0");
        return -1;
    }

    // Query platform: can it handle this natively?
    bool needs_isr = platforms::needsPwmIsrFallback(pin, frequency_hz);

    if (!needs_isr) {
        // Native path
        int result = platforms::setPwmFrequencyNative(pin, frequency_hz);
        if (result != 0) {
            FL_WARN("setPwmFrequency: Native backend failed: " << result);
            return result;
        }

        // Allocate tracking slot
        ch = pwm_state::allocate();
        if (!ch) {
            FL_WARN("setPwmFrequency: All " << static_cast<int>(pwm_state::MAX_PWM_CHANNELS) << " channels in use");
            return -2;
        }

        ch->pin = pin;
        ch->frequency_hz = frequency_hz;
        ch->backend = pwm_state::PwmBackend::Native;
        ch->duty_cycle = 0;
        return 0;
    }

    // ISR software fallback path
    if (frequency_hz > pwm_state::MAX_ISR_PWM_FREQUENCY) {
        FL_WARN("setPwmFrequency: ISR fallback max " << pwm_state::MAX_ISR_PWM_FREQUENCY << " Hz, requested " << frequency_hz);
        return -1;
    }

    // Allocate channel
    ch = pwm_state::allocate();
    if (!ch) {
        FL_WARN("setPwmFrequency: All " << static_cast<int>(pwm_state::MAX_PWM_CHANNELS) << " channels in use");
        return -2;
    }

    // Ensure ISR is running
    int isr_result = pwm_state::ensureIsrActive();
    if (isr_result != 0) {
        return -3;
    }

    // Configure GPIO
    fl::pinMode(pin, fl::PinMode::Output);
    fl::digitalWrite(pin, fl::PinValue::Low);

    // Initialize channel (atomic)
    {
        fl::isr::CriticalSection cs;
        ch->pin = pin;
        ch->frequency_hz = frequency_hz;
        ch->backend = pwm_state::PwmBackend::IsrSoftware;
        ch->period_ticks = pwm_state::ISR_FREQUENCY_HZ / frequency_hz;
        ch->duty_cycle = 0;
        ch->high_ticks = 0;
        ch->tick_counter = 0;
        ch->pin_state = false;
    }

    return 0;
}

u32 getPwmFrequency(int pin) {
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);
    if (ch) {
        return ch->frequency_hz;
    }

    // Not in our state — ask platform (may have been set externally)
    return platforms::getPwmFrequencyNative(pin);
}

int pwmEnd(int pin) {
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);
    if (!ch) {
        return -1;
    }

    pwm_state::releaseChannel(ch);
    return 0;
}

// ============================================================================
// pinMode - releases PWM when pin mode changes
// ============================================================================

void pinMode(int pin, PinMode mode) {
    // Release any active PWM channel on this pin
    // When pinMode is called, the pin's function is being changed,
    // so any existing PWM configuration should be cleared
    pwm_state::PwmPinState* ch = pwm_state::findByPin(pin);
    if (ch) {
        pwm_state::releaseChannel(ch);
    }

    platforms::pinMode(pin, mode);
}

}  // namespace fl
