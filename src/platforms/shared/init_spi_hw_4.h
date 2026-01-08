#pragma once

/// @file init_spi_hw_4.h
/// @brief Default (no-op) SpiHw4 initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific 4-lane SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw_4.h header.

namespace fl {
namespace platform {

/// @brief Initialize SpiHw4 instances (no-op for platforms without SpiHw4)
///
/// This is a no-op function for platforms that don't have 4-lane SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHw4Instances() {
    // No-op: This platform doesn't have SpiHw4 hardware
}

}  // namespace platform
}  // namespace fl
