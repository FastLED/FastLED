#pragma once
// ok no namespace fl — platform dispatch header

/// @file platforms/watchdog_boot_guard.h
/// @brief Platform dispatch for the early-boot loop guard (fl/wdt/boot_guard.h).
///
/// A platform that supports the guard defines
/// `FL_WATCHDOG_BOOT_GUARD_IMPL(escape_boots, early_timeout_ms)`, which must
/// expand to its earliest startup hook. Platforms without one define nothing,
/// and `FL_WATCHDOG_BOOT_GUARD` only checks its arguments at compile time.

#include "platforms/is_platform.h"
#include "fl/stl/has_include.h"

// Same gate as the watchdog dispatcher (platforms/watchdog.impl.cpp.hpp), so
// the guard exists exactly where watchdog_mxrt1062.impl.hpp backs it.
#if defined(FL_IS_TEENSY_4X) && FL_HAS_INCLUDE(<imxrt.h>)
    #include "platforms/arm/mxrt1062/watchdog_boot_guard_mxrt1062.h"
#endif
