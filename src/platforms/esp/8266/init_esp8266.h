// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/esp/8266/init_esp8266.h
/// @brief ESP8266 platform initialization
///
/// ESP8266 UART driver is initialized per-strip. This provides a hook for
/// future initialization needs (buffer pre-allocation, etc.).

namespace fl {
namespace platforms {

/// @brief ESP8266 platform initialization (minimal)
///
/// ESP8266 UART driver for LED output is initialized per-strip.
/// This function is currently a no-op but provides a hook for future
/// initialization needs such as:
/// - DMA buffer pre-allocation
/// - WiFi state tracking integration
/// - UART hardware resource reservation
inline void init() {
    // Minimal or no-op: UART driver is initialized per-strip
    // Future: Consider pre-allocating DMA buffers or reserving UART resources
}

} // namespace platforms
} // namespace fl
