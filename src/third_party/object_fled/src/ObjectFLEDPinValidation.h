#ifndef OBJECTFLED_PIN_VALIDATION_H
#define OBJECTFLED_PIN_VALIDATION_H

#include "fl/stl/stdint.h"

namespace objectfled {

enum class PinIssueType {
    NONE,
    OUT_OF_RANGE,
    INVALID_GPIO,
    GROUND_PIN,
    POWER_PIN,
    READ_ONLY,
    UART_PIN,
    SPI_PIN,
    I2C_PIN,
    STRAPPING_PIN,
    LED_PIN
};

struct PinValidationResult {
    bool valid;
    PinIssueType issue_type;
    const char* error_message;
};

// Pin validation for Teensy 4.x (iMXRT1062)
// Based on _FL_DEFPIN definitions from fastpin_arm_mxrt1062.h
inline PinValidationResult validate_teensy4_pin(uint8_t pin) {
    // Teensy 4.0 supports pins 0-39
    // Teensy 4.1 adds pins 40-54, but we'll be permissive and allow
    // any pin that could theoretically exist on a Teensy 4.x variant

    // First check: pin number range
    // NUM_DIGITAL_PINS from Teensy core should handle board-specific limits
    // But we can do basic range checking here

    if (pin >= NUM_DIGITAL_PINS) {
        return {false, PinIssueType::OUT_OF_RANGE, "Pin number exceeds NUM_DIGITAL_PINS for this board - pin is out of valid range"};
    }

    // Check if pin maps to a valid GPIO port (GPIO6-9)
    // Use the same logic as ObjectFLED::begin() to verify pin is usable
    uint8_t offset = ((uint32_t)portOutputRegister(pin) - (uint32_t)&GPIO6_DR) >> 14;
    if (offset > 3) {
        return {false, PinIssueType::INVALID_GPIO, "Pin does not map to a valid GPIO port (GPIO6-9) - may be a ground/power/read-only pin"};
    }

    // Warn about potentially problematic pins
    // These are based on common Teensy 4.x pin usage patterns

    // UART pins - noise/signal integrity issues
    if (pin == 0 || pin == 1) {
        return {true, PinIssueType::UART_PIN, "WARNING: Pin is Serial1 UART (0=RX1, 1=TX1) - may cause noise/signal issues and affect serial debugging"};
    }

    if (pin == 7 || pin == 8) {
        return {true, PinIssueType::UART_PIN, "WARNING: Pin is Serial2 UART (7=RX2, 8=TX2) - may cause noise/signal issues"};
    }

    // SPI pins - may interfere with flash/SD card communication
    if (pin == 11 || pin == 12 || pin == 13) {
        return {true, PinIssueType::SPI_PIN, "WARNING: Pin is SPI bus (11=MOSI, 12=MISO, 13=SCK) - may cause noise and conflict with flash/SD peripherals"};
    }

    if (pin == 26 || pin == 27) {
        return {true, PinIssueType::SPI_PIN, "WARNING: Pin is SPI1 bus (26=MOSI1, 27=SCK1) - may cause noise and conflict with peripherals"};
    }

    // I2C pins - may interfere with I2C devices
    if (pin == 18 || pin == 19) {
        return {true, PinIssueType::I2C_PIN, "WARNING: Pin is I2C bus (18=SDA0, 19=SCL0) - may cause noise and conflict with I2C devices"};
    }

    if (pin == 16 || pin == 17) {
        return {true, PinIssueType::I2C_PIN, "WARNING: Pin is I2C bus (16=SCL1, 17=SDA1) - may cause noise and conflict with I2C devices"};
    }

    // LED pin on most Teensy boards
    if (pin == 13) {
        return {true, PinIssueType::LED_PIN, "WARNING: Pin 13 is the onboard LED - may cause unexpected visual feedback"};
    }

    // Pin is valid
    return {true, PinIssueType::NONE, nullptr};
}

// Check if a pin is valid (simple boolean check)
inline bool is_valid_teensy4_pin(uint8_t pin) {
    auto result = validate_teensy4_pin(pin);
    return result.valid;
}

} // namespace objectfled

#endif // OBJECTFLED_PIN_VALIDATION_H
