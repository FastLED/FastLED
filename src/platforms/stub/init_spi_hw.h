#pragma once
#include "fl/stl/noexcept.h"

// IWYU pragma: private

/// @file init_spi_hw.h
/// @brief Stub/testing SPI hardware initialization
///
/// This provides initialization for stub/mock SPI hardware instances used in testing.

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {
namespace platforms {

/// @brief Initialize stub SPI hardware instances for testing
///
/// Registers all stub/mock SPI hardware instances (SpiHw1, SpiHw2, SpiHw4, SpiHw8, SpiHw16)
/// for testing environments.
///
/// Implementation is in spi_hw_manager_stub.cpp.hpp
void initSpiHardware() FL_NOEXCEPT;

}  // namespace platforms
}  // namespace fl

#else

// When not in testing mode, provide no-op implementation
namespace fl {
namespace platforms {

/// @brief No-op SPI hardware initialization for non-testing platforms
inline void initSpiHardware() FL_NOEXCEPT {
    // No-op: Not in testing mode
}

}  // namespace platforms
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
