#pragma once

/// @file platforms/avr/pin_avr_native.hpp
/// Native AVR register-based GPIO implementation (non-Arduino path)
///
/// This file provides direct AVR register manipulation for GPIO operations
/// when building without Arduino framework. It implements the same pin functions
/// as Arduino but using native AVR register access.
///
/// Supported platforms: ATmega328P, ATmega2560, ATmega32U4, and compatible AVR MCUs

#include <avr/io.h>
#include <avr/pgmspace.h>
#include "fl/stl/stdint.h"

namespace fl {
namespace platform {

// Pin mode constants (matching Arduino API)
#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif

// Digital value constants (matching Arduino API)
#ifndef LOW
#define LOW 0x0
#endif
#ifndef HIGH
#define HIGH 0x1
#endif

// Analog reference modes (matching Arduino API)
#ifndef DEFAULT
#define DEFAULT 1
#endif
#ifndef INTERNAL
#define INTERNAL 3
#endif
#ifndef EXTERNAL
#define EXTERNAL 0
#endif

// Port identifiers
#define NOT_A_PORT 0
#define PB 2
#define PC 3
#define PD 4

// Pin mapping tables for ATmega328P (Arduino UNO/Nano pinout)
// These map Arduino pin numbers to AVR ports and bit masks
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega168__)

// Maps Arduino digital pin numbers to port identifiers (PB, PC, PD)
static const uint8_t PROGMEM digital_pin_to_port_PGM[] = {
    PD, // 0 - PORTD
    PD, // 1 - PORTD
    PD, // 2 - PORTD
    PD, // 3 - PORTD
    PD, // 4 - PORTD
    PD, // 5 - PORTD
    PD, // 6 - PORTD
    PD, // 7 - PORTD
    PB, // 8 - PORTB
    PB, // 9 - PORTB
    PB, // 10 - PORTB
    PB, // 11 - PORTB
    PB, // 12 - PORTB
    PB, // 13 - PORTB
    PC, // 14 - PORTC (A0)
    PC, // 15 - PORTC (A1)
    PC, // 16 - PORTC (A2)
    PC, // 17 - PORTC (A3)
    PC, // 18 - PORTC (A4)
    PC, // 19 - PORTC (A5)
};

// Maps Arduino digital pin numbers to bit masks
static const uint8_t PROGMEM digital_pin_to_bit_mask_PGM[] = {
    _BV(0), // 0, port D
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
    _BV(6),
    _BV(7),
    _BV(0), // 8, port B
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
    _BV(0), // 14, port C
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
};

// Maps port identifiers to DDR (Data Direction Register) addresses
static const uint16_t PROGMEM port_to_mode_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &DDRB,
    (uint16_t) &DDRC,
    (uint16_t) &DDRD,
};

// Maps port identifiers to PORT (Output Register) addresses
static const uint16_t PROGMEM port_to_output_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &PORTB,
    (uint16_t) &PORTC,
    (uint16_t) &PORTD,
};

// Maps port identifiers to PIN (Input Register) addresses
static const uint16_t PROGMEM port_to_input_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &PINB,
    (uint16_t) &PINC,
    (uint16_t) &PIND,
};

// Analog pin to ADC channel mapping (A0-A5 -> ADC0-ADC5)
static const uint8_t PROGMEM analog_pin_to_channel_PGM[] = {
    0, // A0 -> ADC0
    1, // A1 -> ADC1
    2, // A2 -> ADC2
    3, // A3 -> ADC3
    4, // A4 -> ADC4
    5, // A5 -> ADC5
    6, // A6 (internal only on some variants)
    7, // A7 (internal only on some variants)
};

#elif defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)

// ATmega2560 (Arduino MEGA) pin mapping
// Note: This is a simplified subset - full MEGA has 70 digital pins
static const uint8_t PROGMEM digital_pin_to_port_PGM[] = {
    // Pins 0-21 for basic compatibility
    // A full implementation would include all 70 pins
    4, 4, 4, 4, 4, 4, 4, 4, // 0-7: PE0-PE7 (port E)
    5, 5, 5, 5, 5, 5, 5, 5, // 8-15: PH0-PH7 (port H)
    2, 2, 2, 2, 2, 2,       // 16-21: PB0-PB5 (port B)
};

