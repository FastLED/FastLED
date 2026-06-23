// IWYU pragma: private

/// @file spi_hw_manager_stub.cpp.hpp
/// @brief Stub platform SPI hardware initialization
///
/// This file provides initialization for stub/mock SPI hardware instances used in testing.
/// All stub SPI classes (SpiHw1, 2, 4, 8, 16) are initialized together to prevent
/// duplicate registration.

#if defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)

#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "platforms/shared/spi_hw_16.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Forward declare stub initialization functions from spi_*_stub.cpp.hpp
void initSpiHw1Instances() FL_NOEXCEPT;
void initSpiHw2Instances() FL_NOEXCEPT;
void initSpiHw4Instances() FL_NOEXCEPT;
void initSpiHw8Instances() FL_NOEXCEPT;
void initSpiHw16Instances() FL_NOEXCEPT;

/// @brief Initialize stub SPI hardware instances for testing
///
/// Registers all stub/mock SPI hardware instances (SpiHw1, SpiHw2, SpiHw4, SpiHw8, SpiHw16)
/// for testing environments.
///
/// Uses a static initialization flag to ensure initialization happens only once,
/// even when called from multiple SpiHw* classes.
void initSpiHardware() FL_NOEXCEPT {
    // C++11 guarantees thread-safe static initialization
    static bool sInitialized = false;
    if (sInitialized) {
        return;  // Already initialized
    }
    sInitialized = true;

    // Register all stub instances
    initSpiHw1Instances();
    initSpiHw2Instances();
    initSpiHw4Instances();
    initSpiHw8Instances();
    initSpiHw16Instances();
}

}  // namespace platforms
}  // namespace fl

#endif  // defined(FASTLED_TESTING) || defined(FASTLED_STUB_IMPL)
