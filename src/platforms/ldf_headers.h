#pragma once

// IWYU pragma: public

// ok no namespace fl

#include "is_platform.h"

/// @file platforms/ldf_headers.h
/// PlatformIO Library Dependency Finder (LDF) hint headers
///
/// This file serves as a trampoline to platform-specific LDF hint headers.
/// These headers use #if 0 blocks to include library headers that PlatformIO's
/// LDF scanner needs to see, without actually compiling the code.
///
/// Why this is needed:
/// PlatformIO's LDF in default mode scans headers without evaluating preprocessor
/// paths. This means it can't detect conditional includes like:
///   #if defined(USE_SPI)
///   #include <SPI.h>
///   #endif
///
/// The LDF sees the #include but doesn't know if it's active, causing dependency
/// resolution failures. By using #if 0 blocks, we hint dependencies to the LDF
/// scanner while ensuring the code never actually compiles.
///
/// Architecture:
/// - This file: Trampoline dispatcher (selects platform-specific header)
/// - platforms/*/ldf_headers.h: Platform-specific LDF hints
///
/// Reference: https://docs.platformio.org/en/latest/librarymanager/ldf.html

#if defined(FL_IS_ESP32)
#  include "esp/32/ldf_headers.h"
#elif defined(FL_IS_ESP8266)
#  include "esp/8266/ldf_headers.h"
#elif defined(FL_IS_AVR)
#  include "avr/ldf_headers.h"
#elif defined(FL_IS_ARM)
#  if defined(FL_IS_TEENSY)
#    include "arm/teensy/ldf_headers.h"
#  elif defined(FL_IS_RP2040)
#    include "arm/rp/ldf_headers.h"
#  elif defined(FL_IS_STM32)
#    include "arm/stm32/ldf_headers.h"
#  elif defined(FL_IS_SAMD)
#    include "arm/samd/ldf_headers.h"
#  elif defined(FL_IS_SAM)
#    include "arm/sam/ldf_headers.h"
#  elif defined(FL_IS_NRF52)
#    include "arm/nrf52/ldf_headers.h"
#  elif defined(FL_IS_RENESAS)
#    include "arm/renesas/ldf_headers.h"
#  elif defined(FL_IS_SILABS)
#    include "arm/silabs/ldf_headers.h"
#  endif
#elif defined(FL_IS_APOLLO3)
#  include "apollo3/ldf_headers.h"
#else
#  // Stub platform - no LDF hints needed
#endif
