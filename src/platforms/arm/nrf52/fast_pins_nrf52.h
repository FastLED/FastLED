#ifndef __FAST_PINS_NRF52_H
#define __FAST_PINS_NRF52_H

/**
 * FastPins NRF52/51 Platform Implementation
 *
 * NRF52/51 GPIO Architecture:
 * ===========================
 * - NRF52: Two GPIO ports (P0, P1) - each up to 32 pins
 * - NRF51: Single GPIO port (P0) - up to 32 pins
 * - Register structure: NRF_GPIO_Type with OUTSET, OUTCLR, OUT registers
 * - Atomic operations: OUTSET (set pins HIGH), OUTCLR (set pins LOW)
 * - Performance: ~25-30ns writes (atomic OUTSET/OUTCLR)
 *
 * Pin Encoding:
 * =============
 * - NRF uses NRF_GPIO_PIN_MAP(port, pin) macro
 * - Port 0: pins 0-31 (P0.0 to P0.31)
 * - Port 1: pins 32-47 (P1.0 to P1.15) - NRF52840 only
 * - Arduino pin numbers vary by board (see fastpin_arm_nrf52_variants.h)
 *
 * Register Map:
 * =============
 * NRF_P0->OUT      - Direct GPIO output register (read/write current state)
 * NRF_P0->OUTSET   - Set pins HIGH (write 1 to set, write 0 has no effect)
 * NRF_P0->OUTCLR   - Set pins LOW (write 1 to clear, write 0 has no effect)
 *
 * NRF_P1->OUT      - Direct GPIO output register (port 1, NRF52840 only)
 * NRF_P1->OUTSET   - Set pins HIGH (port 1)
 * NRF_P1->OUTCLR   - Set pins LOW (port 1)
 *
 * Platform Notes:
 * ===============
 * - Most NRF52 boards use identity pin mapping (Arduino pin = MCU pin)
 * - Some boards (e.g., nRF52840 DK) have complex mappings
 * - Use NRF_GPIO_PIN_MAP() or existing FastPin<> infrastructure for pin info
 * - Not all pins available on all boards (check board documentation)
 */

namespace fl {
namespace detail {

/// Get NRF_GPIO_Type pointer for port number (0 or 1)
inline NRF_GPIO_Type* getPortNRF(uint8_t port) {
    if (port == 0) {
        return NRF_P0;
    }
#if defined(NRF_P1)
    else if (port == 1) {
        return NRF_P1;
    }
#endif
    return NRF_P0;  // Fallback to P0
}

/// Get port number for NRF pin (runtime heuristic)
/// Note: This is a simplified heuristic. For compile-time accuracy,
/// use FastPin<PIN>::sport() which has board-specific mappings.
inline uint8_t getPinPortNRF(uint8_t pin) {
    // NRF52: Port 0 = pins 0-31, Port 1 = pins 32-63
    // This assumes identity mapping (pin number = MCU pin)
    // For non-identity boards, this may not be accurate
    return (pin >= 32) ? 1 : 0;
}

/// Get pin mask for NRF pin (runtime heuristic)
inline uint32_t getPinMaskNRF(uint8_t pin) {
    // Mask is based on bit position within port
    uint8_t bit = pin & 0x1F;  // pin % 32
    return 1UL << bit;
}

} // namespace detail

//
// FastPinsSamePort<> Implementation for NRF52/51
//

template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // NRF52/51: All pins must be on same GPIO port (P0 or P1)
    // Use runtime heuristic (pin / 32 gives port number)
    // Note: This assumes identity mapping. For board-specific mappings,
    // proper implementation would use FastPin<>::sport() at compile time.

    uint8_t first_port = detail::getPinPortNRF(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        uint8_t port = detail::getPinPortNRF(pins[i]);
        if (port != first_port) {
            return false;  // Cross-port not allowed in same-port mode
        }
    }

    return true;
}

template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // NRF52/51: Use OUTSET and OUTCLR registers for atomic writes
    NRF_GPIO_Type* port = detail::getPortNRF(mPort);
    port->OUTSET = set_mask;
    port->OUTCLR = clear_mask;
}

template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine GPIO port (P0 or P1)
    mPort = detail::getPinPortNRF(pins[0]);

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskNRF(pins[i]);
    }

    // Build 256-entry LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mLUT[pattern].set_mask = set_mask;
        mLUT[pattern].clear_mask = clear_mask;
    }
}

//
// FastPinsWithClock<> Implementation for NRF52/51
//

