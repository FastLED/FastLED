#pragma once

// ok no namespace fl
// Main dispatcher for STM32 variant-specific pin definitions
// Explicit board registry - single place to see all supported boards
//
// ARCHITECTURE:
// 1. Detect board using ARDUINO_* macros
// 2. Define FASTLED_STM32_BOARD_FILE macro pointing to board-specific pin mapping
// 3. Include family variant file, which includes the board file at the end
//
// TO ADD A NEW BOARD:
// 1. Check board macro: Add `#warning ARDUINO_*` to your sketch and compile
// 2. Create board mapping file in boards/XX/ (see existing boards for examples)
// 3. Add entry below following the pattern
//
// See src/platforms/arm/stm32/pins/README.md for detailed architecture guide

// ========================================
// STM32F1 Family (HAS_BRR = true)
// ========================================

#if defined(__STM32F1__)
  // Maple Mini (libmaple)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f1/maple_mini.h"
  #include "families/stm32f1.h"  // nolint

#elif defined(ARDUINO_GENERIC_F103C8TX)
  // Generic STM32F103C8 (STM32duino)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f1/generic_f103c8.h"
  #include "families/stm32f1.h"  // nolint

#elif defined(STM32F1) && !defined(SPARK)
  // Generic STM32F1 fallback (STM32duino)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f1/generic_f103c8.h"
  #include "families/stm32f1.h"  // nolint

// ========================================
// STM32F2 Family (HAS_BRR = false)
// ========================================

#elif defined(SPARK)
  // Spark Core (STM32F103 with F2-style registers)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f2/spark_core.h"
  #include "families/stm32f2.h"  // nolint

#elif defined(STM32F2XX)
  // Particle Photon (STM32F205)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f2/photon.h"
  #include "families/stm32f2.h"  // nolint

// ========================================
// STM32F4 Family (HAS_BRR = false)
// ========================================

#elif defined(ARDUINO_BLACKPILL_F411CE)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f411ce_blackpill.h"
  #include "families/stm32f4.h"  // nolint

#elif defined(ARDUINO_NUCLEO_F411RE)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f411re_nucleo.h"
  #include "families/stm32f4.h"  // nolint

#elif defined(ARDUINO_BLACKPILL_F401CC) || defined(ARDUINO_BLACKPILL_F401CE)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f401cx_blackpill.h"
  #include "families/stm32f4.h"  // nolint

#elif defined(ARDUINO_NUCLEO_F401RE)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f401re_nucleo.h"
  #include "families/stm32f4.h"  // nolint

#elif defined(ARDUINO_DISCO_F407VG)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f407vg_disco.h"
  #include "families/stm32f4.h"  // nolint

#elif defined(ARDUINO_NUCLEO_F446RE)
  #define FASTLED_STM32_BOARD_FILE "platforms/arm/stm32/pins/boards/f4/f446re_nucleo.h"
  #include "families/stm32f4.h"  // nolint

// ========================================
// Unknown Board - Error with Guidance
// ========================================

#else
  #error "STM32: Unknown board. Check your ARDUINO_* board macro and add it to src/platforms/arm/stm32/pins/fastpin_dispatcher.h. See src/platforms/arm/stm32/pins/README.md for architecture guide and examples."
#endif
