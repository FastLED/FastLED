#pragma once

/// @file init_spi_hw_2.h
/// @brief ARM platform SpiHw2 initialization dispatch
///
/// This header provides lazy initialization for ARM platform dual-lane SPI hardware.
/// It dispatches to the appropriate platform-specific implementation.

// allow-include-after-namespace

#include "platforms/arm/is_arm.h"

#if defined(FL_IS_TEENSY_4X)
    // Teensy 4.x (MXRT1062) has 3 LPSPI peripherals
    namespace fl {
    namespace platform {
    void initSpiHw2Instances();
    }
    }

#elif defined(FL_IS_STM32)
    // STM32 platforms with Timer/DMA-based dual SPI
    #if defined(FASTLED_STM32_HAS_SPI_HW_2)
        namespace fl {
        namespace platform {
        void initSpiHw2Instances();
        }
        }
    #elif !defined(FASTLED_STM32_HAS_SPI_HW_2)
        #include "platforms/shared/init_spi_hw_2.h"
    #endif

#elif defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    // RP2040/RP2350 platforms with PIO-based dual SPI
    namespace fl {
    namespace platform {
    void initSpiHw2Instances();
    }
    }

#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)
    // nRF52 platforms with Timer/PPI-based dual SPI
    namespace fl {
    namespace platform {
    void initSpiHw2Instances();
    }
    }

#elif !defined(FL_IS_TEENSY_4X) && !defined(FL_IS_STM32) && !defined(PICO_RP2040) && !defined(PICO_RP2350) && !defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_RP2350) && !defined(NRF52) && !defined(NRF52832) && !defined(NRF52840) && !defined(NRF52833)
    // For other ARM variants, use the default no-op implementation
    #include "platforms/shared/init_spi_hw_2.h"
#endif
