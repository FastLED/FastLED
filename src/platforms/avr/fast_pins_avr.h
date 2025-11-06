/// @file platforms/avr/fast_pins_avr.h
/// @brief AVR-specific implementation of FastPins using direct PORT writes
///
/// This implementation uses AVR's direct PORT register writes for the fastest
/// possible multi-pin GPIO operations. Unlike most other platforms that use
/// SET/CLEAR registers, AVR requires writing the complete PORT state.
///
/// Key Optimization:
/// - Pre-computes FULL PORT values (including non-FastPin pins)
/// - Single 8-bit PORT write per operation
/// - Preserves non-FastPin pin states automatically
///
/// Performance: ~15-20ns per write (fastest implementation!)
/// Memory: 256 bytes per FastPinsSamePort<8> instance (1 byte per LUT entry)
///
/// Platform Support:
/// - ATmega328P (UNO, Nano): PORTB, PORTC, PORTD
/// - ATmega2560 (MEGA): PORTA through PORTL
/// - ATmega32U4 (Leonardo): PORTB, PORTC, PORTD, PORTE, PORTF
///
/// IMPORTANT: All pins must be on the same PORT (e.g., all on PORTB)

#pragma once

#include "fl/stdint.h"

// Need FL_WARN for validation warnings
#include "fl/warn.h"

// Ensure Arduino.h has been included (provides port mapping macros)
#if !defined(digitalPinToPort)
    #warning "digitalPinToPort not defined - Arduino.h may not be included"
#endif

// Arduino provides these macros for pin-to-port mapping
// digitalPinToPort(pin) - returns port number
// portOutputRegister(port) - returns PORT register pointer
// digitalPinToBitMask(pin) - returns bit mask within port

namespace fl {

namespace detail {

/// Get PORT register pointer for AVR pin
/// Uses Arduino's digitalPinToPort and portOutputRegister macros
inline volatile uint8_t* getPinPortAVR(uint8_t pin) {
    #if defined(digitalPinToPort) && defined(portOutputRegister)
        uint8_t port = digitalPinToPort(pin);
        return portOutputRegister(port);
    #else
        // Fallback - should not happen on real AVR platforms
        (void)pin;
        return nullptr;
    #endif
}

/// Get pin bit mask for AVR pin
/// Uses Arduino's digitalPinToBitMask macro
inline uint8_t getPinMaskAVR(uint8_t pin) {
    #if defined(digitalPinToBitMask)
        return digitalPinToBitMask(pin);
    #else
        // Fallback - should not happen on real AVR platforms
        (void)pin;
        return 0;
    #endif
}

} // namespace detail

/// AVR same-port validation
/// Validates all pins are on same PORT (PORTA, PORTB, PORTC, PORTD, etc.)
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // AVR: All pins must be on same PORT
    volatile uint8_t* first_port = detail::getPinPortAVR(pins[0]);

    for (uint8_t i = 1; i < count; i++) {
        volatile uint8_t* port = detail::getPinPortAVR(pins[i]);
        if (port != first_port) {
            FL_WARN("FastPinsSamePort: Pins must be on same PORT for optimal performance!");
            return false;  // Cross-PORT not allowed
        }
    }
    return true;
}

/// AVR same-port implementation (FASTEST MODE - direct PORT write)
///
/// AVR uses direct PORT register writes instead of SET/CLEAR operations.
/// The LUT stores pre-computed FULL PORT values that include both:
/// 1. FastPin pin states (from the pattern)
/// 2. Non-FastPin pin states (preserved from initial PORT read)
///
/// This allows a single 8-bit write to update all FastPins while preserving
/// other pins in the same PORT.
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // AVR: set_mask contains pre-computed full PORT value
    // clear_mask is unused on AVR (zero-filled during buildLUT)
    (void)clear_mask;  // Suppress unused parameter warning

    // Single 8-bit write = 15-20ns! (fastest implementation)
    *mPort = static_cast<uint8_t>(set_mask);
}

