#pragma once

/// @file platforms/esp/32/drivers/cled.h
/// ESP32 Custom LED (CLED) driver for high-resolution PWM control
///
/// Provides high-resolution LED control using ESP32 LEDC peripheral:
/// - Up to 20-bit resolution (configurable)
/// - Configurable frequency
/// - Sink/source pin configuration
/// - Consistent 16-bit input interface with automatic scaling

#include "fl/stl/stdint.h"

namespace fl {
namespace esp32 {

/// CLED configuration structure
struct CLEDConfig {
    int pin;                    ///< GPIO pin number
    uint8_t channel;            ///< LEDC channel (0-15)
    uint32_t frequency;         ///< PWM frequency in Hz
    uint8_t resolution_bits;    ///< PWM resolution in bits (1-20, hardware-dependent)
    bool is_sink;               ///< True if LED cathode is connected to pin (active low)
};

/// ESP32 Custom LED controller
/// Accepts 16-bit input (0-65535) and scales to configured resolution
/// Users apply gamma correction upstream before calling write16()
class CLED {
public:
    /// Default constructor
    CLED();

    /// Destructor - releases LEDC resources
    ~CLED();

    /// Initialize CLED with configuration
    /// @param config CLED configuration structure
    /// @return true if initialization succeeded
    bool begin(const CLEDConfig& config);

    /// Release resources and shutdown
    void end();

    /// Write 16-bit brightness value (0-65535)
    /// Automatically scales to configured resolution
    /// Apply gamma correction upstream if needed
    /// @param value Brightness (0-65535)
    void write16(uint16_t value);

    /// Get maximum duty cycle value for current resolution
    /// @return Maximum duty cycle value (e.g., 16383 for 14-bit)
    uint32_t getMaxDuty() const;

    /// Get current resolution in bits
    /// @return Resolution in bits
    uint8_t getResolutionBits() const;

private:
    CLEDConfig mConfig;
    uint32_t mMaxDuty;
    bool mInitialized;

    /// Map 16-bit value to current resolution
    /// @param val16 16-bit input value (0-65535)
    /// @return Mapped duty cycle value for hardware
    uint32_t mapToDutyCycle(uint16_t val16) const;
};

}  // namespace esp32
}  // namespace fl
