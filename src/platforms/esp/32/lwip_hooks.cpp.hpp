// IWYU pragma: private

/// @file platforms/esp/32/lwip_hooks.cpp.hpp
/// @brief Provides required LwIP hooks for ESP32 with WiFi support
///
/// Some versions of framework-arduinoespressif32-libs compile the LwIP
/// library with CONFIG_LWIP_HOOK_IP6_INPUT=CUSTOM, which requires the
/// application to provide a lwip_hook_ip6_input() function.
///
/// Without this definition, linking fails with:
///   undefined reference to `lwip_hook_ip6_input'
///
/// This occurs for esp32c3 when using framework-arduinoespressif32-libs
/// @ 5.3.0+sha.083aad99cf (pioarduino platform 53.03.10).
///
/// The hook is called for every incoming IPv6 packet.
/// Returning 0 lets normal LwIP IPv6 processing continue.
///
/// Gated by FL_HAS_NETWORK (from feature_flags/enabled.h), which is 1 only
/// on ESP32 variants that have WiFi silicon (SOC_WIFI_SUPPORTED=1) with
/// ESP-IDF 4.0+. Non-WiFi chips (ESP32-H2, ESP32-P4) and pre-4.0 IDF
/// toolchains are excluded automatically.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"

#if FL_HAS_NETWORK

#ifdef __cplusplus
extern "C" {
#endif

/// @brief LwIP IPv6 input hook required when CONFIG_LWIP_HOOK_IP6_INPUT=CUSTOM.
///
/// This function is called by LwIP for each incoming IPv6 packet before
/// normal IPv6 processing. Returning 0 tells LwIP to continue with its
/// default processing; returning non-zero means the packet was consumed
/// by the hook and LwIP should skip its normal handling.
///
/// @param p    Incoming packet buffer (pbuf chain)
/// @param inp  Network interface on which the packet was received
/// @return     0 to continue normal LwIP IPv6 processing
///
/// Note: We use void* to avoid pulling in lwip/pbuf.h and lwip/netif.h,
/// which require complex include-path ordering under the unity-build system.
/// The function signature is ABI-compatible: both parameters are pointers
/// (same size as void*) and the return value is a single byte integer.
///
/// FL_LINK_WEAK allows user code or a real network stack component to
/// override this default pass-through implementation.
FL_LINK_WEAK int lwip_hook_ip6_input(void *p, void *inp) {
    (void)p;
    (void)inp;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif  // FL_HAS_NETWORK

#endif  // FL_IS_ESP32
