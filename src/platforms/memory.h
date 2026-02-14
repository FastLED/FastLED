#pragma once

// IWYU pragma: public

/// @file platforms/memory.h
/// Platform-specific memory statistics interface
/// Dispatches to platform-specific implementations that define fl_platform_* functions

// ok no namespace fl - This is a dispatcher header that includes platform implementations

#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

// Platform-specific implementation (defines fl_platform_getFreeHeap, etc.)
#if defined(FL_IS_ESP32)
    #include "platforms/esp/memory_esp32.hpp"
#elif defined(FL_IS_ESP8266)
    #include "platforms/esp/memory_esp8266.hpp"
#else
    #include "platforms/shared/memory_noop.hpp"
#endif
