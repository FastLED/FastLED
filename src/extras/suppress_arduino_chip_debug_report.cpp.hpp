#pragma once

// =============================================================================
// Suppress the Arduino-ESP32 boot-banner code path. (#2886 Stage 2 / #2893)
//
// On ESP32 targets, the Arduino-ESP32 core's `cores/esp32/main.cpp:43`
// declares:
//
//   __attribute__((weak)) bool shouldPrintChipDebugReport(void);
//
// and unconditionally references `printBeforeSetupInfo()` from the
// `if (shouldPrintChipDebugReport())` branches at main.cpp:55 and :63.
// Even at `ARDUHAL_LOG_LEVEL=0` (release default), the linker keeps
// `printBeforeSetupInfo()` and its ~1-2 KB of formatted-stream rodata
// alive because those call sites are reachable.
//
// Setting `-DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1` in build
// flags emits a strong override of the weak vendor symbol below. The
// optimizer folds both `if (shouldPrintChipDebugReport())` branches to
// dead code, and `--gc-sections` drops `printBeforeSetupInfo()`
// (~1,464 B at top-25 row #21 of #2886) plus its rodata carrier
// strings — ~3 KB of flash on a typical ESP32-S3 NEOPIXEL build.
//
// USAGE (one option):
//
//   1. PlatformIO — in platformio.ini:
//        build_flags = -DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1
//
//   2. Arduino IDE — in a `build_opt.h` or compiler.cpp.extra_flags
//      override:
//        -DFASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT=1
//
// This is opt-in: globally overriding a vendor weak symbol on every
// FastLED user's behalf would be surprising scope creep, and some
// users rely on the boot banner for debugging brownouts / wake-up
// sources / PSRAM detection.
//
// On non-ESP32 targets, or with the flag undefined / set to 0, this
// file expands to nothing.
// =============================================================================

#include "platforms/esp/is_esp.h"  // ok platform headers - for FL_IS_ESP32

#if defined(FL_IS_ESP32) && defined(FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT) \
    && (FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT)

extern "C" bool shouldPrintChipDebugReport(void) { return false; }

#endif  // FL_IS_ESP32 && FASTLED_SUPPRESS_ARDUINO_CHIP_DEBUG_REPORT
