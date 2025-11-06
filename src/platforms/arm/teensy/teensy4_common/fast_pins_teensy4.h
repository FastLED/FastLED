/// @file platforms/arm/teensy/teensy4_common/fast_pins_teensy4.h
/// @brief Teensy 4.x-specific implementation of FastPinsSamePort using GPIO_DR_SET/CLEAR registers
///
/// This implementation uses Teensy 4.x (MXRT1062) GPIO Data Register SET/CLEAR
/// registers for atomic multi-pin operations. The MXRT1062 has 9 GPIO ports
/// (GPIO1-GPIO9), each with dedicated SET, CLEAR, and TOGGLE registers.
///
/// Hardware Registers (per GPIO port):
/// - GPIOx_DR: Data register (full read/write)
/// - GPIOx_DR_SET: Write 1 to set pins HIGH (W1S)
/// - GPIOx_DR_CLEAR: Write 1 to clear pins LOW (W1C)
/// - GPIOx_DR_TOGGLE: Write 1 to toggle pins
///
/// Teensy Pin Mapping:
/// - Teensy uses "fast" GPIO ports (GPIO6-GPIO9) for most pins
/// - Each pin maps to a specific GPIO port + bit position
/// - See fastpin_arm_mxrt1062.h for complete pin mapping table
///
/// Performance: ~20-30ns per write (direct hardware register access)

#pragma once

#include "fl/stdint.h"
#include "fl/warn.h"

#if defined(FASTLED_TEENSY4) && defined(CORE_TEENSY)
#include "fastpin_arm_mxrt1062.h"

