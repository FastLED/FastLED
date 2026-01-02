/*
  FastLED — Cross-Platform ISR Handler API - Implementation
  ---------------------------------------------------------
  Implementation of cross-platform ISR attachment functions.

  License: MIT (FastLED)
*/

#include "isr.h"

namespace fl {
namespace isr {

int attachTimerHandler(const isr_config_t& config, isr_handle_t* handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_attach_timer_handler(config, handle);
#elif defined(ESP32)
    return esp32_attach_timer_handler(config, handle);
#elif defined(FL_IS_TEENSY)
    return teensy_attach_timer_handler(config, handle);
#elif defined(FL_IS_STM32)
    return stm32_attach_timer_handler(config, handle);
#elif defined(FL_IS_NRF52)
    return nrf52_attach_timer_handler(config, handle);
#elif defined(FL_IS_AVR)
    return avr_attach_timer_handler(config, handle);
#elif defined(FL_IS_RP2040)
    return rp2040_attach_timer_handler(config, handle);
#elif defined(FL_IS_SAMD)
    return samd_attach_timer_handler(config, handle);
#else
    return null_attach_timer_handler(config, handle);
#endif
}

} // namespace isr
} // namespace fl
