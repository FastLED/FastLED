#pragma once

/// @file init_spi_hw_16.h
/// @brief Default (no-op) SpiHw16 initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific 16-lane SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw_16.h header.

namespace fl {
namespace platform {

/// @brief Initialize SpiHw16 instances (no-op for platforms without SpiHw16)
///
/// This is a no-op function for platforms that don't have 16-lane SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHw16Instances() {
    // No-op: This platform doesn't have SpiHw16 hardware
}

}  // namespace platform
}  // namespace fl
