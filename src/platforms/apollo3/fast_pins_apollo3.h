#ifndef __INC_FAST_PINS_APOLLO3_H
#define __INC_FAST_PINS_APOLLO3_H

// FastPins implementation for Ambiq Apollo3 Blue (ARM Cortex-M4F)
//
// Platform: Ambiq Apollo3 Blue (Apollo3)
// Boards: SparkFun Artemis, SparkFun Edge, SparkFun Thing Plus, SparkFun RedBoard Artemis
//
// Architecture:
// - Apollo3 uses APBDMA Fast GPIO with BBSETCLEAR register for atomic operations
// - APBDMA->BBSETCLEAR: Bit-band set/clear register (8 bits control pin groups)
// - APBDMA->BBINPUT: Read pin states
// - Each bit (0-7) controls a group of pins spaced by 8
// - Bit 0 controls pins: 0, 8, 16, 24, 32, 40, 48
// - Bit 1 controls pins: 1, 9, 17, 25, 33, 41, 49
// - Bit 2 controls pins: 2, 10, 18, 26, 34, 42
// - Bit 3 controls pins: 3, 11, 19, 27, 35, 43
// - Bit 4 controls pins: 4, 12, 20, 28, 36, 44
// - Bit 5 controls pins: 5, 13, 21, 29, 37, 45
// - Bit 6 controls pins: 6, 14, 22, 30, 38, 46
// - Bit 7 controls pins: 7, 15, 23, 31, 39, 47
//
// CRITICAL LIMITATION:
// - Pins sharing the same bit group CANNOT be used together in FastPins
// - For N parallel pins, you MUST use pins from N different bit groups
// - Example: Valid: pins 0,1,2,3,4,5,6,7 (all different groups)
// - Example: INVALID: pins 0,8 (both in group 0, will conflict!)
//
// Pin Group Calculation:
// - group = pin % 8  (or pin & 0x7)
// - Pins must be from different groups for independent control
//
// Register Map (Apollo3 APBDMA Fast GPIO):
//   APBDMA->BBSETCLEAR - Bit-band set/clear register
//     Bits 0-7: SET operation (write 1 to set corresponding bit group HIGH)
//     Bits 8-15: CLEAR operation (write 1 to set corresponding bit group LOW)
//   APBDMA->BBINPUT - Read pin states (bits 0-7 correspond to pin groups)
//
// Pin Encoding:
// - Arduino pin numbers (0-48) map to FastPin<> compile-time templates
// - Runtime detection uses switch statement to bridge to compile-time
// - Each pin belongs to one of 8 bit groups (0-7)
//
// Performance:
// - Same-group mode: N/A (not possible - pins must be from different groups)
// - Multi-group mode: ~30-40ns (single BBSETCLEAR write for all 8 bits)
// - Clock speed: Up to 48 MHz
//
// Supported Boards:
// - SparkFun Artemis (49 pins: 0-48)
// - SparkFun Edge (pins vary by board)
// - SparkFun Thing Plus Artemis
// - SparkFun RedBoard Artemis

#if defined(ARDUINO_APOLLO3_SFE_ARTEMIS) || defined(ARDUINO_APOLLO3_SFE_ARTEMIS_ATP) || \
    defined(ARDUINO_APOLLO3_SFE_ARTEMIS_NANO) || defined(ARDUINO_APOLLO3_SFE_EDGE) || \
    defined(ARDUINO_APOLLO3_SFE_EDGE2) || defined(ARDUINO_APOLLO3_SFE_THING_PLUS) || \
    defined(ARDUINO_APOLLO3_SFE_REDBOARD_ARTEMIS) || defined(ARDUINO_APOLLO3_SFE_REDBOARD_ARTEMIS_ATP) || \
    defined(ARDUINO_APOLLO3_SFE_REDBOARD_ARTEMIS_NANO) || defined(FASTLED_APOLLO3)

#include "fastpin_apollo3.h"

// Include HAL for APBDMA register access
#ifdef ARDUINO
#include "Arduino.h"
#endif

// Forward declare APBDMA structure if not already defined
#ifndef APBDMA
extern "C" {
    struct APBDMA_Type {
        volatile uint32_t BBSETCLEAR;  // Bit-band set/clear register
        volatile uint32_t BBINPUT;     // Bit-band input register
    };
    extern struct APBDMA_Type* APBDMA;
}
#endif

