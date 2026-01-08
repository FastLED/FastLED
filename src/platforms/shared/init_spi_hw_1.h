#pragma once

/// @file init_spi_hw_1.h
/// @brief Default (no-op) SpiHw1 initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific 1-lane SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw_1.h header.

namespace fl {
namespace platform {

/// @brief Initialize SpiHw1 instances (no-op for platforms without SpiHw1)
///
/// This is a no-op function for platforms that don't have 1-lane SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHw1Instances() {
    // No-op: This platform doesn't have SpiHw1 hardware
}

}  // namespace platform
}  // namespace fl
