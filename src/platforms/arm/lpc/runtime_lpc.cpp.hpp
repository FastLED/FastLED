#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/cstddef.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/is_arm.h"

extern "C" {
FL_LINK_WEAK void* __dso_handle;
FL_LINK_WEAK char end;
FL_LINK_WEAK char _end;
}

FL_LINK_WEAK void operator delete(void* ptr) FL_NOEXCEPT {
    (void)ptr;
}

FL_LINK_WEAK void operator delete[](void* ptr) FL_NOEXCEPT {
    (void)ptr;
}

FL_LINK_WEAK void operator delete(void* ptr, size_t size) FL_NOEXCEPT {
    (void)ptr;
    (void)size;
}

FL_LINK_WEAK void operator delete[](void* ptr, size_t size) FL_NOEXCEPT {
    (void)ptr;
    (void)size;
}

#endif  // FL_IS_ARM_LPC
