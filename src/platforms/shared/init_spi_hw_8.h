#pragma once

/// @file init_spi_hw_8.h
/// @brief Default (no-op) SpiHw8 initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific 8-lane SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw_8.h header.

namespace fl {
namespace platform {

/// @brief Initialize SpiHw8 instances (no-op for platforms without SpiHw8)
///
/// This is a no-op function for platforms that don't have 8-lane SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHw8Instances() {
    // No-op: This platform doesn't have SpiHw8 hardware
}

}  // namespace platform
}  // namespace fl
