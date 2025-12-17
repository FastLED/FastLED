#pragma once

/// @file spi/config.h
/// @brief Configuration structure for SPI communication

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"

namespace fl {

// ============================================================================
// SPI Constants
// ============================================================================

/// @brief Maximum number of SPI lanes supported (hardware: 1-8, software: up to 32)
constexpr size_t MAX_SPI_LANES = 32;

// ============================================================================
// SPI Output Modes
// ============================================================================

/// @brief SPI output mode for multi-lane devices
enum spi_output_mode_t : uint8_t {
    SPI_AUTO = 0,  ///< Auto-selects best backend (DMA/bit-bang/ISR)
    SPI_HW,        ///< Use DMA-capable hardware (Async or Sync), supports 1/2/4/8 lanes depending on platform
    SPI_BITBANG,   ///< Use bit-bang software (Blocking)
    SPI_ISR,       ///< Use ISR-based software (Async)
};

/// @brief Parallel device execution modes
enum class SpiParallelMode : uint8_t {
    AUTO = 0,          ///< Auto-select best mode (default: ISR)
    ISR_ASYNC,         ///< ISR-driven async mode
    BITBANG_BLOCKING   ///< Bit-bang blocking mode
};

// ============================================================================
// SPI Configuration
// ============================================================================

/// @brief Configuration for SPI device (supports 1-8 lanes)
struct SpiConfig {
    SpiConfig() = default;

    /// @brief Construct single-lane SPI config
    SpiConfig(int clk, int data, uint32_t speed_hz = 0xffffffff, spi_output_mode_t output_mode = SPI_AUTO, uint8_t spi_mode = 0)
        : clock_pin(clk)
        , clock_speed_hz(speed_hz)
        , output_mode(output_mode)
        , spi_mode(spi_mode) {
        data_pins.push_back(data);
    }

    /// @brief Construct multi-lane SPI config
    SpiConfig(int clk, fl::span<const int> pins, uint32_t speed_hz = 0xffffffff, spi_output_mode_t output_mode = SPI_AUTO, uint8_t spi_mode = 0)
        : clock_pin(clk)
        , clock_speed_hz(speed_hz)
        , output_mode(output_mode)
        , spi_mode(spi_mode) {
        // Manually copy pins to data_pins vector
        for (size_t i = 0; i < pins.size(); i++) {
            data_pins.push_back(pins[i]);
        }
    }

    /// @brief Check if this is a multi-lane configuration
    bool isMultiLane() const { return data_pins.size() > 1; }

    int clock_pin;                          ///< SCK pin number
    fl::vector<int> data_pins;              ///< Data pins (1 = single-lane, 2-8 = multi-lane)
    uint32_t clock_speed_hz = 0xffffffff;   ///< Clock frequency in Hz (0xffffffff = as fast as possible)
    spi_output_mode_t output_mode = SPI_AUTO; ///< Output mode (auto/hw/bitbang/isr)
    uint8_t spi_mode = 0;                   ///< SPI mode 0-3 (CPOL/CPHA)
};

namespace spi {

// Use the top-level SpiConfig as the internal Config
// This avoids duplication and ensures consistency
using Config = fl::SpiConfig;

} // namespace spi
} // namespace fl