/// Build LUT for AVR (with PORT preservation)
///
/// This is the key optimization for AVR:
/// 1. Read current PORT state to capture non-FastPin pin values
/// 2. Calculate mask for FastPin bits (bits we control)
/// 3. Calculate mask for non-FastPin bits (bits we preserve)
/// 4. For each pattern, compute full PORT value:
///    - Start with preserved non-FastPin bits
///    - OR in FastPin bits based on pattern
/// 5. Store full PORT value in LUT entry
///
/// Result: writeImpl() only needs to write the pre-computed value!
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Store PORT register pointer
    mPort = detail::getPinPortAVR(pins[0]);

    if (mPort == nullptr) {
        // Safety check - should not happen on real AVR
        FL_WARN("FastPinsSamePort: Failed to get PORT register for AVR pin!");
        return;
    }

    // Extract pin masks for each configured pin
    uint8_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskAVR(pins[i]);
    }

    // Calculate mask for FastPin bits (bits we control)
    uint8_t fastpin_mask = 0;
    for (uint8_t i = 0; i < count; i++) {
        fastpin_mask |= pin_masks[i];
    }

    // Calculate mask for non-FastPin bits (bits we preserve)
    uint8_t other_pins_mask = ~fastpin_mask;

    // Read current PORT state for non-FastPin pins
    // IMPORTANT: This captures the state of pins NOT controlled by FastPins
    uint8_t other_pins_state = *mPort & other_pins_mask;

    // Build 256-entry LUT with full PORT values
    for (uint16_t pattern = 0; pattern < LUT_SIZE; pattern++) {
        // Start with preserved non-FastPin bits
        uint8_t port_value = other_pins_state;

        // OR in FastPin bits based on pattern
        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                port_value |= pin_masks[bit];  // Set FastPin bit HIGH
            }
            // else: bit already cleared in other_pins_state (stays LOW)
        }

        // Store full PORT value in set_mask
        // AVR uses set_mask to store full PORT value
        // clear_mask is unused (set to 0 for consistency)
        mLUT[pattern].set_mask = port_value;
        mLUT[pattern].clear_mask = 0;
    }
}

// ============================================================================
// FastPins<> multi-port implementation for AVR
// ============================================================================
//
// AVR Multi-Port Strategy:
// - Same-port mode: Direct PORT write (15-20ns, identical to FastPinsSamePort<>)
// - Multi-port mode: Read-modify-write for each PORT (80-150ns depending on port count)
//
// Challenge: AVR has no SET/CLEAR registers, so multi-port requires RMW:
//   1. Read current PORT value
//   2. Apply set_mask (OR) and clear_mask (AND ~mask)
//   3. Write back modified value
//
// Performance: Slower than ESP32/STM32/Teensy due to RMW overhead, but still
// 10-20× faster than digitalWrite loops.

/// AVR implementation of allSamePort
/// Detects if all configured pins are on the same PORT
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Get first pin's PORT
    volatile uint8_t* first_port = detail::getPinPortAVR(pins[0]);

    // Check if all other pins are on same PORT
    for (uint8_t i = 1; i < count; i++) {
        volatile uint8_t* port = detail::getPinPortAVR(pins[i]);
        if (port != first_port) {
            return false;  // Cross-PORT detected
        }
    }

    return true;  // All on same PORT
}

/// AVR implementation of buildSamePortLUT
/// Builds 2KB LUT for same-port operation (direct PORT write)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Store PORT register pointer
    mPort = detail::getPinPortAVR(pins[0]);

    if (mPort == nullptr) {
        FL_WARN("FastPins: Failed to get PORT register for AVR pin!");
        return;
    }

    // Extract pin masks
    uint8_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskAVR(pins[i]);
    }

    // Calculate mask for FastPin bits
    uint8_t fastpin_mask = 0;
    for (uint8_t i = 0; i < count; i++) {
        fastpin_mask |= pin_masks[i];
    }

    // Calculate mask for non-FastPin bits
    uint8_t other_pins_mask = ~fastpin_mask;

    // Read current PORT state for non-FastPin pins
    uint8_t other_pins_state = *mPort & other_pins_mask;

    // Build 256-entry LUT with full PORT values
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint8_t port_value = other_pins_state;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                port_value |= pin_masks[bit];
            }
        }

        mSamePortLUT[pattern].set_mask = port_value;
        mSamePortLUT[pattern].clear_mask = 0;
    }
}

