#pragma once

/// @file init_spi_hw_4.h
/// @brief ARM platform SpiHw4 initialization dispatch
///
/// This header provides lazy initialization for ARM platform quad-lane SPI hardware.
/// It dispatches to the appropriate platform-specific implementation.

// allow-include-after-namespace

#include "platforms/arm/is_arm.h"

#if defined(FL_IS_TEENSY_4X)
    // Teensy 4.x (MXRT1062) has 3 LPSPI peripherals supporting 4-lane mode
    namespace fl {
    namespace platform {
    void initSpiHw4Instances();
    }
    }

#elif defined(FL_IS_STM32)
    // STM32 platforms with Timer/DMA-based quad SPI
    #if defined(FASTLED_STM32_HAS_SPI_HW_4)
        namespace fl {
        namespace platform {
        void initSpiHw4Instances();
        }
        }
    #elif !defined(FASTLED_STM32_HAS_SPI_HW_4)
        #include "platforms/shared/init_spi_hw_4.h"
    #endif

#elif defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    // RP2040/RP2350 platforms with PIO-based quad SPI
    namespace fl {
    namespace platform {
    void initSpiHw4Instances();
    }
    }

#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(NRF52833)
    // nRF52 platforms with Timer/PPI-based quad SPI
    namespace fl {
    namespace platform {
    void initSpiHw4Instances();
    }
    }

#elif defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
      defined(__SAMD51J20A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__) || \
      defined(__SAME51J20A__) || defined(__SAME51N19A__) || defined(__SAME51N20A__)
    // SAMD51 platforms with native QSPI peripheral
    namespace fl {
    namespace platform {
    void initSpiHw4Instances();
    }
    }

#elif !defined(FL_IS_TEENSY_4X) && !defined(FL_IS_STM32) && !defined(PICO_RP2040) && !defined(PICO_RP2350) && !defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_RP2350) && !defined(NRF52) && !defined(NRF52832) && !defined(NRF52840) && !defined(NRF52833) && !defined(__SAMD51G19A__) && !defined(__SAMD51J19A__) && !defined(__SAME51J19A__) && !defined(__SAMD51J20A__) && !defined(__SAMD51P19A__) && !defined(__SAMD51P20A__) && !defined(__SAME51J20A__) && !defined(__SAME51N19A__) && !defined(__SAME51N20A__)
    // For other ARM variants, use the default no-op implementation
    #include "platforms/shared/init_spi_hw_4.h"
#endif
