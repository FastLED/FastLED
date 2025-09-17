/// @file platforms.cpp
/// Platform-specific functions and variables

/// Disables pragma messages and warnings
#define FASTLED_INTERNAL
#include "fl/compiler_control.h"

// Removed duplicate weak definition of timer_millis for ATtiny1604.
// The variable is already defined in avr_millis_timer_null_counter.hpp when needed,
// so redefining it here caused multiple-definition linkage errors.

// Provide a single consolidated weak timer_millis symbol for AVR tiny/x-y parts
// whose cores do not export it, satisfying MS_COUNTER binding in led_sysdefs_avr.h.
// This complements avr_millis_timer_null_counter.hpp when that TU is not built.
#if defined(__AVR__)
#  if defined(__AVR_ATtiny1604__) || defined(ARDUINO_attinyxy6) || defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtiny1616__)
#    ifdef __cplusplus
extern "C" {
#    endif
FL_LINK_WEAK volatile unsigned long timer_millis = 0;
#    ifdef __cplusplus
}
#    endif
#  endif
#endif

// Interrupt handlers cannot be defined in the header.
// They must be defined as C functions, or they won't
// be found (due to name mangling), and thus won't
// override any default weak definition.
#if defined(NRF52_SERIES)

    #include "platforms/arm/is_arm.h"
    #include "platforms/arm/nrf52/led_sysdefs_arm_nrf52.h"
    #include "platforms/arm/nrf52/arbiter_nrf52.h"

    uint32_t isrCount;

    #ifdef __cplusplus
        extern "C" {
    #endif
            // NOTE: Update platforms.cpp in root of FastLED library if this changes        
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE0)
                void PWM0_IRQHandler(void) { ++isrCount; PWM_Arbiter<0>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE1)
                void PWM1_IRQHandler(void) { ++isrCount; PWM_Arbiter<1>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE2)
                void PWM2_IRQHandler(void) { ++isrCount; PWM_Arbiter<2>::isr_handler(); }
            #endif
            #if defined(FASTLED_NRF52_ENABLE_PWM_INSTANCE3)
                void PWM3_IRQHandler(void) { ++isrCount; PWM_Arbiter<3>::isr_handler(); }
            #endif
    #ifdef __cplusplus
        }
    #endif

#endif // defined(NRF52_SERIES)



// FASTLED_NAMESPACE_BEGIN
// FASTLED_NAMESPACE_END