template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(
    uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {

    // NRF52/51: Clock and all data pins must be on same GPIO port (P0 or P1)
    // Use runtime heuristic (optimistic validation)

    uint8_t clock_port = detail::getPinPortNRF(clockPin);

    for (uint8_t i = 0; i < count; i++) {
        uint8_t port = detail::getPinPortNRF(dataPins[i]);
        if (port != clock_port) {
            return false;  // Different ports detected
        }
    }

    return true;
}

template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store clock mask and register pointers
    mClockMask = detail::getPinMaskNRF(clockPin);

    uint8_t port = detail::getPinPortNRF(clockPin);
    NRF_GPIO_Type* gpio = detail::getPortNRF(port);

    mClockSet = &gpio->OUTSET;
    mClockClear = &gpio->OUTCLR;
}

//
// FastPins<> Auto-Detect Implementation for NRF52/51
//

template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // NRF52/51: Check if all pins are on same GPIO port (P0 or P1)
    // Use runtime heuristic (pin / 32 gives port number)

    uint8_t first_port = detail::getPinPortNRF(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        uint8_t port = detail::getPinPortNRF(pins[i]);
        if (port != first_port) {
            return false;  // Different ports detected
        }
    }

    return true;  // All pins on same port
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // NRF52/51: Same as FastPinsSamePort<>::writeImpl()
    NRF_GPIO_Type* port = detail::getPortNRF(mPort);
    port->OUTSET = set_mask;
    port->OUTCLR = clear_mask;
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // NRF52/51: Sequential writes to multiple GPIO ports (P0 and P1)
    for (uint8_t i = 0; i < entry.port_count; i++) {
        volatile uint32_t* set_reg = (volatile uint32_t*)entry.ports[i].port_set;
        volatile uint32_t* clear_reg = (volatile uint32_t*)entry.ports[i].port_clear;

        *set_reg = entry.ports[i].set_mask;
        *clear_reg = entry.ports[i].clear_mask;
    }
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Determine GPIO port (P0 or P1)
    mPort = detail::getPinPortNRF(pins[0]);

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskNRF(pins[i]);
    }

    // Build 256-entry LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        for (uint8_t bit = 0; bit < count; bit++) {
            if (pattern & (1 << bit)) {
                set_mask |= pin_masks[bit];
            } else {
                clear_mask |= pin_masks[bit];
            }
        }

        mSamePortLUT[pattern].set_mask = set_mask;
        mSamePortLUT[pattern].clear_mask = clear_mask;
    }
}

template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Group pins by GPIO port
    struct PinInfo {
        uint8_t port;
        uint32_t mask;
    };
    PinInfo pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        pin_info[i].port = detail::getPinPortNRF(pins[i]);
        pin_info[i].mask = detail::getPinMaskNRF(pins[i]);
    }

    // Build 256-entry multi-port LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        // Count unique ports for this pattern
        uint8_t ports[2] = {0xFF, 0xFF};  // Max 2 ports (P0, P1)
        uint8_t port_count = 0;

        // Identify unique ports
        for (uint8_t bit = 0; bit < count; bit++) {
            uint8_t port = pin_info[bit].port;
            bool found = false;
            for (uint8_t p = 0; p < port_count; p++) {
                if (ports[p] == port) {
                    found = true;
                    break;
                }
            }
            if (!found && port_count < 2) {
                ports[port_count++] = port;
            }
        }

        // Build masks for each port
        for (uint8_t p = 0; p < port_count; p++) {
            uint8_t port = ports[p];
            uint32_t set_mask = 0;
            uint32_t clear_mask = 0;

            for (uint8_t bit = 0; bit < count; bit++) {
                if (pin_info[bit].port == port) {
                    if (pattern & (1 << bit)) {
                        set_mask |= pin_info[bit].mask;
                    } else {
                        clear_mask |= pin_info[bit].mask;
                    }
                }
            }

            // Store port pointers and masks
            NRF_GPIO_Type* gpio = detail::getPortNRF(port);
            mMultiPortLUT[pattern].ports[p].port_set = &gpio->OUTSET;
            mMultiPortLUT[pattern].ports[p].port_clear = &gpio->OUTCLR;
            mMultiPortLUT[pattern].ports[p].set_mask = set_mask;
            mMultiPortLUT[pattern].ports[p].clear_mask = clear_mask;
        }

        mMultiPortLUT[pattern].port_count = port_count;
    }
}

} // namespace fl

//
// Platform-Specific Storage for NRF52/51
//

#if defined(NRF52) || defined(NRF52_SERIES) || defined(NRF51) || defined(NRF51_SERIES) || defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_ARCH_NRF5)

namespace fl {

// FastPinsSamePort<> platform-specific members
template<uint8_t MAX_PINS>
class FastPinsSamePort {
private:
    uint8_t mPort = 0;  // NRF52/51: GPIO port (0 = P0, 1 = P1)
};

// FastPins<> platform-specific members
template<uint8_t MAX_PINS>
class FastPins {
private:
    uint8_t mPort = 0;  // NRF52/51: GPIO port for same-port mode (0 = P0, 1 = P1)
};

} // namespace fl

#endif // NRF52/51 platform detection

#endif // __FAST_PINS_NRF52_H
