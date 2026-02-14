// ok no namespace fl
#pragma once

// IWYU pragma: private

#warning "fastpin_stm32_spark.h is deprecated. Use fastpin_dispatcher.h instead. See src/platforms/arm/stm32/pins/README.md for migration guide."

// DEPRECATED: This file is deprecated in favor of the new unified architecture
// Please use fastpin_dispatcher.h which routes to the new families/ and boards/ structure
// This file will be removed in a future release

// Spark Core/Photon pin definitions
// Reference: https://docs.particle.io/reference/hardware/wifi/spark-core/

#include "pin_def_stm32.h"

namespace fl {

// Spark-specific register access
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _IO32(L) _RD32(GPIO ## L)

_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E); _IO32(F); _IO32(G);

#define MAX_PIN 19
_DEFPIN_ARM(0, 7, B);
_DEFPIN_ARM(1, 6, B);
_DEFPIN_ARM(2, 5, B);
_DEFPIN_ARM(3, 4, B);
_DEFPIN_ARM(4, 3, B);
_DEFPIN_ARM(5, 15, A);
_DEFPIN_ARM(6, 14, A);
_DEFPIN_ARM(7, 13, A);
_DEFPIN_ARM(8, 8, A);
_DEFPIN_ARM(9, 9, A);
_DEFPIN_ARM(10, 0, A);
_DEFPIN_ARM(11, 1, A);
_DEFPIN_ARM(12, 4, A);
_DEFPIN_ARM(13, 5, A);
_DEFPIN_ARM(14, 6, A);
_DEFPIN_ARM(15, 7, A);
_DEFPIN_ARM(16, 0, B);
_DEFPIN_ARM(17, 1, B);
_DEFPIN_ARM(18, 3, A);
_DEFPIN_ARM(19, 2, A);

#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl
