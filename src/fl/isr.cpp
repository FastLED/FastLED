/*
  FastLED — Cross-Platform ISR Handler API - Implementation
  ---------------------------------------------------------
  Implementation of cross-platform ISR attachment functions.

  License: MIT (FastLED)
*/

#include "isr.h"

namespace fl {
namespace isr {

// =============================================================================
// Platform-Specific Function Declarations (Internal)
// =============================================================================

// Platform implementations provide these free functions:
// Each platform (null, stub, esp32, teensy, stm32, etc.) provides its own implementation

// Null platform (default/fallback)
int null_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int null_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int null_detach_handler(isr_handle_t& handle);
int null_enable_handler(const isr_handle_t& handle);
int null_disable_handler(const isr_handle_t& handle);
bool null_is_handler_enabled(const isr_handle_t& handle);
const char* null_get_error_string(int error_code);
const char* null_get_platform_name();
uint32_t null_get_max_timer_frequency();
uint32_t null_get_min_timer_frequency();
uint8_t null_get_max_priority();
bool null_requires_assembly_handler(uint8_t priority);

// Stub platform (testing/simulation)
int stub_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int stub_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int stub_detach_handler(isr_handle_t& handle);
int stub_enable_handler(const isr_handle_t& handle);
int stub_disable_handler(const isr_handle_t& handle);
bool stub_is_handler_enabled(const isr_handle_t& handle);
const char* stub_get_error_string(int error_code);
const char* stub_get_platform_name();
uint32_t stub_get_max_timer_frequency();
uint32_t stub_get_min_timer_frequency();
uint8_t stub_get_max_priority();
bool stub_requires_assembly_handler(uint8_t priority);

// ESP32 platform
int esp32_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int esp32_attach_external_handler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle);
int esp32_detach_handler(isr_handle_t& handle);
int esp32_enable_handler(const isr_handle_t& handle);
int esp32_disable_handler(const isr_handle_t& handle);
bool esp32_is_handler_enabled(const isr_handle_t& handle);
const char* esp32_get_error_string(int error_code);
const char* esp32_get_platform_name();
uint32_t esp32_get_max_timer_frequency();
uint32_t esp32_get_min_timer_frequency();
uint8_t esp32_get_max_priority();
bool esp32_requires_assembly_handler(uint8_t priority);

// Teensy platform
int teensy_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int teensy_detach_handler(isr_handle_t& handle);
int teensy_enable_handler(const isr_handle_t& handle);
int teensy_disable_handler(const isr_handle_t& handle);
bool teensy_is_handler_enabled(const isr_handle_t& handle);
const char* teensy_get_error_string(int error_code);
const char* teensy_get_platform_name();
uint32_t teensy_get_max_timer_frequency();
uint32_t teensy_get_min_timer_frequency();
uint8_t teensy_get_max_priority();
bool teensy_requires_assembly_handler(uint8_t priority);

// STM32 platform
int stm32_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int stm32_detach_handler(isr_handle_t& handle);
int stm32_enable_handler(const isr_handle_t& handle);
int stm32_disable_handler(const isr_handle_t& handle);
bool stm32_is_handler_enabled(const isr_handle_t& handle);
const char* stm32_get_error_string(int error_code);
const char* stm32_get_platform_name();
uint32_t stm32_get_max_timer_frequency();
uint32_t stm32_get_min_timer_frequency();
uint8_t stm32_get_max_priority();
bool stm32_requires_assembly_handler(uint8_t priority);

// NRF52 platform
int nrf52_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int nrf52_detach_handler(isr_handle_t& handle);
int nrf52_enable_handler(const isr_handle_t& handle);
int nrf52_disable_handler(const isr_handle_t& handle);
bool nrf52_is_handler_enabled(const isr_handle_t& handle);
const char* nrf52_get_error_string(int error_code);
const char* nrf52_get_platform_name();
uint32_t nrf52_get_max_timer_frequency();
uint32_t nrf52_get_min_timer_frequency();
uint8_t nrf52_get_max_priority();
bool nrf52_requires_assembly_handler(uint8_t priority);

// AVR platform
int avr_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int avr_detach_handler(isr_handle_t& handle);
int avr_enable_handler(const isr_handle_t& handle);
int avr_disable_handler(const isr_handle_t& handle);
bool avr_is_handler_enabled(const isr_handle_t& handle);
const char* avr_get_error_string(int error_code);
const char* avr_get_platform_name();
uint32_t avr_get_max_timer_frequency();
uint32_t avr_get_min_timer_frequency();
uint8_t avr_get_max_priority();
bool avr_requires_assembly_handler(uint8_t priority);

// RP2040 platform
int rp2040_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int rp2040_detach_handler(isr_handle_t& handle);
int rp2040_enable_handler(const isr_handle_t& handle);
int rp2040_disable_handler(const isr_handle_t& handle);
bool rp2040_is_handler_enabled(const isr_handle_t& handle);
const char* rp2040_get_error_string(int error_code);
const char* rp2040_get_platform_name();
uint32_t rp2040_get_max_timer_frequency();
uint32_t rp2040_get_min_timer_frequency();
uint8_t rp2040_get_max_priority();
bool rp2040_requires_assembly_handler(uint8_t priority);

// SAMD platform
int samd_attach_timer_handler(const isr_config_t& config, isr_handle_t* handle);
int samd_detach_handler(isr_handle_t& handle);
int samd_enable_handler(const isr_handle_t& handle);
int samd_disable_handler(const isr_handle_t& handle);
bool samd_is_handler_enabled(const isr_handle_t& handle);
const char* samd_get_error_string(int error_code);
const char* samd_get_platform_name();
uint32_t samd_get_max_timer_frequency();
uint32_t samd_get_min_timer_frequency();
uint8_t samd_get_max_priority();
bool samd_requires_assembly_handler(uint8_t priority);

// =============================================================================
// Cross-Platform ISR API Implementation
// =============================================================================

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

int attachExternalHandler(uint8_t pin, const isr_config_t& config, isr_handle_t* handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_attach_external_handler(pin, config, handle);
#elif defined(ESP32)
    return esp32_attach_external_handler(pin, config, handle);
#else
    return null_attach_external_handler(pin, config, handle);
#endif
}

int detachHandler(isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_detach_handler(handle);
#elif defined(ESP32)
    return esp32_detach_handler(handle);
#elif defined(FL_IS_TEENSY)
    return teensy_detach_handler(handle);
#elif defined(FL_IS_STM32)
    return stm32_detach_handler(handle);
#elif defined(FL_IS_NRF52)
    return nrf52_detach_handler(handle);
#elif defined(FL_IS_AVR)
    return avr_detach_handler(handle);
#elif defined(FL_IS_RP2040)
    return rp2040_detach_handler(handle);
#elif defined(FL_IS_SAMD)
    return samd_detach_handler(handle);
#else
    return null_detach_handler(handle);
#endif
}

int enableHandler(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_enable_handler(handle);
#elif defined(ESP32)
    return esp32_enable_handler(handle);
#elif defined(FL_IS_TEENSY)
    return teensy_enable_handler(handle);
#elif defined(FL_IS_STM32)
    return stm32_enable_handler(handle);
#elif defined(FL_IS_NRF52)
    return nrf52_enable_handler(handle);
#elif defined(FL_IS_AVR)
    return avr_enable_handler(handle);
#elif defined(FL_IS_RP2040)
    return rp2040_enable_handler(handle);
#elif defined(FL_IS_SAMD)
    return samd_enable_handler(handle);
#else
    return null_enable_handler(handle);
#endif
}

int disableHandler(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_disable_handler(handle);
#elif defined(ESP32)
    return esp32_disable_handler(handle);
#elif defined(FL_IS_TEENSY)
    return teensy_disable_handler(handle);
#elif defined(FL_IS_STM32)
    return stm32_disable_handler(handle);
#elif defined(FL_IS_NRF52)
    return nrf52_disable_handler(handle);
#elif defined(FL_IS_AVR)
    return avr_disable_handler(handle);
#elif defined(FL_IS_RP2040)
    return rp2040_disable_handler(handle);
#elif defined(FL_IS_SAMD)
    return samd_disable_handler(handle);
#else
    return null_disable_handler(handle);
#endif
}

bool isHandlerEnabled(const isr_handle_t& handle) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_is_handler_enabled(handle);
#elif defined(ESP32)
    return esp32_is_handler_enabled(handle);
#elif defined(FL_IS_TEENSY)
    return teensy_is_handler_enabled(handle);
#elif defined(FL_IS_STM32)
    return stm32_is_handler_enabled(handle);
#elif defined(FL_IS_NRF52)
    return nrf52_is_handler_enabled(handle);
#elif defined(FL_IS_AVR)
    return avr_is_handler_enabled(handle);
#elif defined(FL_IS_RP2040)
    return rp2040_is_handler_enabled(handle);
#elif defined(FL_IS_SAMD)
    return samd_is_handler_enabled(handle);
#else
    return null_is_handler_enabled(handle);
#endif
}

const char* getErrorString(int error_code) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_error_string(error_code);
#elif defined(ESP32)
    return esp32_get_error_string(error_code);
#elif defined(FL_IS_TEENSY)
    return teensy_get_error_string(error_code);
#elif defined(FL_IS_STM32)
    return stm32_get_error_string(error_code);
#elif defined(FL_IS_NRF52)
    return nrf52_get_error_string(error_code);
#elif defined(FL_IS_AVR)
    return avr_get_error_string(error_code);
#elif defined(FL_IS_RP2040)
    return rp2040_get_error_string(error_code);
#elif defined(FL_IS_SAMD)
    return samd_get_error_string(error_code);
#else
    return null_get_error_string(error_code);
#endif
}

const char* getPlatformName() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_platform_name();
#elif defined(ESP32)
    return esp32_get_platform_name();
#elif defined(FL_IS_TEENSY)
    return teensy_get_platform_name();
#elif defined(FL_IS_STM32)
    return stm32_get_platform_name();
#elif defined(FL_IS_NRF52)
    return nrf52_get_platform_name();
#elif defined(FL_IS_AVR)
    return avr_get_platform_name();
#elif defined(FL_IS_RP2040)
    return rp2040_get_platform_name();
#elif defined(FL_IS_SAMD)
    return samd_get_platform_name();
#else
    return null_get_platform_name();
#endif
}

uint32_t getMaxTimerFrequency() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_max_timer_frequency();
#elif defined(ESP32)
    return esp32_get_max_timer_frequency();
#elif defined(FL_IS_TEENSY)
    return teensy_get_max_timer_frequency();
#elif defined(FL_IS_STM32)
    return stm32_get_max_timer_frequency();
#elif defined(FL_IS_NRF52)
    return nrf52_get_max_timer_frequency();
#elif defined(FL_IS_AVR)
    return avr_get_max_timer_frequency();
#elif defined(FL_IS_RP2040)
    return rp2040_get_max_timer_frequency();
#elif defined(FL_IS_SAMD)
    return samd_get_max_timer_frequency();
#else
    return null_get_max_timer_frequency();
#endif
}

uint32_t getMinTimerFrequency() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_min_timer_frequency();
#elif defined(ESP32)
    return esp32_get_min_timer_frequency();
#elif defined(FL_IS_TEENSY)
    return teensy_get_min_timer_frequency();
#elif defined(FL_IS_STM32)
    return stm32_get_min_timer_frequency();
#elif defined(FL_IS_NRF52)
    return nrf52_get_min_timer_frequency();
#elif defined(FL_IS_AVR)
    return avr_get_min_timer_frequency();
#elif defined(FL_IS_RP2040)
    return rp2040_get_min_timer_frequency();
#elif defined(FL_IS_SAMD)
    return samd_get_min_timer_frequency();
#else
    return null_get_min_timer_frequency();
#endif
}

uint8_t getMaxPriority() {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_get_max_priority();
#elif defined(ESP32)
    return esp32_get_max_priority();
#elif defined(FL_IS_TEENSY)
    return teensy_get_max_priority();
#elif defined(FL_IS_STM32)
    return stm32_get_max_priority();
#elif defined(FL_IS_NRF52)
    return nrf52_get_max_priority();
#elif defined(FL_IS_AVR)
    return avr_get_max_priority();
#elif defined(FL_IS_RP2040)
    return rp2040_get_max_priority();
#elif defined(FL_IS_SAMD)
    return samd_get_max_priority();
#else
    return null_get_max_priority();
#endif
}

bool requiresAssemblyHandler(uint8_t priority) {
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    return stub_requires_assembly_handler(priority);
#elif defined(ESP32)
    return esp32_requires_assembly_handler(priority);
#elif defined(FL_IS_TEENSY)
    return teensy_requires_assembly_handler(priority);
#elif defined(FL_IS_STM32)
    return stm32_requires_assembly_handler(priority);
#elif defined(FL_IS_NRF52)
    return nrf52_requires_assembly_handler(priority);
#elif defined(FL_IS_AVR)
    return avr_requires_assembly_handler(priority);
#elif defined(FL_IS_RP2040)
    return rp2040_requires_assembly_handler(priority);
#elif defined(FL_IS_SAMD)
    return samd_requires_assembly_handler(priority);
#else
    return null_requires_assembly_handler(priority);
#endif
}

} // namespace isr
} // namespace fl
