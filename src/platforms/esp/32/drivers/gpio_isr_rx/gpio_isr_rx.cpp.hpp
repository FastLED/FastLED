// IWYU pragma: private

/**
 * @file gpio_isr_rx.cpp
 * @brief GPIO ISR RX factory method (binds to MCPWM implementation)
 *
 * This file redirects the GpioIsrRx::create() factory method to the
 * dual-ISR MCPWM implementation on platforms with MCPWM hardware.
 * Returns nullptr on platforms without MCPWM (e.g., ESP32-C3/C2).
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "gpio_isr_rx.h"

// Include feature flags
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

// Check if this SoC has MCPWM hardware
#include "soc/soc_caps.h"  // IWYU pragma: keep
#if defined(SOC_MCPWM_SUPPORTED) && SOC_MCPWM_SUPPORTED

#include "gpio_isr_rx_mcpwm.h"

namespace fl {

// Factory method implementation - binds to MCPWM implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) {
    return GpioIsrRxMcpwm_create(pin);
}

} // namespace fl

#else // !SOC_MCPWM_SUPPORTED

namespace fl {

// No MCPWM hardware on this SoC - ISR RX not supported
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) {
    (void)pin;
    return fl::shared_ptr<GpioIsrRx>();
}

} // namespace fl

#endif // SOC_MCPWM_SUPPORTED
#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