namespace fl {

namespace detail {

/// Helper to get pin bit group (0-7) for an Apollo3 pin
template<uint8_t PIN>
inline uint8_t getApollo3Group() {
    return PIN & 0x7;  // Pin modulo 8 gives bit group
}

/// Helper to get pin bit mask in BBSETCLEAR register
template<uint8_t PIN>
inline uint8_t getApollo3Mask() {
    return 1 << (PIN & 0x7);  // Bit position in BBSETCLEAR
}

/// Runtime pin info lookup using switch statement
/// Bridges runtime pin numbers to compile-time FastPin<> templates
struct PinInfo {
    uint8_t group;   // Bit group (0-7)
    uint8_t mask;    // Bit mask (1<<group)
};

inline PinInfo getPinInfo(uint8_t pin) {
    PinInfo info = {0xFF, 0};  // Invalid group by default

    // Apollo3: Up to 49 pins (0-48) depending on board
    switch (pin) {
        case 0: info.group = getApollo3Group<0>(); info.mask = getApollo3Mask<0>(); break;
        case 1: info.group = getApollo3Group<1>(); info.mask = getApollo3Mask<1>(); break;
        case 2: info.group = getApollo3Group<2>(); info.mask = getApollo3Mask<2>(); break;
        case 3: info.group = getApollo3Group<3>(); info.mask = getApollo3Mask<3>(); break;
        case 4: info.group = getApollo3Group<4>(); info.mask = getApollo3Mask<4>(); break;
        case 5: info.group = getApollo3Group<5>(); info.mask = getApollo3Mask<5>(); break;
        case 6: info.group = getApollo3Group<6>(); info.mask = getApollo3Mask<6>(); break;
        case 7: info.group = getApollo3Group<7>(); info.mask = getApollo3Mask<7>(); break;
        case 8: info.group = getApollo3Group<8>(); info.mask = getApollo3Mask<8>(); break;
        case 9: info.group = getApollo3Group<9>(); info.mask = getApollo3Mask<9>(); break;
        case 10: info.group = getApollo3Group<10>(); info.mask = getApollo3Mask<10>(); break;
        case 11: info.group = getApollo3Group<11>(); info.mask = getApollo3Mask<11>(); break;
        case 12: info.group = getApollo3Group<12>(); info.mask = getApollo3Mask<12>(); break;
        case 13: info.group = getApollo3Group<13>(); info.mask = getApollo3Mask<13>(); break;
        case 14: info.group = getApollo3Group<14>(); info.mask = getApollo3Mask<14>(); break;
        case 15: info.group = getApollo3Group<15>(); info.mask = getApollo3Mask<15>(); break;
        case 16: info.group = getApollo3Group<16>(); info.mask = getApollo3Mask<16>(); break;
        case 17: info.group = getApollo3Group<17>(); info.mask = getApollo3Mask<17>(); break;
        case 18: info.group = getApollo3Group<18>(); info.mask = getApollo3Mask<18>(); break;
        case 19: info.group = getApollo3Group<19>(); info.mask = getApollo3Mask<19>(); break;
        case 20: info.group = getApollo3Group<20>(); info.mask = getApollo3Mask<20>(); break;
        case 21: info.group = getApollo3Group<21>(); info.mask = getApollo3Mask<21>(); break;
        case 22: info.group = getApollo3Group<22>(); info.mask = getApollo3Mask<22>(); break;
        case 23: info.group = getApollo3Group<23>(); info.mask = getApollo3Mask<23>(); break;
        case 24: info.group = getApollo3Group<24>(); info.mask = getApollo3Mask<24>(); break;
        case 25: info.group = getApollo3Group<25>(); info.mask = getApollo3Mask<25>(); break;
        case 26: info.group = getApollo3Group<26>(); info.mask = getApollo3Mask<26>(); break;
        case 27: info.group = getApollo3Group<27>(); info.mask = getApollo3Mask<27>(); break;
        case 28: info.group = getApollo3Group<28>(); info.mask = getApollo3Mask<28>(); break;
        case 29: info.group = getApollo3Group<29>(); info.mask = getApollo3Mask<29>(); break;
        case 30: info.group = getApollo3Group<30>(); info.mask = getApollo3Mask<30>(); break;
        case 31: info.group = getApollo3Group<31>(); info.mask = getApollo3Mask<31>(); break;
        case 32: info.group = getApollo3Group<32>(); info.mask = getApollo3Mask<32>(); break;
        default: break;
    }

    return info;
}

} // namespace detail


//==============================================================================
// FastPinsSamePort<N_PINS> - Apollo3 Fast GPIO control
//==============================================================================

/// FastPinsSamePort: Ultra-fast GPIO control using Apollo3 Fast GPIO
///
/// CRITICAL REQUIREMENTS:
/// - All pins MUST be from DIFFERENT bit groups (group = pin % 8)
/// - Using two pins from the same group will cause conflicts!
/// - Example: Valid: pins 0,1,2,3,4,5,6,7 (all different groups)
/// - Example: INVALID: pins 0,8 (both group 0, will conflict!)
///
/// Apollo3 Fast GPIO Limitation:
/// - The APBDMA BBSETCLEAR register has 8 bits (0-7)
/// - Each bit controls a GROUP of pins, not individual pins
/// - Bit 0 controls pins: 0, 8, 16, 24, 32, 40, 48
/// - Bit 1 controls pins: 1, 9, 17, 25, 33, 41, 49
/// - etc.
/// - This means you cannot independently control pin 0 and pin 8!
///
/// Performance: ~30-40ns per write (single BBSETCLEAR register write)
///
/// Example:
///   FastPinsSamePort<8> pins(0, 1, 2, 3, 4, 5, 6, 7);  // OK: all different groups
///   FastPinsSamePort<8> pins(0, 8, 16, 24, 32, 40, 1, 2);  // ERROR: 0,8,16,24,32,40 all in group 0!
template<int N_PINS>
class FastPinsSamePort {
public:
    /// Constructor: Initialize with pin numbers
    template<typename... Pins>
    FastPinsSamePort(Pins... pins) {
        static_assert(sizeof...(pins) == N_PINS, "Number of pins must match N_PINS");
        uint8_t pin_array[] = {static_cast<uint8_t>(pins)...};
        init(pin_array);
    }

    /// Write value to all pins (1 bit per pin, LSB = first pin)
    inline void write(uint32_t value) const {
        // Build SET and CLEAR masks for BBSETCLEAR register
        // SET: bits 0-7 (set pin HIGH)
        // CLEAR: bits 8-15 (set pin LOW)
        uint32_t bbsetclear = 0;

        for (int i = 0; i < N_PINS; i++) {
            if (value & (1 << i)) {
                // Set bit HIGH: write to SET field (bits 0-7)
                bbsetclear |= (uint32_t)_pin_masks[i];
            } else {
                // Set bit LOW: write to CLEAR field (bits 8-15)
                bbsetclear |= ((uint32_t)_pin_masks[i] << 8);
            }
        }

        // Single atomic write to BBSETCLEAR register
        APBDMA->BBSETCLEAR = bbsetclear;
    }

    /// Write using LUT (optimized for repeated patterns)
    inline void writeLUT(uint32_t lut_index) const {
        APBDMA->BBSETCLEAR = _lut[lut_index];
    }

private:
    void init(const uint8_t* pins) {
        // Get pin info and validate
        uint8_t group_used[8] = {0};  // Track which bit groups are used

        for (int i = 0; i < N_PINS; i++) {
            detail::PinInfo info = detail::getPinInfo(pins[i]);

            if (info.group == 0xFF) {
                // Invalid pin number
                return;
            }

            if (group_used[info.group]) {
                // ERROR: Two pins in the same bit group!
                // This is a critical error for Apollo3 Fast GPIO
                // Cannot proceed - pins will conflict
                return;
            }

            group_used[info.group] = 1;
            _pin_masks[i] = info.mask;

            // Enable Fast GPIO for this pin
            am_hal_gpio_fastgpio_enable(pins[i]);
        }

        // Build LUT for all possible output combinations
        buildLUT();
    }

    void buildLUT() {
        const uint32_t lut_size = (1 << N_PINS);
        for (uint32_t value = 0; value < lut_size; value++) {
            uint32_t bbsetclear = 0;

            for (int i = 0; i < N_PINS; i++) {
                if (value & (1 << i)) {
                    // Set bit HIGH: write to SET field (bits 0-7)
                    bbsetclear |= (uint32_t)_pin_masks[i];
                } else {
                    // Set bit LOW: write to CLEAR field (bits 8-15)
                    bbsetclear |= ((uint32_t)_pin_masks[i] << 8);
                }
            }

            _lut[value] = bbsetclear;
        }
    }

    uint8_t _pin_masks[N_PINS];        // Bit mask for each pin (1<<group)
    uint32_t _lut[1 << N_PINS];        // LUT for all output combinations
};


//==============================================================================
// FastPinsWithClock<N_DATA_PINS> - Apollo3 Fast GPIO with clock pin
//==============================================================================

/// FastPinsWithClock: GPIO control with dedicated clock pin (for SPI-like protocols)
///
/// CRITICAL REQUIREMENTS:
/// - All N_DATA_PINS + clock pin MUST be from DIFFERENT bit groups
/// - Maximum 8 data pins (since 1 bit is used for clock, leaving 7 data bits)
/// - Example: Valid: data pins 0,1,2,3,4,5,6,7 + clock pin 8 (ERROR - clock conflicts with data pin 0!)
/// - Example: Valid: data pins 1,2,3,4,5,6,7 + clock pin 0 (OK - all different groups, max 7 data)
///
/// Performance: ~40-50ns per write (data + clock strobe)
///
/// Example:
///   FastPinsWithClock<7> spi(0, 1, 2, 3, 4, 5, 6, 7);  // 7 data pins, clock on pin 8
///   spi.setPins(1, 2, 3, 4, 5, 6, 7, 0);  // Set data pins and clock pin
///   spi.writeDataAndClock(0x55, 0);  // Write data + clock LOW
///   spi.writeDataAndClock(0x55, 1);  // Write data + clock HIGH
template<int N_DATA_PINS>
class FastPinsWithClock {
public:
    static_assert(N_DATA_PINS <= 7, "Apollo3 Fast GPIO supports max 7 data pins + 1 clock (8 bits total)");

    /// Constructor: Initialize with data pin numbers (clock pin set via setClockPin)
    template<typename... Pins>
    FastPinsWithClock(Pins... pins) {
        static_assert(sizeof...(pins) == N_DATA_PINS, "Number of pins must match N_DATA_PINS");
        uint8_t pin_array[] = {static_cast<uint8_t>(pins)...};
        initData(pin_array);
    }

    /// Set clock pin
    void setClockPin(uint8_t clock_pin) {
        detail::PinInfo info = detail::getPinInfo(clock_pin);

        if (info.group == 0xFF) {
            return;  // Invalid pin
        }

        // Check if clock pin conflicts with data pins
        for (int i = 0; i < N_DATA_PINS; i++) {
            if (_data_groups[i] == info.group) {
                // ERROR: Clock pin in same bit group as data pin!
                return;
            }
        }

        _clock_mask = info.mask;
        _clock_group = info.group;

        // Enable Fast GPIO for clock pin
        am_hal_gpio_fastgpio_enable(clock_pin);

        // Rebuild LUT with clock pin
        buildLUT();
    }

    /// Write data and clock state
    inline void writeDataAndClock(uint8_t data, uint8_t clock_state) const {
        uint32_t bbsetclear = 0;

        // Data pins
        for (int i = 0; i < N_DATA_PINS; i++) {
            if (data & (1 << i)) {
                bbsetclear |= (uint32_t)_data_masks[i];
            } else {
                bbsetclear |= ((uint32_t)_data_masks[i] << 8);
            }
        }

        // Clock pin
        if (clock_state) {
            bbsetclear |= (uint32_t)_clock_mask;
        } else {
            bbsetclear |= ((uint32_t)_clock_mask << 8);
        }

        APBDMA->BBSETCLEAR = bbsetclear;
    }

    /// Write using LUT (optimized for repeated patterns)
    /// lut_index = (data | (clock_state << N_DATA_PINS))
    inline void writeLUT(uint32_t lut_index) const {
        APBDMA->BBSETCLEAR = _lut[lut_index];
    }

private:
    void initData(const uint8_t* pins) {
        uint8_t group_used[8] = {0};

        for (int i = 0; i < N_DATA_PINS; i++) {
            detail::PinInfo info = detail::getPinInfo(pins[i]);

            if (info.group == 0xFF) {
                return;
            }

            if (group_used[info.group]) {
                // ERROR: Two data pins in the same bit group!
                return;
            }

            group_used[info.group] = 1;
            _data_masks[i] = info.mask;
            _data_groups[i] = info.group;

            // Enable Fast GPIO for this pin
            am_hal_gpio_fastgpio_enable(pins[i]);
        }
    }

    void buildLUT() {
        const uint32_t lut_size = (1 << (N_DATA_PINS + 1));  // +1 for clock bit
        for (uint32_t value = 0; value < lut_size; value++) {
            uint32_t bbsetclear = 0;

            // Data bits
            for (int i = 0; i < N_DATA_PINS; i++) {
                if (value & (1 << i)) {
                    bbsetclear |= (uint32_t)_data_masks[i];
                } else {
                    bbsetclear |= ((uint32_t)_data_masks[i] << 8);
                }
            }

            // Clock bit
            if (value & (1 << N_DATA_PINS)) {
                bbsetclear |= (uint32_t)_clock_mask;
            } else {
                bbsetclear |= ((uint32_t)_clock_mask << 8);
            }

            _lut[value] = bbsetclear;
        }
    }

    uint8_t _data_masks[N_DATA_PINS];   // Bit mask for each data pin
    uint8_t _data_groups[N_DATA_PINS];  // Bit group for each data pin (for conflict checking)
    uint8_t _clock_mask;                // Bit mask for clock pin
    uint8_t _clock_group;               // Bit group for clock pin
    uint32_t _lut[1 << (N_DATA_PINS + 1)];  // LUT for all output combinations
};


//==============================================================================
// FastPins<N_PINS> - Auto-detecting Fast GPIO control
//==============================================================================

/// FastPins: Auto-detecting Fast GPIO control
///
/// Apollo3 Behavior:
/// - Always uses Fast GPIO (APBDMA BBSETCLEAR register)
/// - Automatically validates that all pins are from different bit groups
/// - Falls back to slower HAL functions if group conflict detected
///
/// CRITICAL REQUIREMENTS:
/// - All pins MUST be from DIFFERENT bit groups (group = pin % 8)
/// - Fails at initialization if pins conflict
///
/// Performance:
/// - Fast mode: ~30-40ns (when all pins are from different groups)
/// - Fallback mode: ~100-200ns (using HAL functions if conflicts detected)
///
/// Example:
///   FastPins<8> pins(0, 1, 2, 3, 4, 5, 6, 7);  // Fast mode (all different groups)
///   FastPins<2> pins(0, 8);  // Fallback mode (both in group 0, conflict!)
template<int N_PINS>
class FastPins {
public:
    /// Constructor: Initialize with pin numbers
    template<typename... Pins>
    FastPins(Pins... pins) : _fast_mode(false) {
        static_assert(sizeof...(pins) == N_PINS, "Number of pins must match N_PINS");
        uint8_t pin_array[] = {static_cast<uint8_t>(pins)...};
        init(pin_array);
    }

    /// Write value to all pins (1 bit per pin, LSB = first pin)
    inline void write(uint32_t value) const {
        if (_fast_mode) {
            // Fast path: single BBSETCLEAR write
            uint32_t bbsetclear = 0;

            for (int i = 0; i < N_PINS; i++) {
                if (value & (1 << i)) {
                    bbsetclear |= (uint32_t)_pin_masks[i];
                } else {
                    bbsetclear |= ((uint32_t)_pin_masks[i] << 8);
                }
            }

            APBDMA->BBSETCLEAR = bbsetclear;
        } else {
            // Slow fallback: use HAL functions
            for (int i = 0; i < N_PINS; i++) {
                if (value & (1 << i)) {
                    am_hal_gpio_fastgpio_set(_pins[i]);
                } else {
                    am_hal_gpio_fastgpio_clr(_pins[i]);
                }
            }
        }
    }

    /// Write using LUT (only available in fast mode)
    inline void writeLUT(uint32_t lut_index) const {
        if (_fast_mode) {
            APBDMA->BBSETCLEAR = _lut[lut_index];
        } else {
            write(lut_index);
        }
    }

    /// Check if fast mode is enabled
    bool isFastMode() const { return _fast_mode; }

private:
    void init(const uint8_t* pins) {
        uint8_t group_used[8] = {0};
        _fast_mode = true;  // Assume fast mode until proven otherwise

        for (int i = 0; i < N_PINS; i++) {
            _pins[i] = pins[i];
            detail::PinInfo info = detail::getPinInfo(pins[i]);

            if (info.group == 0xFF) {
                _fast_mode = false;
                continue;
            }

            if (group_used[info.group]) {
                // Conflict detected: two pins in same bit group
                _fast_mode = false;
            }

            group_used[info.group] = 1;
            _pin_masks[i] = info.mask;

            // Enable Fast GPIO for this pin
            am_hal_gpio_fastgpio_enable(pins[i]);
        }

        if (_fast_mode) {
            buildLUT();
        }
    }

    void buildLUT() {
        const uint32_t lut_size = (1 << N_PINS);
        for (uint32_t value = 0; value < lut_size; value++) {
            uint32_t bbsetclear = 0;

            for (int i = 0; i < N_PINS; i++) {
                if (value & (1 << i)) {
                    bbsetclear |= (uint32_t)_pin_masks[i];
                } else {
                    bbsetclear |= ((uint32_t)_pin_masks[i] << 8);
                }
            }

            _lut[value] = bbsetclear;
        }
    }

    uint8_t _pins[N_PINS];             // Pin numbers
    uint8_t _pin_masks[N_PINS];        // Bit mask for each pin
    bool _fast_mode;                   // true if fast mode enabled
    uint32_t _lut[1 << N_PINS];        // LUT for all output combinations
};

} // namespace fl

#endif  // defined(ARDUINO_APOLLO3_*) || defined(FASTLED_APOLLO3)

#endif  // __INC_FAST_PINS_APOLLO3_H
