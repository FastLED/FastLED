// ok no namespace fl
#pragma once

#warning "fastpin_stm32f1.h is deprecated. Use fastpin_dispatcher.h instead. See src/platforms/arm/stm32/pins/README.md for migration guide."

// DEPRECATED: This file is deprecated in favor of the new unified architecture
// Please use fastpin_dispatcher.h which routes to the new families/ and boards/ structure
// This file will be removed in a future release

// STM32F1 pin definitions for BluePill, Maple Mini, and similar STM32F1 based boards
// Provides access to all common STM32 GPIO pins using hardware names (PA0-PA15, PB0-PB15, etc.)

#include "fastpin_stm32f1_base.h"

namespace fl {

#define MAX_PIN PB1

// Port B pins
_DEFPIN_ARM(PB11, 11, B);
_DEFPIN_ARM(PB10, 10, B);
_DEFPIN_ARM(PB2, 2, B);
_DEFPIN_ARM(PB0, 0, B);
_DEFPIN_ARM(PB7, 7, B);
_DEFPIN_ARM(PB6, 6, B);
_DEFPIN_ARM(PB5, 5, B);
_DEFPIN_ARM(PB4, 4, B);
_DEFPIN_ARM(PB3, 3, B);
_DEFPIN_ARM(PB15, 15, B);
_DEFPIN_ARM(PB14, 14, B);
_DEFPIN_ARM(PB13, 13, B);
_DEFPIN_ARM(PB12, 12, B);
_DEFPIN_ARM(PB8, 8, B);
_DEFPIN_ARM(PB1, 1, B);

// Port A pins
_DEFPIN_ARM(PA7, 7, A);
_DEFPIN_ARM(PA6, 6, A);
_DEFPIN_ARM(PA5, 5, A);
_DEFPIN_ARM(PA4, 4, A);
_DEFPIN_ARM(PA3, 3, A);
_DEFPIN_ARM(PA2, 2, A);
_DEFPIN_ARM(PA1, 1, A);
_DEFPIN_ARM(PA0, 0, A);
_DEFPIN_ARM(PA15, 15, A);
_DEFPIN_ARM(PA14, 14, A);
_DEFPIN_ARM(PA13, 13, A);
_DEFPIN_ARM(PA12, 12, A);
_DEFPIN_ARM(PA11, 11, A);
_DEFPIN_ARM(PA10, 10, A);
_DEFPIN_ARM(PA9, 9, A);
_DEFPIN_ARM(PA8, 8, A);

// Port C pins
_DEFPIN_ARM(PC15, 15, C);
_DEFPIN_ARM(PC14, 14, C);
_DEFPIN_ARM(PC13, 13, C);

// SPI2 pins (BluePill default)
#define SPI_DATA PB15
#define SPI_CLOCK PB13

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl
