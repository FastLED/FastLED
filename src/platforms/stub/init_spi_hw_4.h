#pragma once

/// @file init_spi_hw_4.h
/// @brief Stub platform SpiHw4 initialization for testing
///
/// This header provides lazy initialization for stub/mock SpiHw4 instances
/// used in testing environments.

// allow-include-after-namespace

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {
namespace platform {

/// @brief Initialize stub SpiHw4 instances for testing
///
/// Registers mock SpiHw4 controller instances for testing.
/// Implementation is in src/platforms/stub/spi_4_stub.cpp
void initSpiHw4Instances();

}  // namespace platform
}  // namespace fl

#elif !defined(FASTLED_TESTING) && !defined(FASTLED_STUB_IMPL)

// When not in testing mode, use the default no-op implementation
#include "platforms/shared/init_spi_hw_4.h"

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