static const uint8_t PROGMEM digital_pin_to_bit_mask_PGM[] = {
    _BV(0), _BV(1), _BV(2), _BV(3), _BV(4), _BV(5), _BV(6), _BV(7), // 0-7
    _BV(0), _BV(1), _BV(2), _BV(3), _BV(4), _BV(5), _BV(6), _BV(7), // 8-15
    _BV(0), _BV(1), _BV(2), _BV(3), _BV(4), _BV(5),                 // 16-21
};

// Port register mappings for ATmega2560
static const uint16_t PROGMEM port_to_mode_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &DDRB,
    (uint16_t) &DDRC,
    (uint16_t) &DDRD,
    (uint16_t) &DDRE,
    (uint16_t) &DDRF,
    (uint16_t) &DDRG,
    (uint16_t) &DDRH,
    NOT_A_PORT, // Port I
    (uint16_t) &DDRJ,
    (uint16_t) &DDRK,
    (uint16_t) &DDRL,
};

static const uint16_t PROGMEM port_to_output_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &PORTB,
    (uint16_t) &PORTC,
    (uint16_t) &PORTD,
    (uint16_t) &PORTE,
    (uint16_t) &PORTF,
    (uint16_t) &PORTG,
    (uint16_t) &PORTH,
    NOT_A_PORT,
    (uint16_t) &PORTJ,
    (uint16_t) &PORTK,
    (uint16_t) &PORTL,
};

static const uint16_t PROGMEM port_to_input_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
    (uint16_t) &PINB,
    (uint16_t) &PINC,
    (uint16_t) &PIND,
    (uint16_t) &PINE,
    (uint16_t) &PINF,
    (uint16_t) &PING,
    (uint16_t) &PINH,
    NOT_A_PORT,
    (uint16_t) &PINJ,
    (uint16_t) &PINK,
    (uint16_t) &PINL,
};

static const uint8_t PROGMEM analog_pin_to_channel_PGM[] = {
    0, 1, 2, 3, 4, 5, 6, 7, // A0-A7
    8, 9, 10, 11, 12, 13, 14, 15, // A8-A15
};

#elif defined(__AVR_ATmega4809__) || defined(ARDUINO_AVR_NANO_EVERY)

// ATmega4809 (Arduino Nano Every) - megaAVR architecture
// This chip uses a different port register structure: PORTB.DIR instead of DDRB
// The Arduino core provides compatibility macros, but we need to ensure proper addressing

// Arduino Nano Every pin mapping (same physical layout as Nano/Uno)
static const uint8_t PROGMEM digital_pin_to_port_PGM[] = {
    PD, // 0 - PORTD
    PD, // 1 - PORTD
    PD, // 2 - PORTD (was PA in earlier revisions)
    PD, // 3 - PORTD (was PF in earlier revisions)
    PD, // 4 - PORTD (was PC in earlier revisions)
    PD, // 5 - PORTD (was PB in earlier revisions)
    PD, // 6 - PORTD (was PF in earlier revisions)
    PD, // 7 - PORTD (was PA in earlier revisions)
    PB, // 8 - PORTB (was PE in earlier revisions)
    PB, // 9 - PORTB
    PB, // 10 - PORTB
    PB, // 11 - PORTB (was PE in earlier revisions)
    PB, // 12 - PORTB (was PE in earlier revisions)
    PB, // 13 - PORTB (was PE in earlier revisions)
    PC, // 14 - PORTC (A0)
    PC, // 15 - PORTC (A1)
    PC, // 16 - PORTC (A2)
    PC, // 17 - PORTC (A3)
    PC, // 18 - PORTC (A4 / SDA)
    PC, // 19 - PORTC (A5 / SCL)
};

// Maps Arduino digital pin numbers to bit masks
static const uint8_t PROGMEM digital_pin_to_bit_mask_PGM[] = {
    _BV(0), // 0, port D
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
    _BV(6),
    _BV(7),
    _BV(0), // 8, port B
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
    _BV(0), // 14, port C
    _BV(1),
    _BV(2),
    _BV(3),
    _BV(4),
    _BV(5),
};

// For ATmega4809, we use the .DIR, .OUT, .IN members of PORT structures
// The Arduino core provides compatibility macros that map DDRB -> PORTB.DIR, etc.
static const uint16_t PROGMEM port_to_mode_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
#if defined(DDRB)
    (uint16_t) &DDRB,  // Arduino core provides compatibility macro
    (uint16_t) &DDRC,
    (uint16_t) &DDRD,
