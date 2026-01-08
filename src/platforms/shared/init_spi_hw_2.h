#pragma once

/// @file init_spi_hw_2.h
/// @brief Default (no-op) SpiHw2 initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific 2-lane SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw_2.h header.

namespace fl {
namespace platform {

/// @brief Initialize SpiHw2 instances (no-op for platforms without SpiHw2)
///
/// This is a no-op function for platforms that don't have 2-lane SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHw2Instances() {
    // No-op: This platform doesn't have SpiHw2 hardware
}

}  // namespace platform
}  // namespace fl
