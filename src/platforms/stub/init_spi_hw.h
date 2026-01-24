#pragma once

/// @file init_spi_hw.h
/// @brief Stub/testing SPI hardware initialization
///
/// This provides initialization for stub/mock SPI hardware instances used in testing.

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {
namespace platform {

/// @brief Initialize stub SPI hardware instances for testing
///
/// Registers all stub/mock SPI hardware instances (SpiHw1, SpiHw2, SpiHw4, SpiHw8, SpiHw16)
/// for testing environments.
///
/// Implementation is in spi_hw_manager_stub.cpp.hpp
void initSpiHardware();

}  // namespace platform
}  // namespace fl

#else

// When not in testing mode, provide no-op implementation
namespace fl {
namespace platform {

/// @brief No-op SPI hardware initialization for non-testing platforms
inline void initSpiHardware() {
    // No-op: Not in testing mode
}

}  // namespace platform
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