#else
    (uint16_t) &PORTB.DIR,  // Native megaAVR register access
    (uint16_t) &PORTC.DIR,
    (uint16_t) &PORTD.DIR,
#endif
};

static const uint16_t PROGMEM port_to_output_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
#if defined(PORTB)
    (uint16_t) &PORTB,  // Arduino core provides compatibility macro
    (uint16_t) &PORTC,
    (uint16_t) &PORTD,
#else
    (uint16_t) &PORTB.OUT,  // Native megaAVR register access
    (uint16_t) &PORTC.OUT,
    (uint16_t) &PORTD.OUT,
#endif
};

static const uint16_t PROGMEM port_to_input_PGM[] = {
    NOT_A_PORT,
    NOT_A_PORT,
#if defined(PINB)
    (uint16_t) &PINB,  // Arduino core provides compatibility macro
    (uint16_t) &PINC,
    (uint16_t) &PIND,
#else
    (uint16_t) &PORTB.IN,  // Native megaAVR register access
    (uint16_t) &PORTC.IN,
    (uint16_t) &PORTD.IN,
#endif
};

// Analog pin to ADC channel mapping (A0-A5 -> ADC0-ADC5)
static const uint8_t PROGMEM analog_pin_to_channel_PGM[] = {
    0, // A0 -> ADC0
    1, // A1 -> ADC1
    2, // A2 -> ADC2
    3, // A3 -> ADC3
    4, // A4 -> ADC4
    5, // A5 -> ADC5
    6, // A6 (internal only on some variants)
    7, // A7 (internal only on some variants)
};

#elif defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy2__) || \
      defined(__AVR_ATtiny1604__) || defined(__AVR_ATtiny1616__) || defined(__AVR_ATtiny3216__) || defined(__AVR_ATtiny3217__)

// megaAVR 0-series / tinyAVR 0/1/2-series (ATtiny1604, ATtiny1616, etc.)
// These use PORTA.DIR/OUT/IN style registers instead of DDRB/PORTB/PINB

// Minimal pin mapping for PORTB pins (common across tinyAVR 0/1/2-series)
static const uint8_t PROGMEM digital_pin_to_port_PGM[] = { PB, PB, PB, PB, PB, PB };
static const uint8_t PROGMEM digital_pin_to_bit_mask_PGM[] = { _BV(0), _BV(1), _BV(2), _BV(3), _BV(4), _BV(5) };

// megaAVR uses PORTB.DIR, PORTB.OUT, PORTB.IN instead of DDRB, PORTB, PINB
static const uint16_t PROGMEM port_to_mode_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &PORTB.DIR };
static const uint16_t PROGMEM port_to_output_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &PORTB.OUT };
static const uint16_t PROGMEM port_to_input_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &PORTB.IN };
static const uint8_t PROGMEM analog_pin_to_channel_PGM[] = { 0, 1, 2, 3 };

#else
// Fallback for other classic AVR variants (minimal implementation)
// Uses traditional DDRx/PORTx/PINx registers
static const uint8_t PROGMEM digital_pin_to_port_PGM[] = { PB, PB, PB, PB, PB, PB };
static const uint8_t PROGMEM digital_pin_to_bit_mask_PGM[] = { _BV(0), _BV(1), _BV(2), _BV(3), _BV(4), _BV(5) };
static const uint16_t PROGMEM port_to_mode_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &DDRB };
static const uint16_t PROGMEM port_to_output_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &PORTB };
static const uint16_t PROGMEM port_to_input_PGM[] = { NOT_A_PORT, NOT_A_PORT, (uint16_t) &PINB };
static const uint8_t PROGMEM analog_pin_to_channel_PGM[] = { 0, 1, 2, 3 };
#endif

// Analog reference storage (set by analogReference(), used by analogRead())
static uint8_t analog_reference = DEFAULT;

