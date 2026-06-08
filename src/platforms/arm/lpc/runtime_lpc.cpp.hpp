#pragma once

// IWYU pragma: private
#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/stddef.h"
#include "fl/stl/noexcept.h"

// Minimal C++ runtime support for bare-metal LPC8xx
// These are required by the C++ compiler but not needed for most embedded use

extern "C" {

// C++ global destructor handle - required for static objects with destructors
// Weak symbol so linker script can override if needed
void* __dso_handle __attribute__((weak)) = nullptr;

// Heap boundary symbols - required by newlib's sbrk()
// Define both 'end' and '_end' as the linker may look for either
// Place at end of .bss section - actual address will be set by linker
char end __attribute__((weak));
char _end __attribute__((weak));

}  // extern "C"

// Minimal operator delete implementations
// These are no-ops since we don't use dynamic memory in embedded contexts
// The linker needs them defined even if they're never called

void operator delete(void* ptr) FL_NOEXCEPT {
    (void)ptr;
    // No-op: we don't free memory on bare-metal
}

void operator delete(void* ptr, unsigned int size) FL_NOEXCEPT {
    (void)ptr;
    (void)size;
    // No-op: we don't free memory on bare-metal
}

void operator delete[](void* ptr) FL_NOEXCEPT {
    (void)ptr;
    // No-op: we don't free memory on bare-metal
}

void operator delete[](void* ptr, unsigned int size) FL_NOEXCEPT {
    (void)ptr;
    (void)size;
    // No-op: we don't free memory on bare-metal
}

#endif  // FL_IS_ARM_LPC
