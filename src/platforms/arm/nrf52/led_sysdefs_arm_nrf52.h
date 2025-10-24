// ok no namespace fl
#ifndef __LED_SYSDEFS_ARM_NRF52
#define __LED_SYSDEFS_ARM_NRF52

#include "fl/force_inline.h"

#ifndef FASTLED_ARM
#error "FASTLED_ARM must be defined before including this header. Ensure platforms/arm/is_arm.h is included first."
#endif

#ifndef F_CPU
    #define F_CPU 64000000 // the NRF52 series has a 64MHz CPU
#endif

// even though CPU is at 64MHz, use the 8MHz-defined timings because...
// PWM module   runs at 16MHz
// SPI0..2      runs at  8MHz
#define CLOCKLESS_FREQUENCY 16000000 // the NRF52 has EasyDMA for PWM module at 16MHz

#ifndef F_TIMER
    #define F_TIMER 16000000 // the NRF52 timer is 16MHz, even though CPU is 64MHz
#endif

#if !defined(FASTLED_USE_PROGMEM)
    #define FASTLED_USE_PROGMEM 0 // nRF52 series have flat memory model
#endif

#if !defined(FASTLED_ALLOW_INTERRUPTS)
    #define FASTLED_ALLOW_INTERRUPTS 1
#endif

// Use PWM instance 0
// See clockless_arm_nrf52.h and (in root of library) platforms.cpp
#define FASTLED_NRF52_ENABLE_PWM_INSTANCE0

#if defined(FASTLED_NRF52_NEVER_INLINE)
    #define FASTLED_NRF52_INLINE_ATTRIBUTE FASTLED_FORCE_INLINE
#else     
    #define FASTLED_NRF52_INLINE_ATTRIBUTE FASTLED_FORCE_INLINE
#endif    



#include <nrf.h>
#include <nrf_spim.h>   // for FastSPI
#include <nrf_pwm.h>    // for Clockless
#include <nrf_nvic.h>   // for Clockless / anything else using interrupts
typedef __I  uint32_t RoReg;
typedef __IO uint32_t RwReg;

#define cli()  __disable_irq()
#define sei()  __enable_irq()

#define FASTLED_NRF52_DEBUGPRINT(format, ...)\
//    do { FastLED_NRF52_DebugPrint(format, ##__VA_ARGS__); } while(0);

// Force malloc wrappers from Adafruit BSP heap_3.c to be linked
// The Adafruit nRF52 BSP uses FreeRTOS malloc wrappers (__wrap_malloc, etc.)
// that are defined in heap_3.c. However, because they're in a static library,
// the linker only pulls them in when something references them. The problem is
// that libc_nano.a on Cortex-M4 with float printf support directly calls
// __wrap_malloc (not malloc), creating a circular dependency where nothing
// explicitly references __wrap_malloc before the library is scanned.
//
// This function forces those symbols to be marked as needed by creating
// explicit references to them. The function itself will be optimized away
// or placed in a discardable section, but the references ensure the linker
// pulls in heap_3.c.o from libFrameworkArduino.a
//
// Can be disabled by defining FASTLED_FORCE_NRF52_WRAP_MALLOC=0
#if !defined(FASTLED_FORCE_NRF52_WRAP_MALLOC)
    #define FASTLED_FORCE_NRF52_WRAP_MALLOC 1
#endif

#if FASTLED_FORCE_NRF52_WRAP_MALLOC && (defined(ARDUINO_NRF52_ADAFRUIT) || defined(NRF52_SERIES))
#ifdef __cplusplus
extern "C" {
#endif

// Declare the malloc wrapper functions from heap_3.c
extern void* __wrap_malloc(size_t size);
extern void __wrap_free(void* ptr);
extern void* __wrap_realloc(void* ptr, size_t size);
extern void* __wrap_calloc(size_t nmemb, size_t size);

// Dummy function that references the malloc wrappers
// __attribute__((used)) prevents the compiler from optimizing it away
// __attribute__((noinline)) prevents inlining
// static prevents external linkage issues
// The function is never actually called, but its existence forces the linker
// to resolve the __wrap_* symbols, pulling in heap_3.c.o
__attribute__((used, noinline))
static volatile void* fastled_nrf52_force_malloc_wrappers_link(void) {
    // Cast function pointers to volatile void* to prevent optimization
    // while avoiding type mismatch errors with volatile function pointers
    volatile void* ptrs[] = {
        (volatile void*)(void*)__wrap_malloc,
        (volatile void*)(void*)__wrap_free,
        (volatile void*)(void*)__wrap_realloc,
        (volatile void*)(void*)__wrap_calloc
    };

    // Return first pointer to ensure they're all "used"
    return ptrs[0];
}

#ifdef __cplusplus
}
#endif
#endif // FASTLED_FORCE_NRF52_WRAP_MALLOC && (ARDUINO_NRF52_ADAFRUIT || NRF52_SERIES)

#endif // __LED_SYSDEFS_ARM_NRF52
