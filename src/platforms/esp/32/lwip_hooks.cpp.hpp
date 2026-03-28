// IWYU pragma: private

/// @file platforms/esp/32/lwip_hooks.cpp.hpp
/// @brief Stub implementations for lwip hook functions on ESP32.
///
/// Certain versions of the pioarduino/ESP-IDF Arduino framework ship a
/// liblwip.a that is built with LWIP_HOOK_IP6_INPUT enabled, which means the
/// lwip ip6_input() function contains a strong reference to
/// lwip_hook_ip6_input(). Normally the esp_netif component supplies this
/// symbol, but in some Arduino framework builds it is missing, causing
/// linker errors ("undefined reference to `lwip_hook_ip6_input'") whenever
/// an application links networking code (e.g. OTA, HTTP server).
///
/// This file provides a minimal stub that lets the packet through
/// unchanged (returns 0 = continue normal IPv6 processing).

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

// Forward-declare lwip types so we don't drag in the full lwip headers
// into every translation unit.  The linker only cares about the symbol
// name and calling convention, not the concrete struct layouts.
struct pbuf;
struct netif;

extern "C" {

/// Stub IPv6 input hook – returns 0 so lwip continues normal processing.
__attribute__((weak))
int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
    (void)p;
    (void)inp;
    return 0; // 0 = continue normal IPv6 processing
}

} // extern "C"

#endif // FL_IS_ESP32
