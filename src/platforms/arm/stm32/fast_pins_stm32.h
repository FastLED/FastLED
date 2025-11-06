/// @file platforms/arm/stm32/fast_pins_stm32.h
/// @brief STM32-specific implementation of FastPins using BSRR register
///
/// This implementation uses STM32's Bit Set/Reset Register (BSRR) for atomic
/// multi-pin operations. BSRR is a 32-bit register where:
/// - Bits 0-15: SET bits (write 1 to set corresponding pin HIGH)
/// - Bits 16-31: RESET bits (write 1 to set corresponding pin LOW)
///
/// Hardware Register:
/// - GPIOx->BSRR: Combined set/reset register (write-only)
/// - STM32F2: GPIOx->BSRRL (set) and GPIOx->BSRRH (reset) as separate 16-bit regs
///
/// Limitations:
/// - Current implementation assumes all pins are on the same GPIO port (A/B/C/D/E)
/// - Cross-port scenarios require enhanced LUT with per-port tracking (TODO)
///
/// Performance: ~40ns per write (single 32-bit write to BSRR)

#pragma once

#include "fl/warn.h"

namespace fl {

namespace detail {

/// Runtime pin mask lookup helper for STM32
/// NOTE: This simplified version assumes pin number = bit position within a port
/// Real implementation would need to map Arduino pin numbers to GPIO port + bit
inline uint32_t getPinMaskSTM32(uint8_t pin) {
    // STM32 pins are typically 0-15 per GPIO port
    // For simplicity, use pin directly as bit position
    // TODO: Proper pin-to-port mapping
    if (pin >= 16) return 0;  // Safety check
    return 1UL << pin;
}

/// Get GPIO port pointer for STM32 pin (for FastPinsSamePort)
/// Returns the GPIO_TypeDef pointer for the given pin
/// Note: This is a simplified implementation that uses pin naming conventions
/// Real STM32 boards use pin names like PA0-PA15, PB0-PB15, etc.
inline GPIO_TypeDef* getPinPortSTM32(uint8_t pin) {
    // STM32 pin naming convention:
    // PA0-PA15 = 0-15 (GPIOA)
    // PB0-PB15 = 16-31 (GPIOB)
    // PC0-PC15 = 32-47 (GPIOC)
    // PD0-PD15 = 48-63 (GPIOD)
    // etc.

    // This is a simplified mapping - real boards may use different schemes
    // In practice, most STM32 Arduino cores define pin numbers that include port info

    #if defined(GPIOA)
        if (pin >= 0 && pin <= 15) return GPIOA;
    #endif
    #if defined(GPIOB)
        if (pin >= 16 && pin <= 31) return GPIOB;
    #endif
    #if defined(GPIOC)
        if (pin >= 32 && pin <= 47) return GPIOC;
    #endif
    #if defined(GPIOD)
        if (pin >= 48 && pin <= 63) return GPIOD;
    #endif
    #if defined(GPIOE)
        if (pin >= 64 && pin <= 79) return GPIOE;
    #endif

    // Fallback to GPIOA
    return GPIOA;
}

/// Get pin bit mask within its port for STM32
/// Returns the bit mask (0-15) for the pin within its GPIO port
inline uint32_t getPinBitMaskSTM32(uint8_t pin) {
    // Extract bit position within port (0-15)
    uint8_t bit = pin % 16;
    return 1UL << bit;
}

} // namespace detail

/// STM32 implementation of FastPins::writeImpl()
/// Uses BSRR register for atomic set/reset operations
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // TODO: This simplified version writes to GPIOA only
    // Full implementation needs to:
    // 1. Determine which GPIO port(s) the pins belong to
    // 2. Issue separate BSRR writes per port
    // 3. Store port information in enhanced LUT structure

#if defined(STM32F2XX)
    // STM32F2 has separate BSRRL (set) and BSRRH (reset) registers
    // Assume all pins on GPIOA for now
    GPIOA->BSRRL = set_mask & 0xFFFF;
    GPIOA->BSRRH = clear_mask & 0xFFFF;
#else
    // Modern STM32: Combined BSRR register
    // Bits 0-15 = set, bits 16-31 = reset
    uint32_t bsrr_value = (set_mask & 0xFFFF) | ((clear_mask & 0xFFFF) << 16);

    // Assume all pins on GPIOA for now
    // TODO: Dynamic port selection based on configured pins
    GPIOA->BSRR = bsrr_value;
#endif
}

/// STM32 implementation of FastPins::buildLUT()
/// Constructs lookup table mapping bit patterns to GPIO masks
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks for each configured pin
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinMaskSTM32(pins[i]);
    }

    // Build LUT: For each possible bit pattern (0 to 2^count - 1)
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        // Map each bit in pattern to corresponding pin mask
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

    // Fill remaining LUT entries (if any) with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