namespace fl {

namespace detail {

/// Helper to get GPIO port SET register address for a Teensy 4.x pin
/// Uses FastPin<>::sport() to get the correct GPIO_DR_SET register
template<uint8_t PIN>
inline volatile uint32_t* getTeensy4SetReg() {
    return (volatile uint32_t*)FastPin<PIN>::sport();
}

/// Helper to get GPIO port CLEAR register address for a Teensy 4.x pin
/// Uses FastPin<>::cport() to get the correct GPIO_DR_CLEAR register
template<uint8_t PIN>
inline volatile uint32_t* getTeensy4ClearReg() {
    return (volatile uint32_t*)FastPin<PIN>::cport();
}

/// Helper to get pin bit mask for a Teensy 4.x pin
/// Uses FastPin<>::mask() to get the correct bit mask within the GPIO port
template<uint8_t PIN>
inline uint32_t getTeensy4Mask() {
    return FastPin<PIN>::mask();
}

/// Runtime pin info lookup using switch statement
/// Returns GPIO port SET/CLEAR register addresses and bit mask for given pin number
struct PinInfo {
    volatile uint32_t* set_reg;
    volatile uint32_t* clear_reg;
    uint32_t mask;
};

inline PinInfo getPinInfo(uint8_t pin) {
    PinInfo info = {nullptr, nullptr, 0};

    // Use switch to map pin number to FastPin template instantiation
    // This is necessary because we need compile-time pin numbers for FastPin<PIN>
    switch (pin) {
        case 0: info.set_reg = getTeensy4SetReg<0>(); info.clear_reg = getTeensy4ClearReg<0>(); info.mask = getTeensy4Mask<0>(); break;
        case 1: info.set_reg = getTeensy4SetReg<1>(); info.clear_reg = getTeensy4ClearReg<1>(); info.mask = getTeensy4Mask<1>(); break;
        case 2: info.set_reg = getTeensy4SetReg<2>(); info.clear_reg = getTeensy4ClearReg<2>(); info.mask = getTeensy4Mask<2>(); break;
        case 3: info.set_reg = getTeensy4SetReg<3>(); info.clear_reg = getTeensy4ClearReg<3>(); info.mask = getTeensy4Mask<3>(); break;
        case 4: info.set_reg = getTeensy4SetReg<4>(); info.clear_reg = getTeensy4ClearReg<4>(); info.mask = getTeensy4Mask<4>(); break;
        case 5: info.set_reg = getTeensy4SetReg<5>(); info.clear_reg = getTeensy4ClearReg<5>(); info.mask = getTeensy4Mask<5>(); break;
        case 6: info.set_reg = getTeensy4SetReg<6>(); info.clear_reg = getTeensy4ClearReg<6>(); info.mask = getTeensy4Mask<6>(); break;
        case 7: info.set_reg = getTeensy4SetReg<7>(); info.clear_reg = getTeensy4ClearReg<7>(); info.mask = getTeensy4Mask<7>(); break;
        case 8: info.set_reg = getTeensy4SetReg<8>(); info.clear_reg = getTeensy4ClearReg<8>(); info.mask = getTeensy4Mask<8>(); break;
        case 9: info.set_reg = getTeensy4SetReg<9>(); info.clear_reg = getTeensy4ClearReg<9>(); info.mask = getTeensy4Mask<9>(); break;
        case 10: info.set_reg = getTeensy4SetReg<10>(); info.clear_reg = getTeensy4ClearReg<10>(); info.mask = getTeensy4Mask<10>(); break;
        case 11: info.set_reg = getTeensy4SetReg<11>(); info.clear_reg = getTeensy4ClearReg<11>(); info.mask = getTeensy4Mask<11>(); break;
        case 12: info.set_reg = getTeensy4SetReg<12>(); info.clear_reg = getTeensy4ClearReg<12>(); info.mask = getTeensy4Mask<12>(); break;
        case 13: info.set_reg = getTeensy4SetReg<13>(); info.clear_reg = getTeensy4ClearReg<13>(); info.mask = getTeensy4Mask<13>(); break;
        case 14: info.set_reg = getTeensy4SetReg<14>(); info.clear_reg = getTeensy4ClearReg<14>(); info.mask = getTeensy4Mask<14>(); break;
        case 15: info.set_reg = getTeensy4SetReg<15>(); info.clear_reg = getTeensy4ClearReg<15>(); info.mask = getTeensy4Mask<15>(); break;
        case 16: info.set_reg = getTeensy4SetReg<16>(); info.clear_reg = getTeensy4ClearReg<16>(); info.mask = getTeensy4Mask<16>(); break;
        case 17: info.set_reg = getTeensy4SetReg<17>(); info.clear_reg = getTeensy4ClearReg<17>(); info.mask = getTeensy4Mask<17>(); break;
        case 18: info.set_reg = getTeensy4SetReg<18>(); info.clear_reg = getTeensy4ClearReg<18>(); info.mask = getTeensy4Mask<18>(); break;
        case 19: info.set_reg = getTeensy4SetReg<19>(); info.clear_reg = getTeensy4ClearReg<19>(); info.mask = getTeensy4Mask<19>(); break;
        case 20: info.set_reg = getTeensy4SetReg<20>(); info.clear_reg = getTeensy4ClearReg<20>(); info.mask = getTeensy4Mask<20>(); break;
        case 21: info.set_reg = getTeensy4SetReg<21>(); info.clear_reg = getTeensy4ClearReg<21>(); info.mask = getTeensy4Mask<21>(); break;
        case 22: info.set_reg = getTeensy4SetReg<22>(); info.clear_reg = getTeensy4ClearReg<22>(); info.mask = getTeensy4Mask<22>(); break;
        case 23: info.set_reg = getTeensy4SetReg<23>(); info.clear_reg = getTeensy4ClearReg<23>(); info.mask = getTeensy4Mask<23>(); break;
        case 24: info.set_reg = getTeensy4SetReg<24>(); info.clear_reg = getTeensy4ClearReg<24>(); info.mask = getTeensy4Mask<24>(); break;
        case 25: info.set_reg = getTeensy4SetReg<25>(); info.clear_reg = getTeensy4ClearReg<25>(); info.mask = getTeensy4Mask<25>(); break;
        case 26: info.set_reg = getTeensy4SetReg<26>(); info.clear_reg = getTeensy4ClearReg<26>(); info.mask = getTeensy4Mask<26>(); break;
        case 27: info.set_reg = getTeensy4SetReg<27>(); info.clear_reg = getTeensy4ClearReg<27>(); info.mask = getTeensy4Mask<27>(); break;
        case 28: info.set_reg = getTeensy4SetReg<28>(); info.clear_reg = getTeensy4ClearReg<28>(); info.mask = getTeensy4Mask<28>(); break;
        case 29: info.set_reg = getTeensy4SetReg<29>(); info.clear_reg = getTeensy4ClearReg<29>(); info.mask = getTeensy4Mask<29>(); break;
        case 30: info.set_reg = getTeensy4SetReg<30>(); info.clear_reg = getTeensy4ClearReg<30>(); info.mask = getTeensy4Mask<30>(); break;
        case 31: info.set_reg = getTeensy4SetReg<31>(); info.clear_reg = getTeensy4ClearReg<31>(); info.mask = getTeensy4Mask<31>(); break;
        case 32: info.set_reg = getTeensy4SetReg<32>(); info.clear_reg = getTeensy4ClearReg<32>(); info.mask = getTeensy4Mask<32>(); break;
        case 33: info.set_reg = getTeensy4SetReg<33>(); info.clear_reg = getTeensy4ClearReg<33>(); info.mask = getTeensy4Mask<33>(); break;
        case 34: info.set_reg = getTeensy4SetReg<34>(); info.clear_reg = getTeensy4ClearReg<34>(); info.mask = getTeensy4Mask<34>(); break;
        case 35: info.set_reg = getTeensy4SetReg<35>(); info.clear_reg = getTeensy4ClearReg<35>(); info.mask = getTeensy4Mask<35>(); break;
        case 36: info.set_reg = getTeensy4SetReg<36>(); info.clear_reg = getTeensy4ClearReg<36>(); info.mask = getTeensy4Mask<36>(); break;
        case 37: info.set_reg = getTeensy4SetReg<37>(); info.clear_reg = getTeensy4ClearReg<37>(); info.mask = getTeensy4Mask<37>(); break;
        case 38: info.set_reg = getTeensy4SetReg<38>(); info.clear_reg = getTeensy4ClearReg<38>(); info.mask = getTeensy4Mask<38>(); break;
        case 39: info.set_reg = getTeensy4SetReg<39>(); info.clear_reg = getTeensy4ClearReg<39>(); info.mask = getTeensy4Mask<39>(); break;
        default: break;  // Invalid pin
    }

    return info;
}

} // namespace detail

/// Teensy 4.x same-port validation
template<uint8_t MAX_PINS>
bool FastPinsSamePort<MAX_PINS>::validateSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Teensy 4.x: All pins must be on same GPIO port (same SET register address)
    detail::PinInfo first_info = detail::getPinInfo(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        detail::PinInfo info = detail::getPinInfo(pins[i]);
        if (info.set_reg != first_info.set_reg) {
            FL_WARN("FastPinsSamePort: Pins must be on same GPIO port for optimal performance!");
            return false;  // Cross-port not allowed in same-port mode
        }
    }
    return true;
}

