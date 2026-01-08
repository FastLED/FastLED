#pragma once

/// @file init_spi_hw_1.h
/// @brief Stub platform SpiHw1 initialization for testing
///
/// This header provides lazy initialization for stub/mock SpiHw1 instances
/// used in testing environments.

// allow-include-after-namespace

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

namespace fl {
namespace platform {

/// @brief Initialize stub SpiHw1 instances for testing
///
/// Registers mock SpiHw1 controller instances for testing.
/// Implementation is in src/platforms/stub/spi_1_stub.cpp
void initSpiHw1Instances();

}  // namespace platform
}  // namespace fl

#elif !defined(FASTLED_TESTING) && !defined(FASTLED_STUB_IMPL)

// When not in testing mode, use the default no-op implementation
#include "platforms/shared/init_spi_hw_1.h"

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
