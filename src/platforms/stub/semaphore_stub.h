// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/stub/semaphore_stub.h
/// @brief Stub platform semaphore implementation trampoline
///
/// This header dispatches to the appropriate semaphore implementation based on
/// multithreading support. Routes to either STL-based semaphore (when multithreaded)
/// or a fake no-op implementation (for single-threaded platforms).

#include "fl/stl/thread.h"  // Defines FASTLED_MULTITHREADED via platform detection

// Dispatch to appropriate implementation based on FASTLED_MULTITHREADED
#if FASTLED_MULTITHREADED
    #include "platforms/stub/semaphore_stub_stl.h"
#else
    #include "platforms/stub/semaphore_stub_noop.h"
#endif
