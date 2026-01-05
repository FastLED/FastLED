// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/thread_stub.h
/// @brief Stub platform thread implementation trampoline
///
/// This header dispatches to the appropriate thread implementation based on
/// pthread availability. Routes to either STL-based threading (when pthread
/// is available) or a fake no-op implementation (for single-threaded platforms).

#include "fl/has_include.h"

// Detect pthread availability for multithreading support

#ifndef FL_STUB_HAS_MULTITHREADED

#if FL_HAS_INCLUDE(<pthread.h>)
    #define FL_STUB_HAS_MULTITHREADED 1
#else
    #define FL_STUB_HAS_MULTITHREADED 0
#endif

#if FL_STUB_HAS_MULTITHREADED
    #include "platforms/stub/thread_stub_stl.h"
#else
    #include "platforms/stub/thread_stub_noop.h"
#endif

#ifndef FASTLED_MULTITHREADED
#error "Expected FASTLED_MULTITHREADED to be defined"
#endif

#endif  // FL_STUB_HAS_MULTITHREADED
