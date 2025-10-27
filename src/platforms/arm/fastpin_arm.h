/// @file platforms/arm/fastpin_arm.h
/// ARM platform fastpin dispatcher
///
/// Selects the appropriate fastpin implementation for different ARM-based
/// microcontroller families (Teensy, Arduino, STM32, nRF, SAMD, RP2040, etc.)

// ok no namespace fl
#pragma once

#if defined(NRF51)
    #include "platforms/arm/nrf51/fastpin_arm_nrf51.h"
#elif defined(NRF52_SERIES)
    #include "platforms/arm/nrf52/fastpin_arm_nrf52.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__)
    // Teensy 3.0/3.1/3.2 (MK20DX Cortex-M4)
    #include "platforms/arm/teensy/teensy31_32/fastpin_arm_k20.h"
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
    // Teensy 3.5/3.6 (MK64/MK66 Cortex-M4)
    #include "platforms/arm/teensy/teensy36/fastpin_arm_k66.h"
#elif defined(__MKL26Z64__)
    // Teensy LC (MKL26Z64 Cortex-M0+)
    #include "platforms/arm/teensy/teensy_lc/fastpin_arm_kl26.h"
#elif defined(__IMXRT1062__)
    // Teensy 4.0/4.1 (IMXRT1062 Cortex-M7)
    #include "platforms/arm/teensy/teensy4_common/fastpin_arm_mxrt1062.h"
#elif defined(__SAM3X8E__)
    // Arduino Due (SAM3X8E)
    #include "platforms/arm/sam/fastpin_arm_sam.h"
#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || defined(STM32F2XX) || defined(STM32F4)
    // STM32 microcontrollers
    #include "platforms/arm/stm32/fastpin_arm_stm32.h"
#elif defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__)
    // Microchip SAMD21
    #include "platforms/arm/d21/fastpin_arm_d21.h"
#elif defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
    // Microchip SAMD51/SAME51
    #include "platforms/arm/d51/fastpin_arm_d51.h"
#elif defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2350)
    // RP2350 (Raspberry Pi Pico 2)
    #include "platforms/arm/rp/rp2350/fastpin_arm_rp2350.h"
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040)
    // RP2040 (Raspberry Pi Pico)
    #include "platforms/arm/rp/rp2040/fastpin_arm_rp2040.h"
#elif defined(ARDUINO_ARCH_RENESAS)
    // Renesas RA4M1 (Arduino UNO R4)
    #include "platforms/arm/renesas/fastpin_arm_renesas.h"
#elif defined(ARDUINO_GIGA) || defined(ARDUINO_GIGA_M7) || defined(STM32H7)
    // Arduino GIGA (STM32H747) or any STM32H7 board with Arduino framework
    #include "platforms/arm/giga/fastpin_arm_giga.h"
#elif defined(ARDUINO_ARCH_SILABS)
    // Silicon Labs EFM32 (MGM240P, etc.)
    #include "platforms/arm/mgm240/fastpin_arm_mgm240.h"
#endif
