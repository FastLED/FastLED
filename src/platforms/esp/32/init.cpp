// src/platforms/esp/32/init.cpp
//
// ESP32 Platform Initialization
//
// FIXES WINDOWS USB DISCONNECT ISSUE. If the watchdog fires normally then the USB line
// is not held low. This causes Windows to think the device is dead and won't reconnect.
// This file automatically installs the watchdog with USB disconnect fix.
//
// This file provides automatic initialization hooks for ESP32 platforms.
// Functions here run during C++ static initialization (before main/setup).
//
// Conditional compilation:
// - Always active on ESP32 platforms (not limited to debug builds)
// - Define FL_NO_ESP_WATCHDOG_OVERRIDE to disable automatic watchdog installation
// - Warnings only appear in debug builds (FL_WARN uses FL_DBG)

#ifdef FL_IS_ESP32

#ifndef FL_NO_ESP_WATCHDOG_OVERRIDE

#include "fl/compiler_control.h"
#include "watchdog_esp32.h"

namespace fl {
namespace detail {

// Initialization function that runs before main()
// Sets up watchdog with default configuration for automatic monitoring
void esp32_init() {
    // Install watchdog with default 5 second timeout
    // This provides automatic proof-of-life monitoring for the main loop
    // and installs the panic handler override for USB disconnect
    fl::watchdog_setup(5000);  // 5 second default timeout
}

} // namespace detail
} // namespace fl

// Use FL_INIT to run esp32_init() during static initialization (before main/setup)
// This ensures the watchdog and panic handler override are installed early
// Always active on ESP32 platforms (not limited to debug builds)
FL_INIT(fl::detail::esp32_init);

#endif // !defined(FL_NO_ESP_WATCHDOG_OVERRIDE)

#endif // FL_IS_ESP32