// ============================================================================
// FastPinsSamePort<> implementations for STM32
// ============================================================================

/// STM32 same-port validation for FastPinsSamePort
/// Validates that all pins are on the same GPIO port (GPIOA/B/C/D/E)
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // STM32: All pins must be on same GPIO port
    GPIO_TypeDef* first_port = detail::getPinPortSTM32(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        GPIO_TypeDef* port = detail::getPinPortSTM32(pins[i]);
        if (port != first_port) {
            FL_WARN("FastPinsSamePort: Pins span multiple GPIO ports - not all on same port!");
            return false;  // Cross-port not allowed in same-port mode
        }
    }
    return true;
}

/// STM32 same-port implementation for FastPinsSamePort::writeImpl()
/// Uses single-port BSRR for atomic writes
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
#if defined(STM32F2XX)
    // STM32F2 has separate BSRRL (set) and BSRRH (reset) registers
    mGpioPort->BSRRL = set_mask & 0xFFFF;
    mGpioPort->BSRRH = clear_mask & 0xFFFF;
#else
    // Modern STM32: Combined BSRR register
    // Bits 0-15 = set, bits 16-31 = reset
    uint32_t bsrr = (set_mask & 0xFFFF) | ((clear_mask & 0xFFFF) << 16);
    mGpioPort->BSRR = bsrr;
#endif
}

/// STM32 implementation for FastPinsSamePort::buildLUT()
/// Builds 256-entry LUT with single-port masks
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Store GPIO port (validation ensures all same port)
    mGpioPort = detail::getPinPortSTM32(pins[0]);

    // Extract pin masks (bit position within port)
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinBitMaskSTM32(pins[i]);
    }

    // Build LUT: For each possible bit pattern (0 to 2^count - 1)
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
        uint32_t set_mask = 0;
        uint32_t clear_mask = 0;

        // Map each bit in pattern to corresponding pin mask
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

    // Fill remaining LUT entries (if any) with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

// ============================================================================
// FastPinsWithClock<> implementations for STM32
// ============================================================================

/// STM32 implementation of validateAllSamePort for FastPinsWithClock
/// Validates that clock pin and all data pins are on the same GPIO port
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    // Get clock pin's GPIO port
    GPIO_TypeDef* clock_port = detail::getPinPortSTM32(clockPin);

    // Validate all data pins are on same port as clock
    for (uint8_t i = 0; i < count; i++) {
        GPIO_TypeDef* data_port = detail::getPinPortSTM32(dataPins[i]);
        if (data_port != clock_port) {
            FL_WARN("FastPinsWithClock: Clock and data pins must be on same GPIO port!");
            return false;
        }
    }
    return true;
}

/// STM32 implementation of buildClockMask for FastPinsWithClock
/// Stores GPIO port pointer and clock pin mask
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Store GPIO port (shared with data pins after validation)
    mClockPort = detail::getPinPortSTM32(clockPin);

    // Store clock pin mask (bit position within port)
    mClockMask = detail::getPinBitMaskSTM32(clockPin);
}

/// STM32 implementation of clockHighImpl for FastPinsWithClock
/// Sets clock pin HIGH using BSRR register
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockHighImpl() {
#if defined(STM32F2XX)
    // STM32F2: Use BSRRL register for set
    mClockPort->BSRRL = mClockMask & 0xFFFF;
#else
    // Modern STM32: Use BSRR register (bits 0-15 for set)
    mClockPort->BSRR = mClockMask & 0xFFFF;
#endif
}

/// STM32 implementation of clockLowImpl for FastPinsWithClock
/// Sets clock pin LOW using BSRR register
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockLowImpl() {
#if defined(STM32F2XX)
    // STM32F2: Use BSRRH register for reset
    mClockPort->BSRRH = mClockMask & 0xFFFF;
#else
    // Modern STM32: Use BSRR register (bits 16-31 for reset)
    mClockPort->BSRR = (mClockMask & 0xFFFF) << 16;
#endif
}

// ============================================================================
// FastPins<> multi-port implementations for STM32
// ============================================================================

/// STM32 implementation of allSamePort() for FastPins<>
/// Detects if all pins are on the same GPIO port
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // STM32: Check if all pins on same GPIO port (GPIOA/B/C/D/E)
    GPIO_TypeDef* first_port = detail::getPinPortSTM32(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        GPIO_TypeDef* port = detail::getPinPortSTM32(pins[i]);
        if (port != first_port) {
            return false;  // Cross-port detected
        }
    }
    return true;  // All on same port
}