/// Teensy 4.x same-port implementation
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::writeImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Write to stored GPIO SET/CLEAR registers
    *mGpioSet = set_mask;
    *mGpioClear = clear_mask;
}

/// Build LUT for Teensy 4.x
template<uint8_t MAX_PINS>
void FastPinsSamePort<MAX_PINS>::buildLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;
    mPinCount = count;

    // Get GPIO port info from first pin (validation ensures all same port)
    detail::PinInfo first_info = detail::getPinInfo(pins[0]);
    mGpioSet = first_info.set_reg;
    mGpioClear = first_info.clear_reg;

    // Extract pin masks
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        detail::PinInfo info = detail::getPinInfo(pins[i]);
        pin_masks[i] = info.mask;
    }

    // Build LUT: For each possible bit pattern
    uint16_t num_patterns = 1 << count;
    for (uint16_t pattern = 0; pattern < num_patterns; pattern++) {
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

    // Fill remaining LUT entries with zeros
    for (uint16_t pattern = num_patterns; pattern < LUT_SIZE; pattern++) {
        mLUT[pattern].set_mask = 0;
        mLUT[pattern].clear_mask = 0;
    }
}

// ============================================================================
// FastPinsWithClock<> implementations for Teensy 4.x
// ============================================================================

/// Teensy 4.x implementation of validateAllSamePort for FastPinsWithClock
/// Validates that clock pin and all data pins are on the same GPIO port
template<uint8_t DATA_PINS>
bool FastPinsWithClock<DATA_PINS>::validateAllSamePort(uint8_t clockPin, const uint8_t* dataPins, uint8_t count) {
    // Get clock pin's GPIO port
    detail::PinInfo clock_info = detail::getPinInfo(clockPin);

    // Validate all data pins are on same port as clock
    for (uint8_t i = 0; i < count; i++) {
        detail::PinInfo data_info = detail::getPinInfo(dataPins[i]);
        if (data_info.set_reg != clock_info.set_reg) {
            FL_WARN("FastPinsWithClock: Clock and data pins must be on same GPIO port!");
            return false;
        }
    }
    return true;
}

/// Teensy 4.x implementation of buildClockMask for FastPinsWithClock
/// Stores GPIO port SET/CLEAR register addresses and clock pin mask
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::buildClockMask(uint8_t clockPin) {
    // Get clock pin info (shared GPIO port with data pins after validation)
    detail::PinInfo clock_info = detail::getPinInfo(clockPin);

    // Store GPIO port SET/CLEAR registers
    mClockSet = clock_info.set_reg;
    mClockClear = clock_info.clear_reg;

    // Store clock pin mask
    mClockMask = clock_info.mask;
}

/// Teensy 4.x implementation of clockHighImpl for FastPinsWithClock
/// Sets clock pin HIGH using GPIO_DR_SET register
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockHighImpl() {
    *mClockSet = mClockMask;
}

