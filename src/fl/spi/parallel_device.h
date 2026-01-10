#pragma once

/// @file spi/parallel_device.h
/// @brief Parallel GPIO SPI device for 1-32 outputs driven from single data stream

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "fl/result.h"
#include "fl/stl/optional.h"
#include "fl/promise.h"  // for fl::Error
#include "fl/spi/config.h"
#include "fl/spi/transaction.h"
#include "fl/numeric_limits.h"
#include "platforms/shared/spi_types.h"  // ok platform headers

// Forward declarations for backends
namespace fl {
    class SpiIsr1;
    class SpiIsr2;
    class SpiIsr4;
    class SpiIsr8;
    class SpiIsr16;
    class SpiIsr32;
    class SpiBlock1;
    class SpiBlock2;
    class SpiBlock4;
    class SpiBlock8;
    class SpiBlock16;
    class SpiBlock32;
}

namespace fl {
namespace spi {

// ============================================================================
// ParallelDevice - 1-32 GPIO pins (single stream → LUT → parallel output)
// ============================================================================

/// @brief Parallel GPIO SPI device (1-32 pins driven from single data stream)
/// @details Uses lookup table (LUT) to map byte values to GPIO pin states.
///          Single data stream drives all pins simultaneously.
///          Backend: SpiIsr* (ISR-driven) or SpiBlock* (bit-bang)
///
/// **Architecture:**
/// - Single data stream (byte sequence)
/// - 256-entry LUT maps each byte value → GPIO pin states
/// - Each data byte controls multiple GPIO pins via SET/CLEAR masks
/// - ISR mode: Timer-driven interrupts (async, ~1.6MHz → 800kHz SPI)
/// - Bit-bang mode: Main thread inline (blocking, higher potential speed)
///
/// **Example:**
/// @code
/// ParallelDevice::Config config;
/// config.clock_pin = 18;
/// config.gpio_pins = {0,1,2,3,4,5,6,7};  // 8 parallel outputs
/// config.mode = SpiParallelMode::AUTO;
/// ParallelDevice spi(config);
/// spi.begin();
///
/// uint8_t data[] = {0xFF, 0x00, 0xAA};  // Same to all 8 pins
/// auto tx = spi.write(data, sizeof(data));
/// tx.wait();
/// @endcode
class ParallelDevice {
public:
    /// @brief Configuration for parallel GPIO SPI
    struct Config {
        uint8_t clock_pin;              ///< Clock pin (SCK)
        fl::vector<uint8_t> gpio_pins;  ///< GPIO pins (1-32 pins)
        SpiParallelMode mode;           ///< Execution mode (ISR vs bit-bang)
        uint32_t timer_hz;              ///< Timer frequency for ISR mode

        Config();  // Implemented in .cpp to avoid circular dependency
    };

    /// @brief Construct parallel device
    /// @param config Configuration with 1-32 GPIO pins
    explicit ParallelDevice(const Config& config);

    /// @brief Destructor - releases hardware resources
    ~ParallelDevice();

    // Disable copy/move
    ParallelDevice(const ParallelDevice&) = delete;
    ParallelDevice& operator=(const ParallelDevice&) = delete;
    ParallelDevice(ParallelDevice&&) = delete;
    ParallelDevice& operator=(ParallelDevice&&) = delete;

    // ========== Initialization ==========

    /// @brief Initialize hardware and setup LUT
    /// @returns Optional error (nullopt on success)
    /// @note Auto-selects backend based on pin count and mode
    fl::optional<fl::Error> begin();

    /// @brief Shutdown hardware and release resources
    void end();

    /// @brief Check if device is initialized
    /// @returns true if initialized and ready
    bool isReady() const;

    // ========== Transmission ==========

    /// @brief Write data (single stream drives all pins via LUT)
    /// @param data Data to transmit
    /// @param size Number of bytes
    /// @returns Result containing Transaction handle or error
    /// @note Each byte value is mapped via LUT to GPIO pin states
    Result<Transaction> write(const uint8_t* data, size_t size);

    /// @brief Wait for pending transmission to complete
    /// @param timeout_ms Maximum time to wait (default: forever)
    /// @returns true if completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = fl::numeric_limits<uint32_t>::max());

    /// @brief Check if transmission is in progress
    /// @returns true if busy, false if idle
    bool isBusy() const;

    // ========== Configuration ==========

    /// @brief Configure custom LUT (advanced)
    /// @param set_masks Array of 256 set masks (GPIO pins to set high for each byte value)
    /// @param clear_masks Array of 256 clear masks (GPIO pins to clear low for each byte value)
    /// @note Default LUT maps byte bits directly to GPIO pins
    /// @note Must be called before begin() or after begin() but before write()
    void configureLUT(const uint32_t* set_masks, const uint32_t* clear_masks);

    /// @brief Get current configuration
    /// @returns Reference to config
    const Config& getConfig() const;

private:
    struct Impl;
    fl::unique_ptr<Impl> pImpl;
};

} // namespace spi
} // namespace fl