/// STM32 implementation of buildSamePortLUT() for FastPins<>
/// Builds 2KB LUT for same-port operation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks (bit position within port)
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        pin_masks[i] = detail::getPinBitMaskSTM32(pins[i]);
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

    // Store GPIO port (same for all pins in same-port mode)
    mGpioPort = detail::getPinPortSTM32(pins[0]);
}

/// STM32 implementation of buildMultiPortLUT() for FastPins<>
/// Builds 10KB LUT for multi-port operation with cross-port support
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Group pins by GPIO port
    struct PinInfo {
        GPIO_TypeDef* port;
        uint32_t mask;
    };
    PinInfo pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        pin_info[i].port = detail::getPinPortSTM32(pins[i]);
        pin_info[i].mask = detail::getPinBitMaskSTM32(pins[i]);
    }

    // Build 256-entry multi-port LUT
    for (uint16_t pattern = 0; pattern < 256; pattern++) {
        // Initialize all ports to empty
        for (uint8_t p = 0; p < 4; p++) {
            mMultiPortLUT[pattern].ports[p].port_set = nullptr;
            mMultiPortLUT[pattern].ports[p].port_clear = nullptr;
            mMultiPortLUT[pattern].ports[p].set_mask = 0;
            mMultiPortLUT[pattern].ports[p].clear_mask = 0;
        }

        // Track unique ports (up to 4)
        GPIO_TypeDef* unique_ports[4] = {nullptr, nullptr, nullptr, nullptr};
        uint8_t port_count = 0;

        // Group pin operations by port
        for (uint8_t bit = 0; bit < count; bit++) {
            GPIO_TypeDef* pin_port = pin_info[bit].port;
            uint32_t pin_mask = pin_info[bit].mask;

            // Find or add port to unique_ports array
            int8_t port_index = -1;
            for (uint8_t p = 0; p < port_count; p++) {
                if (unique_ports[p] == pin_port) {
                    port_index = p;
                    break;
                }
            }

            if (port_index == -1) {
                // New port - add to unique_ports
                if (port_count < 4) {
                    port_index = port_count;
                    unique_ports[port_count] = pin_port;
                    port_count++;
                } else {
                    // Should never happen (max 4 ports supported)
                    continue;
                }
            }

            // Add pin to port's masks
            if (pattern & (1 << bit)) {
                mMultiPortLUT[pattern].ports[port_index].set_mask |= pin_mask;
            } else {
                mMultiPortLUT[pattern].ports[port_index].clear_mask |= pin_mask;
            }
        }

        // Store port addresses and count
        mMultiPortLUT[pattern].port_count = port_count;
        for (uint8_t p = 0; p < port_count; p++) {
            // Store port addresses (cast to void* for generic storage)
            mMultiPortLUT[pattern].ports[p].port_set = (void*)unique_ports[p];
            mMultiPortLUT[pattern].ports[p].port_clear = (void*)unique_ports[p];
        }
    }
}

/// STM32 implementation of writeSamePortImpl() for FastPins<>
/// Writes to single GPIO port (30ns)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
#if defined(STM32F2XX)
    // STM32F2 has separate BSRRL (set) and BSRRH (reset) registers
    mGpioPort->BSRRL = set_mask & 0xFFFF;
    mGpioPort->BSRRH = clear_mask & 0xFFFF;
#else
    // Modern STM32: Combined BSRR register
    // Bits 0-15 = set, bits 16-31 = reset
    uint32_t bsrr = (set_mask & 0xFFFF) | ((clear_mask & 0xFFFF) << 16);
    mGpioPort->BSRR = bsrr;
#endif
}

/// STM32 implementation of writeMultiPortImpl() for FastPins<>
/// Writes to multiple GPIO ports sequentially (60-120ns)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeMultiPortImpl(const FastPinsMaskEntryMulti& entry) {
    // Loop through each active port and write
    for (uint8_t p = 0; p < entry.port_count; p++) {
        const auto& port_entry = entry.ports[p];

        // Skip if no operations on this port
        if (port_entry.set_mask == 0 && port_entry.clear_mask == 0) {
            continue;
        }

        // Cast back to GPIO_TypeDef*
        GPIO_TypeDef* port = (GPIO_TypeDef*)port_entry.port_set;

#if defined(STM32F2XX)
        // STM32F2 has separate BSRRL (set) and BSRRH (reset) registers
        if (port_entry.set_mask) {
            port->BSRRL = port_entry.set_mask & 0xFFFF;
        }
        if (port_entry.clear_mask) {
            port->BSRRH = port_entry.clear_mask & 0xFFFF;
        }
#else
        // Modern STM32: Combined BSRR register
        // Bits 0-15 = set, bits 16-31 = reset
        uint32_t bsrr = (port_entry.set_mask & 0xFFFF) | ((port_entry.clear_mask & 0xFFFF) << 16);
        port->BSRR = bsrr;
#endif
    }
}

} // namespace fl
