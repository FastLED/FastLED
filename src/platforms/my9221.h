#pragma once

// MY9221 platform boundary: allocation happens before the active-edge guard.
#include "platforms/is_platform.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/memory_resource.h"
#include "fl/stl/allocator.h"
#include "fl/stl/malloc.h"

// IWYU pragma: begin_keep
#if defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
// Host emulation must not acquire a vendor SDK dependency.
#elif defined(FL_IS_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#elif defined(FL_IS_ESP8266)
#include <interrupts.h>
#include <umm_malloc/umm_heap_select.h>
#elif defined(FL_IS_RP)
#include <hardware/sync.h>
#elif defined(__AVR__)
#include <avr/interrupt.h>
#elif defined(FL_IS_CI13XX)
#include <ci130x_core_eclic.h>
#endif
// IWYU pragma: end_keep

namespace fl {
namespace detail {

// Only masks the calling core's ordinary maskable interrupts. NMI/fault,
// higher-priority ESP interrupts and concurrent GPIO owners remain outside
// this guard's guarantee. No unconditional enabling on scope exit.
template <int Identity> class My9221InterruptGuard {
#if defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
#elif defined(FL_IS_ESP32)
    UBaseType_t mSaved;
#elif defined(FL_IS_ESP8266) || defined(FL_IS_RP) || defined(__arm__) || defined(FL_IS_CI13XX)
    fl::u32 mSaved;
#elif defined(__AVR__)
    fl::u8 mSaved;
#endif
public:
    FASTLED_FORCE_INLINE My9221InterruptGuard() FL_NO_EXCEPT {
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("" ::: "memory");
#endif
#if defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
        // Explicit nonphysical host backend; no target interrupt proof.
#elif defined(FL_IS_ESP32)
        mSaved = portSET_INTERRUPT_MASK_FROM_ISR();
#elif defined(FL_IS_ESP8266)
        mSaved = xt_rsil(15);
#elif defined(FL_IS_RP)
        mSaved = save_and_disable_interrupts();
#elif defined(__AVR__)
        mSaved = SREG;
        cli();
#elif defined(__arm__) && ((defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M') || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__))
        __asm__ __volatile__("MRS %0, PRIMASK\n\tCPSID i" : "=r"(mSaved) :: "memory");
#elif defined(FL_IS_CI13XX)
        // SDK global_irq_disable uses machine mstatus.MIE (bit3).
        mSaved = clear_csr(0x300, 8);
#else
        FL_STATIC_ASSERT(Identity != Identity,
                         "MY9221 needs a saved-state interrupt backend");
#endif
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("" ::: "memory");
#endif
    }
    FASTLED_FORCE_INLINE ~My9221InterruptGuard() FL_NO_EXCEPT {
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("" ::: "memory");
#endif
#if defined(FASTLED_STUB_IMPL) || defined(FL_IS_WASM)
#elif defined(FL_IS_ESP32)
        portCLEAR_INTERRUPT_MASK_FROM_ISR(mSaved);
#elif defined(FL_IS_ESP8266)
        xt_wsr_ps(mSaved);
#elif defined(FL_IS_RP)
        restore_interrupts(mSaved);
#elif defined(__AVR__)
        SREG = mSaved;
        __asm__ __volatile__("" ::: "memory");
#elif defined(__arm__) && ((defined(__ARM_ARCH_PROFILE) && __ARM_ARCH_PROFILE == 'M') || defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__))
        __asm__ __volatile__("MSR PRIMASK, %0" :: "r"(mSaved) : "memory");
#elif defined(FL_IS_CI13XX)
        if (mSaved & 8) { (void)set_csr(0x300, 8); }
        else { (void)clear_csr(0x300, 8); }
#endif
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(__GNUC__) || defined(__clang__))
        __asm__ __volatile__("" ::: "memory");
#endif
    }
    My9221InterruptGuard(const My9221InterruptGuard&) = delete;
    My9221InterruptGuard& operator=(const My9221InterruptGuard&) = delete;
};

#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(FL_IS_ESP32) || defined(FL_IS_ESP8266))
// A concrete private resource avoids the mutable PSRAM-capable Alloc hook.
class My9221InternalResource : public fl::memory_resource {
    void* do_allocate(fl::size bytes) FL_NO_EXCEPT override {
#if defined(FL_IS_ESP32)
        return fl::InternalAlloc(bytes);
#else
        HeapSelectDram select;
        return fl::malloc(bytes);
#endif
    }
    void do_deallocate(void* ptr, fl::size) FL_NO_EXCEPT override {
#if defined(FL_IS_ESP32)
        fl::InternalFree(ptr);
#else
        fl::free(ptr);
#endif
    }
};
#endif

inline fl::memory_resource* my9221FrameResource() FL_NO_EXCEPT {
#if !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM) && (defined(FL_IS_ESP32) || defined(FL_IS_ESP8266))
    static My9221InternalResource resource;
    return &resource;
#else
    return fl::default_memory_resource();
#endif
}

} // namespace detail
} // namespace fl
