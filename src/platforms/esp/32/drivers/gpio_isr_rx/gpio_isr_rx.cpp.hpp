// IWYU pragma: private

/**
 * @file gpio_isr_rx.cpp
 * @brief GPIO ISR RX factory method (binds to MCPWM implementation)
 *
 * This file redirects the GpioIsrRx::create() factory method to the
 * dual-ISR MCPWM implementation on platforms with MCPWM hardware.
 * Returns nullptr (which the caller wraps as DummyRxDevice) on platforms
 * without MCPWM (e.g., ESP32-C3/C2) OR without RMT5 (e.g., ESP32-C2,
 * which lacks the RMT peripheral entirely).
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx.h"
// IWYU pragma: end_keep
#include "fl/stl/noexcept.h"

// Include feature flags
// IWYU pragma: begin_keep
#include "platforms/esp/32/feature_flags/enabled.h"
// IWYU pragma: end_keep

// soc_caps.h is only available under the ESP-IDF tree; only pull it in when
// we'll actually consult SOC_MCPWM_SUPPORTED (i.e. when FASTLED_RMT5 is on).
#if FASTLED_RMT5
#include "soc/soc_caps.h"  // IWYU pragma: keep
#endif

// The caller in `src/fl/channels/rx.cpp.hpp` is gated only on `FL_IS_ESP32`
// and references `GpioIsrRx::create()` unconditionally. We must therefore
// provide a definition for every ESP32 board — even ones without RMT5 or
// MCPWM (e.g. ESP32-C2 has neither). The MCPWM-backed path only fires
// when both gates are positive; everything else falls into the stub.
// See FastLED #2630.
#if FASTLED_RMT5 && defined(SOC_MCPWM_SUPPORTED) && SOC_MCPWM_SUPPORTED

// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/gpio_isr_rx/gpio_isr_rx_mcpwm.h"
// IWYU pragma: end_keep

namespace fl {

// Factory method implementation - binds to MCPWM implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) FL_NOEXCEPT {
    return GpioIsrRxMcpwm_create(pin);
}

} // namespace fl

#else // !FASTLED_RMT5 OR !SOC_MCPWM_SUPPORTED

namespace fl {

// No MCPWM hardware (or no RMT5 support) on this SoC — ISR RX not
// available. Returning null lets the caller substitute DummyRxDevice
// at runtime rather than failing to link.
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) FL_NOEXCEPT {
    (void)pin;
    return fl::shared_ptr<GpioIsrRx>();
}

} // namespace fl

#endif // FASTLED_RMT5 && SOC_MCPWM_SUPPORTED
#endif // FL_IS_ESP32