// Helper macros to read from PROGMEM tables
#define digitalPinToPort(P) ( pgm_read_byte( digital_pin_to_port_PGM + (P) ) )
#define digitalPinToBitMask(P) ( pgm_read_byte( digital_pin_to_bit_mask_PGM + (P) ) )
#define portModeRegister(P) ( (volatile uint8_t *)( pgm_read_word( port_to_mode_PGM + (P) ) ) )
#define portOutputRegister(P) ( (volatile uint8_t *)( pgm_read_word( port_to_output_PGM + (P) ) ) )
#define portInputRegister(P) ( (volatile uint8_t *)( pgm_read_word( port_to_input_PGM + (P) ) ) )
#define analogPinToChannel(P) ( pgm_read_byte( analog_pin_to_channel_PGM + (P) ) )

// ============================================================================
// GPIO Functions - Native AVR Implementation
// ============================================================================

inline void pinMode(int pin, PinMode mode) {
    uint8_t port = digitalPinToPort(pin);
    if (port == NOT_A_PORT) return;

    uint8_t bit_mask = digitalPinToBitMask(pin);
    volatile uint8_t *ddr = portModeRegister(port);
    volatile uint8_t *port_reg = portOutputRegister(port);

    // Save interrupt state and disable interrupts for atomic operation
    uint8_t oldSREG = SREG;
    __builtin_avr_cli();

    // Translate PinMode to internal constants
    // PinMode: Input=0, Output=1, InputPullup=2, InputPulldown=3
    int mode_val = static_cast<int>(mode);

    if (mode_val == 0) {  // Input
        *ddr &= ~bit_mask;      // Set pin as input
        *port_reg &= ~bit_mask; // Disable pull-up
    } else if (mode_val == 2) {  // InputPullup
        *ddr &= ~bit_mask;      // Set pin as input
        *port_reg |= bit_mask;  // Enable pull-up
    } else if (mode_val == 3) {  // InputPulldown (not supported on AVR)
        *ddr &= ~bit_mask;      // Treat as Input
        *port_reg &= ~bit_mask; // Disable pull-up
    } else {  // Output
        *ddr |= bit_mask;       // Set pin as output
    }

    // Restore interrupt state
    SREG = oldSREG;
}

inline void digitalWrite(int pin, PinValue val) {
    uint8_t port = digitalPinToPort(pin);
    if (port == NOT_A_PORT) return;

    uint8_t bit_mask = digitalPinToBitMask(pin);
    volatile uint8_t *port_reg = portOutputRegister(port);

    // Save interrupt state and disable interrupts for atomic operation
    uint8_t oldSREG = SREG;
    __builtin_avr_cli();

    // Translate PinValue (Low=0, High=1) to register value
    if (static_cast<int>(val) == 0) {  // Low
        *port_reg &= ~bit_mask; // Clear bit (set to LOW)
    } else {  // High
        *port_reg |= bit_mask;  // Set bit (set to HIGH)
    }

    // Restore interrupt state
    SREG = oldSREG;
}

inline PinValue digitalRead(int pin) {
    uint8_t port = digitalPinToPort(pin);
    if (port == NOT_A_PORT) return PinValue::Low;

    uint8_t bit_mask = digitalPinToBitMask(pin);
    volatile uint8_t *pin_reg = portInputRegister(port);

    // Read pin state (no interrupt protection needed for read)
    if (*pin_reg & bit_mask) {
        return PinValue::High;
    }
    return PinValue::Low;
}

inline uint16_t analogRead(int pin) {
#if defined(ADCSRA) && defined(ADMUX)
    // Convert analog pin number to ADC channel
    uint8_t channel = analogPinToChannel(pin);

    // Set analog reference and select ADC channel
    // Clear previous channel selection, keep reference bits
    ADMUX = (analog_reference << 6) | (channel & 0x0F);

    // Start conversion
    ADCSRA |= _BV(ADSC);

    // Wait for conversion to complete
    while (ADCSRA & _BV(ADSC)) {
        // Busy wait
    }

    // Read ADC result (must read ADCL first, then ADCH)
    uint8_t low = ADCL;
    uint8_t high = ADCH;

    return static_cast<uint16_t>((high << 8) | low);
#else
    // ADC not available on this variant
    (void)pin;
    return 0;
#endif
}

