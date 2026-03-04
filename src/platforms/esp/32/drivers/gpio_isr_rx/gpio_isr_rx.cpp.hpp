// IWYU pragma: private

/**
 * @file gpio_isr_rx.cpp
 * @brief GPIO ISR RX factory method (binds to MCPWM implementation)
 *
 * This file redirects the GpioIsrRx::create() factory method to the
 * dual-ISR MCPWM implementation.
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "gpio_isr_rx.h"
#include "gpio_isr_rx_mcpwm.h"

// Include feature flags
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

namespace fl {

// Factory method implementation - binds to MCPWM implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) {
    return GpioIsrRxMcpwm_create(pin);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // FL_IS_ESP32
