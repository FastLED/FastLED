#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"

// __dso_handle must have C++ linkage: the compiler implicitly declares it that
// way when it emits __cxa_atexit() for C++ static locals with destructors
// (e.g. bus_traits.h's gHolder). Declaring it extern "C" here conflicts with
// that implicit declaration. The emitted symbol is `__dso_handle` either way
// (global-scope objects aren't name-mangled), so this is linker-safe.
FL_LINK_WEAK void* __dso_handle;

extern "C" {
FL_LINK_WEAK char end;
FL_LINK_WEAK char _end;
}

FL_LINK_WEAK void operator delete(void* ptr) FL_NO_EXCEPT {
    (void)ptr;
}

FL_LINK_WEAK void operator delete[](void* ptr) FL_NO_EXCEPT {
    (void)ptr;
}

FL_LINK_WEAK void operator delete(void* ptr, size_t size) FL_NO_EXCEPT {
    (void)ptr;
    (void)size;
}

FL_LINK_WEAK void operator delete[](void* ptr, size_t size) FL_NO_EXCEPT {
    (void)ptr;
    (void)size;
}

#endif  // FL_IS_ARM_LPC