inline void analogWrite(int pin, uint16_t val) {
    // Simplified PWM implementation
    // Full implementation would require timer configuration per pin
    // For now, treat as digital output for edge cases
    if (val == 0) {
        platform::digitalWrite(pin, PinValue::Low);
    } else if (val >= 255) {
        platform::digitalWrite(pin, PinValue::High);
    } else {
        // PWM requires timer setup which is platform-specific
        // This is a basic no-op for intermediate values
        // A full implementation would configure OCRnx registers
        platform::pinMode(pin, PinMode::Output);

        // Placeholder: In a real implementation, this would:
        // 1. Determine which timer channel controls this pin
        // 2. Configure the appropriate TCCRnx registers for Fast PWM
        // 3. Set OCRnx register to 'val' for duty cycle
        // 4. Enable PWM output on the pin

        // For now, just set digital high if val > 127
        platform::digitalWrite(pin, val > 127 ? PinValue::High : PinValue::Low);
    }
}

inline void setPwm16(int pin, uint16_t val) {
    // AVR 16-bit PWM using Timer1 (pins 9, 10 on Uno)
    // Provides true 16-bit resolution at ~244 Hz (16 MHz / 65536)

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega168__)
    // ATmega328P (Arduino Uno/Nano): Timer1 controls pins 9 (OC1A) and 10 (OC1B)
    if (pin == 9 || pin == 10) {
        // Set pins as output
        platform::pinMode(pin, PinMode::Output);

        // Configure Timer1 for Fast PWM mode 14 (ICR1 = TOP)
        // WGM13:0 = 1110 (Fast PWM, TOP = ICR1)
        // COM1A1:0 = 10 (Clear OC1A on compare match, set at BOTTOM)
        // COM1B1:0 = 10 (Clear OC1B on compare match, set at BOTTOM)
        // CS12:0 = 001 (No prescaler, 16 MHz timer clock)

        // Set ICR1 to maximum for 16-bit resolution
        ICR1 = 0xFFFF;  // TOP = 65535

        // Configure waveform generation mode and prescaler
        TCCR1A = _BV(WGM11);  // WGM11 = 1, WGM10 = 0
        TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);  // WGM13 = 1, WGM12 = 1, CS10 = 1 (no prescaler)

        // Set compare output mode and duty cycle
        if (pin == 9) {
            // Pin 9 = OC1A (OCR1A)
            TCCR1A |= _BV(COM1A1);  // Clear OC1A on compare match
            OCR1A = val;
        } else {
            // Pin 10 = OC1B (OCR1B)
            TCCR1A |= _BV(COM1B1);  // Clear OC1B on compare match
            OCR1B = val;
        }

        return;
    }
#elif defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
    // ATmega2560 (Arduino Mega): Timer1 controls pins 11 (OC1A) and 12 (OC1B)
    if (pin == 11 || pin == 12) {
        platform::pinMode(pin, PinMode::Output);

        ICR1 = 0xFFFF;
        TCCR1A = _BV(WGM11);
        TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);

        if (pin == 11) {
            TCCR1A |= _BV(COM1A1);
            OCR1A = val;
        } else {
            TCCR1A |= _BV(COM1B1);
            OCR1B = val;
        }

        return;
    }
#endif

    // Fallback for non-Timer1 pins: scale 16-bit to 8-bit
    platform::analogWrite(pin, val >> 8);
}

inline void setAdcRange(AdcRange range) {
    // Translate AdcRange to AVR analogReference constants
    // AdcRange: Default=0, Range0_1V1=1, Range0_1V5=2, Range0_2V2=3, Range0_3V3=4, Range0_5V=5, External=6
    // AVR constants: DEFAULT=1, INTERNAL=3, EXTERNAL=0
    uint8_t ref_mode;
    switch (range) {
        case AdcRange::Default:
            ref_mode = DEFAULT;  // 1 (5V on 5V boards)
            break;
        case AdcRange::Range0_1V1:
            ref_mode = INTERNAL;  // 3 (1.1V internal reference)
            break;
        case AdcRange::External:
            ref_mode = EXTERNAL;  // 0 (AREF pin)
            break;
        case AdcRange::Range0_5V:
            ref_mode = DEFAULT;  // 1 (same as Default for 5V AVR boards)
            break;
        default:
            // Other ranges not supported on AVR (no-op)
            return;
    }

    // Store reference mode for use in analogRead()
    // Don't set ADMUX here to avoid shorting AVCC and AREF
    analog_reference = ref_mode;
}

}  // namespace platform
}  // namespace fl
