#pragma once

// ok no namespace fl

/// @file platforms/pin_probe.h
/// Trampoline for fl/system/pin_probe.h. Included only by
/// fl/system/pin_probe.cpp.hpp.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/pin_probe_esp32.hpp"
#else
#include "platforms/shared/pin_probe_null.hpp"
#endif
