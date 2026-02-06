#pragma once

/// @file init_spi_hw.h
/// @brief Default (no-op) SPI hardware initialization
///
/// This is the fallback implementation for platforms that don't have
/// platform-specific SPI hardware. Platform-specific implementations
/// should override this by providing their own init_spi_hw.h header.

namespace fl {
namespace platforms {

/// @brief Initialize SPI hardware instances (no-op for platforms without SPI hardware)
///
/// This is a no-op function for platforms that don't have SPI hardware.
/// Platform-specific implementations should replace this with actual registration code.
inline void initSpiHardware() {
    // No-op: This platform doesn't have SPI hardware
}

}  // namespace platforms
}  // namespace fl
