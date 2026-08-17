// IWYU pragma: private

// ok no namespace fl
#ifndef __LED_SYSDEFS_ARM_NRF52
#define __LED_SYSDEFS_ARM_NRF52

#include "fl/stl/compiler_control.h"

#include "platforms/arm/is_arm.h"
#include "platforms/arm/nrf52/is_nrf52.h"

#ifndef FL_IS_ARM
#error "FL_IS_ARM must be defined before including this header. Ensure platforms/arm/is_arm.h is included first."
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

// Use PWM instance 0 by default. Individual instance flags remain overridable
// for applications that reserve or coordinate more than one PWM peripheral.
// See clockless_arm_nrf52.h and (in root of library) platforms.cpp
#if !defined(FL_NRF52_PWM_ID)
    #define FL_NRF52_PWM_ID 0
#endif
#if !defined(FL_NRF52_ENABLE_PWM_INSTANCE0)
    #define FL_NRF52_ENABLE_PWM_INSTANCE0 (FL_NRF52_PWM_ID == 0)
#endif
#if !defined(FL_NRF52_ENABLE_PWM_INSTANCE1)
    #define FL_NRF52_ENABLE_PWM_INSTANCE1 (FL_NRF52_PWM_ID == 1)
#endif
#if !defined(FL_NRF52_ENABLE_PWM_INSTANCE2)
    #define FL_NRF52_ENABLE_PWM_INSTANCE2 (FL_NRF52_PWM_ID == 2)
#endif
#if !defined(FL_NRF52_ENABLE_PWM_INSTANCE3)
    #define FL_NRF52_ENABLE_PWM_INSTANCE3 (FL_NRF52_PWM_ID == 3)
#endif

#if defined(FASTLED_NRF52_NEVER_INLINE)
    #define FASTLED_NRF52_INLINE_ATTRIBUTE FASTLED_FORCE_INLINE
#else     
    #define FASTLED_NRF52_INLINE_ATTRIBUTE FASTLED_FORCE_INLINE
#endif    



// IWYU pragma: begin_keep
#include <nrf.h>
#include <nrf_spim.h>   // for FastSPI
#include <nrf_pwm.h>    // for Clockless
#include <nrf_nvic.h>   // for Clockless / anything else using interrupts
// IWYU pragma: end_keep

// Adafruit's FreeRTOS core requires PWM interrupts to use its maximum
// syscall-safe priority. Mbed does not define that FreeRTOS setting, so use
// the nRF52840's lowest application interrupt priority instead. Allow board
// cores and applications to override the selection when needed.
#if !defined(FL_NRF52_PWM_INTERRUPT_PRIORITY)
    #if defined(configMAX_SYSCALL_INTERRUPT_PRIORITY)
        #define FL_NRF52_PWM_INTERRUPT_PRIORITY configMAX_SYSCALL_INTERRUPT_PRIORITY
    #elif defined(NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY)
        #define FL_NRF52_PWM_INTERRUPT_PRIORITY NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY
    #else
        #define FL_NRF52_PWM_INTERRUPT_PRIORITY 7
    #endif
#endif

// Mbed owns the nrfx PWM interrupt handlers. Use synchronous EasyDMA playback
// there so FastLED does not replace a framework ISR or disturb its callback
// dispatch. The Adafruit core keeps the existing asynchronous interrupt path.
#if !defined(FL_NRF52_USE_PWM_INTERRUPTS)
    #if defined(__MBED__)
        #define FL_NRF52_USE_PWM_INTERRUPTS 0
    #else
        #define FL_NRF52_USE_PWM_INTERRUPTS 1
    #endif
#endif

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"
typedef __I  fl::u32 RoReg;
typedef __IO fl::u32 RwReg;

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
// Can be disabled by defining FL_NRF52_FORCE_WRAP_MALLOC=0
#if !defined(FL_NRF52_FORCE_WRAP_MALLOC)
    #if defined(__MBED__)
        #define FL_NRF52_FORCE_WRAP_MALLOC 0
    #else
        #define FL_NRF52_FORCE_WRAP_MALLOC 1
    #endif
#endif

#if FL_NRF52_FORCE_WRAP_MALLOC && defined(FL_IS_NRF52)
#ifdef __cplusplus
extern "C" {
#endif

// Declare the malloc wrapper functions from heap_3.c
extern void* __wrap_malloc(size_t size) FL_NO_EXCEPT;
extern void __wrap_free(void* ptr) FL_NO_EXCEPT;
extern void* __wrap_realloc(void* ptr, size_t size) FL_NO_EXCEPT;
extern void* __wrap_calloc(size_t nmemb, size_t size) FL_NO_EXCEPT;

// Dummy function that references the malloc wrappers
// __attribute__((used)) prevents the compiler from optimizing it away
// __attribute__((noinline)) prevents inlining
// static prevents external linkage issues
// The function is never actually called, but its existence forces the linker
// to resolve the __wrap_* symbols, pulling in heap_3.c.o
__attribute__((used, noinline))
static volatile void* fastled_nrf52_force_malloc_wrappers_link(void) FL_NO_EXCEPT {
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
#endif // FL_NRF52_FORCE_WRAP_MALLOC && FL_IS_NRF52

#endif // __LED_SYSDEFS_ARM_NRF52
