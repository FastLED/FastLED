/// @file debug_setup.cpp
/// Platform-specific debug initialization dispatch
///
/// This file routes to platform-specific debug setup implementations.
/// Currently only ESP32 platforms are supported.

// ok no namespace fl

#if defined(FASTLED_DEBUG)
    #if defined(ESP32)
        // ESP32 platforms have debug setup implementation
        #include "platforms/esp/32/debug_setup.hpp"
    #else
        // Warn about missing debug_setup implementation for this platform
        #warning "debug_setup.cpp is not implemented for this platform. FASTLED_DEBUG will have no effect."
    #endif
#endif
