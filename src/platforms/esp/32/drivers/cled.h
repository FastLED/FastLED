#pragma once

// IWYU pragma: private

/// @file platforms/esp/32/drivers/cled.h
/// ESP32 Custom LED (CLED) driver for high-resolution PWM control
///
/// Provides high-resolution LED control using ESP32 LEDC peripheral:
/// - Up to 20-bit resolution (configurable)
/// - Configurable frequency
/// - Sink/source pin configuration
/// - Consistent 16-bit input interface with automatic scaling

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace esp32 {

/// CLED configuration structure
struct CLEDConfig {
    int pin;                    ///< GPIO pin number
    u8 channel;            ///< LEDC channel (0-15)
    u32 frequency;         ///< PWM frequency in Hz
    u8 resolution_bits;    ///< PWM resolution in bits (1-20, hardware-dependent)
    bool is_sink;               ///< True if LED cathode is connected to pin (active low)
};

/// ESP32 Custom LED controller
/// Accepts 16-bit input (0-65535) and scales to configured resolution
/// Users apply gamma correction upstream before calling write16()
class CLED {
public:
    /// Default constructor
    CLED() FL_NOEXCEPT;

    /// Destructor - releases LEDC resources
    ~CLED();

    /// Initialize CLED with configuration
    /// @param config CLED configuration structure
    /// @return true if initialization succeeded
    bool begin(const CLEDConfig& config) FL_NOEXCEPT;

    /// Release resources and shutdown
    void end() FL_NOEXCEPT;

    /// Write 16-bit brightness value (0-65535)
    /// Automatically scales to configured resolution
    /// Apply gamma correction upstream if needed
    /// @param value Brightness (0-65535)
    void write16(u16 value) FL_NOEXCEPT;

    /// Get maximum duty cycle value for current resolution
    /// @return Maximum duty cycle value (e.g., 16383 for 14-bit)
    u32 getMaxDuty() const FL_NOEXCEPT;

    /// Get current resolution in bits
    /// @return Resolution in bits
    u8 getResolutionBits() const FL_NOEXCEPT;

private:
    CLEDConfig mConfig;
    u32 mMaxDuty;
    bool mInitialized;

    /// Map 16-bit value to current resolution
    /// @param val16 16-bit input value (0-65535)
    /// @return Mapped duty cycle value for hardware
    u32 mapToDutyCycle(u16 val16) const FL_NOEXCEPT;
};

}  // namespace esp32
}  // namespace fl
