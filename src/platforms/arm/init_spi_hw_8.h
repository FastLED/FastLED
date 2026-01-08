#pragma once

/// @file init_spi_hw_8.h
/// @brief ARM platform SpiHw8 initialization dispatch
///
/// This header provides lazy initialization for ARM platform 8-lane SPI hardware.
/// It dispatches to the appropriate platform-specific implementation.

// allow-include-after-namespace

#include "platforms/arm/is_arm.h"

#if defined(FL_IS_STM32)
    // STM32 platforms with Timer/DMA-based 8-lane SPI
    #if defined(FASTLED_STM32_HAS_SPI_HW_8)
        namespace fl {
        namespace platform {
        void initSpiHw8Instances();
        }
        }
    #elif !defined(FASTLED_STM32_HAS_SPI_HW_8)
        #include "platforms/shared/init_spi_hw_8.h"
    #endif

#elif defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    // RP2040/RP2350 platforms with PIO-based 8-lane SPI
    namespace fl {
    namespace platform {
    void initSpiHw8Instances();
    }
    }

#elif !defined(FL_IS_STM32) && !defined(PICO_RP2040) && !defined(PICO_RP2350) && !defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_RP2350)
    // For other ARM variants, use the default no-op implementation
    #include "platforms/shared/init_spi_hw_8.h"
#endif
