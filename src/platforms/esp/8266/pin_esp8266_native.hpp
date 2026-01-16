#pragma once

/// @file platforms/esp/8266/pin_esp8266_native.hpp
/// ESP8266 SDK native GPIO implementation
///
/// Provides pin control functions using ESP8266 SDK GPIO register APIs.
/// This file is used when building without Arduino framework.
///
/// Implementation based on ESP8266 Arduino core's wiring_digital.cpp:
/// https://github.com/esp8266/Arduino/blob/master/cores/esp8266/core_esp8266_wiring_digital.cpp

#include "fl/compiler_control.h"
#include "fl/pin.h"

FL_EXTERN_C_BEGIN
// ESP8266 SDK peripheral register definitions
extern "C" {
    #include "esp8266_peri.h"
    #include "osapi.h"
    // ADC function from ESP8266 SDK (user_interface.h)
    uint16_t system_adc_read(void);
}
FL_EXTERN_C_END

namespace fl {
namespace platform {

// ============================================================================
// GPIO Register Access Macros (ESP8266)
// ============================================================================

// Base register address for GPIO
#define ESP8266_REG(addr) (*reinterpret_cast<volatile uint32_t*>(0x60000000 + (addr)))  // ok reinterpret cast

// GPIO (pins 0-15) registers
#define GPO      ESP8266_REG(0x300)  // Output level
#define GPOS     ESP8266_REG(0x304)  // Output set (write 1 to set)
#define GPOC     ESP8266_REG(0x308)  // Output clear (write 1 to clear)
#define GPE      ESP8266_REG(0x30C)  // Output enable
#define GPES     ESP8266_REG(0x310)  // Enable set (write 1 to enable output)
#define GPEC     ESP8266_REG(0x314)  // Enable clear (write 1 to disable output)
#define GPI      ESP8266_REG(0x318)  // Input level (read-only)

// GPIO pin configuration register (one per pin)
#define GPC(p)   ESP8266_REG(0x328 + ((p & 0xF) * 4))

// GPIO pin function register (one per pin)
#define GPF(p)   ESP8266_REG(0x800 + ((p & 0xF) * 4))

// GPIO pin input read macro
#define GPIP(p)  ((GPI >> (p)) & 1)

// GPIO 16 special registers (separate from 0-15)
#define GP16O    ESP8266_REG(0x768)  // GPIO16 output
#define GP16E    ESP8266_REG(0x774)  // GPIO16 enable
#define GP16I    ESP8266_REG(0x78C)  // GPIO16 input
#define GPC16    ESP8266_REG(0x790)  // GPIO16 control
#define GPF16    ESP8266_REG(0x7A0)  // GPIO16 function

// Pin control register bit positions
#define GPCI     7   // Interrupt configuration (bits 7-10)
#define GPCD     2   // Open-drain mode (1 = open drain, 0 = normal)

// Pin function register bit positions
#define GPFPU    7   // Pull-up enable (1 = enable)
#define GPFPD    6   // Pull-down enable (1 = enable)
#define GPFFS    4   // Function select bits (4-6)

// GPIO function select helper - sets function select bits to GPIO mode
#define GPFFS_GPIO(p)  (((p) < 16) ? 0 : 0)  // Function 0 = GPIO for pins 0-15
#define GP16FFS(v)     ((v) << 0)  // GPIO16 function select
#define GP16FPD        6   // GPIO16 pull-down bit

// ============================================================================
// Digital Pin Functions
// ============================================================================

inline void pinMode(int pin, PinMode mode) {
    if (pin < 0) {
        return;  // Invalid pin
    }

    int mode_int = static_cast<int>(mode);

    if (pin < 16) {
        // Configure pins 0-15
        switch (mode_int) {
            case 0:  // Input
                // Set GPIO function (function select = 0)
                GPF(pin) = 0;
                // Disable output (write 1 to bit position to disable)
                GPEC = (1 << pin);
                // Configure as input with open-drain disabled, no pull resistors
                GPC(pin) = (GPC(pin) & ~(0xF << GPCI)) | (1 << GPCD);
                break;

            case 1:  // Output
                // Set GPIO function (function select = 0)
                GPF(pin) = 0;
                // Clear open-drain and interrupt config
                GPC(pin) = (GPC(pin) & ~((0xF << GPCI) | (1 << GPCD)));
                // Enable output (write 1 to bit position to enable)
                GPES = (1 << pin);
                break;

            case 2:  // InputPullup
                // Set GPIO function with pull-up enabled
                GPF(pin) = (1 << GPFPU);
                // Disable output
                GPEC = (1 << pin);
                // Configure as input with open-drain enabled (required for pull-up)
                GPC(pin) = (GPC(pin) & ~(0xF << GPCI)) | (1 << GPCD);
                break;

            default:
                break;  // Unknown mode, do nothing
        }
    } else if (pin == 16) {
        // GPIO16 has separate registers and different control
        switch (mode_int) {
            case 0:  // Input
                // Set GPIO function for GPIO16
                GPF16 = GP16FFS(0);
                // Clear control register
                GPC16 = 0;
                // Disable output
                GP16E &= ~1;
                break;

            case 1:  // Output
                // Set GPIO function
                GPF16 = GP16FFS(0);
                // Clear control register
                GPC16 = 0;
                // Enable output
                GP16E |= 1;
                break;

            case 3:  // InputPulldown (pin 16 only)
                // Set GPIO function with pull-down enabled
                GPF16 = GP16FFS(0) | (1 << GP16FPD);
                // Clear control register
                GPC16 = 0;
                // Disable output
                GP16E &= ~1;
                break;

            default:
                break;  // Unknown mode
        }
    }
    // Pins > 16 are invalid, silently ignore
}

inline void digitalWrite(int pin, PinValue val) {
    if (pin < 0) {
        return;  // Invalid pin
    }

    int val_int = static_cast<int>(val);

    if (pin < 16) {
        // For pins 0-15: use set/clear registers for atomic operation
        if (val_int) {
            GPOS = (1 << pin);  // Set output high
        } else {
            GPOC = (1 << pin);  // Set output low
        }
    } else if (pin == 16) {
        // GPIO16 uses direct register manipulation
        if (val_int) {
            GP16O |= 1;   // Set high
        } else {
            GP16O &= ~1;  // Set low
        }
    }
    // Pins > 16 are invalid, silently ignore
}

inline PinValue digitalRead(int pin) {
    if (pin < 0) {
        return PinValue::Low;  // Invalid pin
    }

    if (pin < 16) {
        // Read from input register (bit shift and mask)
        return GPIP(pin) ? PinValue::High : PinValue::Low;
    } else if (pin == 16) {
        // GPIO16 input register
        return (GP16I & 0x01) ? PinValue::High : PinValue::Low;
    }

    return PinValue::Low;  // Invalid pin
}

// ============================================================================
// Analog Pin Functions
// ============================================================================
// Note: ESP8266 has only one ADC pin (A0/TOUT), and no true analog output.
// ADC input range: 0-1.0V (some development boards scale 0-3.3V externally)
// ADC resolution: 10-bit (0-1023)

// A0 pin constant (matches Arduino ESP8266 core definition)
#define A0 17

inline uint16_t analogRead(int pin) {
    // ESP8266 has only one ADC on A0/TOUT pin (pin 17 or 0)
    // Pin 17 is the standard A0 constant, pin 0 is accepted for compatibility
    if (pin == 17 || pin == 0) {
        // Read ADC value using ESP8266 SDK function
        // Returns 10-bit value (0-1023) representing 0-1.0V
        return system_adc_read();
    }

    // For non-ADC pins, return digital read scaled to ADC range
    // This matches Arduino ESP8266 core behavior
    return static_cast<uint16_t>(platform::digitalRead(pin) == PinValue::High ? 1023 : 0);
}

inline void analogWrite(int pin, uint16_t val) {
    // ESP8266 does not have true analog output (no DAC)
    // The Arduino core implements PWM via software waveform generation using
    // TIMER1 and GPIO manipulation, which is complex and requires:
    // - Timer configuration and interrupt handling
    // - Phase-aligned multi-channel PWM state management
    // - Waveform generation with cycle-accurate timing
    //
    // For FastLED's use case, this complexity is not needed. Instead, we provide
    // a simple digital fallback that handles common cases:
    // - val == 0: digitalWrite(pin, LOW)   (fully off)
    // - val >= 255: digitalWrite(pin, HIGH) (fully on)
    // - 0 < val < 255: no-op (partial PWM not supported)
    //
    // Applications requiring true PWM should use the Arduino ESP8266 core
    // or implement custom timer-based PWM.

    if (pin < 0 || pin > 16) {
        return;  // Invalid pin
    }

    if (val == 0) {
        // Fully off
        platform::digitalWrite(pin, PinValue::Low);
    } else if (val >= 255) {
        // Fully on (assuming 8-bit PWM range 0-255)
        platform::digitalWrite(pin, PinValue::High);
    }
    // For intermediate values (0 < val < 255), do nothing
    // True PWM would require timer-based implementation
}

inline void setPwm16(int pin, uint16_t val) {
    // ESP8266 hardware supports up to 14-bit PWM via TIMER1
    // However, implementing true PWM requires complex timer configuration
    // Provide digital fallback similar to analogWrite
    if (pin < 0 || pin > 16) {
        return;  // Invalid pin
    }

    // Scale 16-bit to midpoint for on/off decision
    if (val == 0) {
        platform::digitalWrite(pin, PinValue::Low);
    } else if (val >= 65535) {
        platform::digitalWrite(pin, PinValue::High);
    } else if (val >= 32768) {
        platform::digitalWrite(pin, PinValue::High);  // Above 50% → on
    } else {
        platform::digitalWrite(pin, PinValue::Low);   // Below 50% → off
    }
    // True 16-bit PWM would require TIMER1-based implementation
}

inline void setAdcRange(AdcRange range) {
    // ESP8266 ADC reference voltage is fixed at 1.0V (internal reference)
    // This function has no effect on ESP8266 hardware - reference cannot be changed
    //
    // Arduino core compatibility: Only DEFAULT mode is supported
    // Other modes (EXTERNAL, INTERNAL, etc.) are silently ignored
    //
    // Note: Some ESP8266 development boards (e.g., NodeMCU, WeMos D1 Mini)
    // include external voltage divider circuitry to scale 0-3.3V input
    // down to the chip's 0-1.0V ADC range. This is a hardware feature
    // and cannot be controlled via software.

    (void)range;  // Parameter unused - no-op
}

}  // namespace platform
}  // namespace fl
