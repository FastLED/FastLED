#pragma once

/// @file init_spi_hw.h
/// @brief ARM platform SPI hardware initialization dispatch
///
/// This header provides the unified initialization entry point for ARM SPI hardware.
/// It dispatches to platform-specific implementations based on coarse-grained detection.
///
/// Platform dispatch pattern:
/// - STM32: spi_hw_manager_stm32.cpp.hpp
/// - Teensy 4.x: spi_hw_manager_mxrt1062.cpp.hpp
/// - RP2040/RP2350: spi_hw_manager_rp.cpp.hpp
/// - SAMD21: spi_hw_manager_samd21.cpp.hpp
/// - SAMD51: spi_hw_manager_samd51.cpp.hpp
/// - nRF52: spi_hw_manager_nrf52.cpp.hpp

// Coarse-grained platform detection headers
#include "platforms/arm/stm32/is_stm32.h"
#include "platforms/arm/teensy/is_teensy.h"
#include "platforms/arm/rp/is_rp.h"
#include "platforms/arm/rp/is_rp2040.h"
#include "platforms/arm/rp/is_rp2350.h"
#include "platforms/arm/samd/is_samd.h"
#include "platforms/arm/nrf52/is_nrf52.h"

#if defined(FL_IS_STM32)
    // STM32 family - use STM32-specific manager
    #include "platforms/arm/stm32/drivers/spi_hw_manager_stm32.cpp.hpp"
#endif

namespace fl {
namespace platform {

#if defined(FL_IS_STM32) || defined(FL_IS_TEENSY_4X) || \
    defined(FL_IS_RP2040) || defined(FL_IS_RP2350) || \
    defined(FL_IS_SAMD51) || defined(FL_IS_SAMD21) || \
    defined(FL_IS_NRF52)

/// @brief Initialize SPI hardware for ARM platforms
///
/// Platform-specific implementations:
/// - STM32: platforms/arm/stm32/spi_hw_manager_stm32.cpp.hpp
/// - Teensy 4.x: platforms/arm/teensy/teensy4_common/spi_hw_manager_mxrt1062.cpp.hpp
/// - RP2040/RP2350: platforms/arm/rp/rpcommon/spi_hw_manager_rp.cpp.hpp
/// - SAMD51: platforms/arm/d51/spi_hw_manager_samd51.cpp.hpp
/// - SAMD21: platforms/arm/d21/spi_hw_manager_samd21.cpp.hpp
/// - nRF52: platforms/arm/nrf52/spi_hw_manager_nrf52.cpp.hpp
void initSpiHardware();

#else

/// @brief No-op SPI hardware initialization for unsupported ARM platforms
inline void initSpiHardware() {
    // No-op for unsupported ARM platforms
}

#endif

}  // namespace platform
}  // namespace fl