/// Teensy 4.x implementation of clockLowImpl for FastPinsWithClock
/// Sets clock pin LOW using GPIO_DR_CLEAR register
template<uint8_t DATA_PINS>
void FastPinsWithClock<DATA_PINS>::clockLowImpl() {
    *mClockClear = mClockMask;
}

// ============================================================================
// FastPins<> multi-port implementations for Teensy 4.x
// ============================================================================

/// Teensy 4.x implementation of allSamePort() for FastPins<>
/// Detects if all pins are on the same GPIO port
template<uint8_t MAX_PINS>
bool FastPins<MAX_PINS>::allSamePort(const uint8_t* pins, uint8_t count) {
    if (count == 0) return true;

    // Teensy 4.x: Check if all pins on same GPIO port (same SET register)
    detail::PinInfo first_info = detail::getPinInfo(pins[0]);
    for (uint8_t i = 1; i < count; i++) {
        detail::PinInfo info = detail::getPinInfo(pins[i]);
        if (info.set_reg != first_info.set_reg) {
            return false;  // Cross-port detected
        }
    }
    return true;  // All on same port
}

/// Teensy 4.x implementation of buildSamePortLUT() for FastPins<>
/// Builds 2KB LUT for same-port operation
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildSamePortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Extract pin masks (bit position within port)
    uint32_t pin_masks[MAX_PINS];
    for (uint8_t i = 0; i < count; i++) {
        detail::PinInfo info = detail::getPinInfo(pins[i]);
        pin_masks[i] = info.mask;
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

    // Store GPIO port SET/CLEAR registers (same for all pins in same-port mode)
    detail::PinInfo first_info = detail::getPinInfo(pins[0]);
    mGpioSet = first_info.set_reg;
    mGpioClear = first_info.clear_reg;
}

/// Teensy 4.x implementation of buildMultiPortLUT() for FastPins<>
/// Builds 10KB LUT for multi-port operation with cross-GPIO support
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::buildMultiPortLUT(const uint8_t* pins, uint8_t count) {
    if (count > MAX_PINS) count = MAX_PINS;

    // Group pins by GPIO port
    struct PinInfoLocal {
        volatile uint32_t* set_reg;
        volatile uint32_t* clear_reg;
        uint32_t mask;
    };
    PinInfoLocal pin_info[MAX_PINS];

    for (uint8_t i = 0; i < count; i++) {
        detail::PinInfo info = detail::getPinInfo(pins[i]);
        pin_info[i].set_reg = info.set_reg;
        pin_info[i].clear_reg = info.clear_reg;
        pin_info[i].mask = info.mask;
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
        volatile uint32_t* unique_ports[4] = {nullptr, nullptr, nullptr, nullptr};
        uint8_t port_count = 0;

        // Group pin operations by port
        for (uint8_t bit = 0; bit < count; bit++) {
            volatile uint32_t* pin_port = pin_info[bit].set_reg;
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
            // Store SET register for this port
            mMultiPortLUT[pattern].ports[p].port_set = (void*)unique_ports[p];

            // Calculate CLEAR register (SET + offset)
            // GPIO_DR_CLEAR is at offset +8 bytes from GPIO_DR_SET
            volatile uint32_t* clear_reg = (volatile uint32_t*)((uintptr_t)unique_ports[p] + 8);
            mMultiPortLUT[pattern].ports[p].port_clear = (void*)clear_reg;
        }
    }
}

/// Teensy 4.x implementation of writeSamePortImpl() for FastPins<>
/// Writes to single GPIO port (30ns)
template<uint8_t MAX_PINS>
void FastPins<MAX_PINS>::writeSamePortImpl(uint32_t set_mask, uint32_t clear_mask) {
    // Write to stored GPIO SET/CLEAR registers
    *mGpioSet = set_mask;
    *mGpioClear = clear_mask;
}

/// Teensy 4.x implementation of writeMultiPortImpl() for FastPins<>
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

        // Cast back to volatile uint32_t* for SET/CLEAR registers
        volatile uint32_t* set_reg = (volatile uint32_t*)port_entry.port_set;
        volatile uint32_t* clear_reg = (volatile uint32_t*)port_entry.port_clear;

        // Write to GPIO_DR_SET and GPIO_DR_CLEAR
        if (port_entry.set_mask) {
            *set_reg = port_entry.set_mask;
        }
        if (port_entry.clear_mask) {
            *clear_reg = port_entry.clear_mask;
        }
    }
}

} // namespace fl

#endif // FASTLED_TEENSY4 && CORE_TEENSY