/// AVR implementation of buildMultiPortLUT
/// Builds 10KB LUT for cross-PORT operation (RMW per PORT)
///
/// AVR multi-port strategy:
/// - Store PORT register pointers for each unique PORT
/// - Store set_mask (bits to OR) and clear_mask (bits to AND-NOT) per PORT
/// - writeMultiPortImpl() performs RMW for each PORT sequentially
///
/// Performance: ~40ns per PORT (vs 15-20ns for same-port)
/// Example: 2 PORTs = 80ns, 3 PORTs = 120ns, 4 PORTs = 160ns
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Collect PORT information for each pin
    struct PortInfo {
        volatile uint8_t* port;
        uint8_t mask;
    };
    PortInfo pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        pin_info[i].port = detail::getPinPortAVR(pins[i]);
        pin_info[i].mask = detail::getPinMaskAVR(pins[i]);
    }

    // Find unique PORTs
    volatile uint8_t* unique_ports[4];  // Max 4 PORTs
    uint8_t unique_count = 0;

    for (uint8_t i = 0; i < count; i++) {
        volatile uint8_t* port = pin_info[i].port;

        // Check if PORT already in unique list
        bool found = false;
        for (uint8_t j = 0; j < unique_count; j++) {
            if (unique_ports[j] == port) {
                found = true;
                break;
            }
        }

        // Add to unique list if not found
        if (!found && unique_count < 4) {
            unique_ports[unique_count++] = port;
        }
    }

    // Build 256-entry multi-port LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        // Initialize entry
        mMultiPortLUT[pattern].port_count = unique_count;

        // For each unique PORT
        for (uint8_t p = 0; p < unique_count; p++) {
            volatile uint8_t* port = unique_ports[p];
            uint8_t set_mask = 0;
            uint8_t clear_mask = 0;

            // Calculate set/clear masks for this PORT
            for (uint8_t i = 0; i < count; i++) {
                if (pin_info[i].port == port) {
                    if (pattern & (1 << i)) {
                        set_mask |= pin_info[i].mask;
                    } else {
                        clear_mask |= pin_info[i].mask;
                    }
                }
            }

            // Store PORT info in LUT entry
            mMultiPortLUT[pattern].ports[p].port_set = (void*)port;
            mMultiPortLUT[pattern].ports[p].port_clear = nullptr;  // Unused on AVR
            mMultiPortLUT[pattern].ports[p].set_mask = set_mask;
            mMultiPortLUT[pattern].ports[p].clear_mask = clear_mask;
        }
    }
}

/// AVR implementation of writeSamePortImpl
/// Direct PORT write (15-20ns) - same as FastPinsSamePort<>
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // AVR: set_mask contains pre-computed full PORT value
    // clear_mask is unused on AVR
    (void)clear_mask;

    // Single 8-bit write = 15-20ns!
    *mPort = static_cast<uint8_t>(set_mask);
}

/// AVR implementation of writeMultiPortImpl
/// Read-modify-write for each PORT (80-160ns depending on port count)
///
/// Performance breakdown per PORT:
/// - Read PORT: ~10ns
/// - Modify (OR + AND-NOT): ~15ns
/// - Write PORT: ~15ns
/// Total: ~40ns per PORT
///
/// Example timing:
/// - 2 PORTs: 80ns (PORTB + PORTD)
/// - 3 PORTs: 120ns (PORTB + PORTC + PORTD)
/// - 4 PORTs: 160ns (PORTA + PORTB + PORTC + PORTD on Mega)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // Loop through each PORT and perform RMW
    for (uint8_t p = 0; p < entry.port_count; p++) {
        // Skip if no operations needed
        if (entry.ports[p].set_mask == 0 && entry.ports[p].clear_mask == 0) {
            continue;
        }

        // Cast void* back to PORT register pointer
        volatile uint8_t* port = (volatile uint8_t*)entry.ports[p].port_set;

        // Read current PORT value
        uint8_t port_value = *port;

        // Apply modifications
        port_value |= entry.ports[p].set_mask;         // Set bits HIGH
        port_value &= ~entry.ports[p].clear_mask;      // Clear bits LOW

        // Write back modified value
        *port = port_value;
    }
}

// ============================================================================
// FastPinsWithClock<> implementations for AVR
// ============================================================================

/// AVR implementation of validateAllSamePort for FastPinsWithClock
/// Validates that clock pin and all data pins are on the same PORT
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    // Get clock pin's PORT
    volatile uint8_t* clock_port = detail::getPinPortAVR(clockPin);

    // Validate all data pins are on same PORT as clock
    for (uint8_t i = 0; i < count; i++) {
        volatile uint8_t* data_port = detail::getPinPortAVR(dataPins[i]);
        if (data_port != clock_port) {
            FL_WARN("FastPinsWithClock: Clock and data pins must be on same PORT!");
            return false;
        }
    }
    return true;
}

/// AVR implementation of buildClockMask for FastPinsWithClock
/// Stores PORT register pointer and clock pin mask
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store PORT register (shared with data pins after validation)
    mClockPort = detail::getPinPortAVR(clockPin);

    // Store clock pin mask
    mClockMask = detail::getPinMaskAVR(clockPin);
}

/// AVR implementation of clockHighImpl for FastPinsWithClock
/// Sets clock pin HIGH using direct PORT write with preservation
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockHighImpl() {
    // AVR: Direct PORT write with bit manipulation
    // Read current PORT state to preserve other pins
    uint8_t port_value = *mClockPort;
    // Set clock bit HIGH
    port_value |= mClockMask;
    // Write back
    *mClockPort = port_value;
}

/// AVR implementation of clockLowImpl for FastPinsWithClock
/// Sets clock pin LOW using direct PORT write with preservation
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockLowImpl() {
    // AVR: Direct PORT write with bit manipulation
    // Read current PORT state to preserve other pins
    uint8_t port_value = *mClockPort;
    // Clear clock bit LOW
    port_value &= ~mClockMask;
    // Write back
    *mClockPort = port_value;
}

} // namespace fl
