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

} // namespace fl
