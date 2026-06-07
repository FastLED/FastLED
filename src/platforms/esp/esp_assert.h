#pragma once

// IWYU pragma: private

#include "fl/log/log.h"  // for FASTLED_LOG_RUNTIME_ENABLED
#include "fl/stl/strstream.h"
#include "fl/stl/noexcept.h"

// Forward declaration to avoid pulling in fl/stl/cstdio.h and causing fl/stl/cstdio.cpp to be compiled
namespace fl {
    void println(const char* str) FL_NOEXCEPT;
}

// Gate the entire macro body on FASTLED_LOG_RUNTIME_ENABLED so release
// builds (NDEBUG → FASTLED_LOG_VERBOSITY=0 per #2890) don't emit the
// per-call-site `fl::sstream` chain + `fl::println` call. This matches
// the standard C `<assert.h>` behavior under NDEBUG: the macro becomes
// `((void)0)` and the condition expression is not evaluated. Sketches
// must therefore not put side effects in assertion conditions
// (already best practice). See #2923 / #2886.
#if FASTLED_LOG_RUNTIME_ENABLED
#define FASTLED_ASSERT(x, MSG)                                                 \
    do {                                                                       \
        if (!(x)) {                                                            \
            fl::println((fl::sstream() << "FASTLED ASSERT FAILED: " << MSG).c_str());  \
        }                                                                      \
    } while (0)
#else
// `sizeof((x), (MSG), 0)` keeps the expression syntactically validated
// (catches typos at compile time) without evaluating it at runtime.
#define FASTLED_ASSERT(x, MSG)                                                 \
    do { (void)sizeof((void)(x), (void)(MSG), 0); } while (0)
#endif
