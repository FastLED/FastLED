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
// Full no-op in release — matches standard C `<assert.h>` behavior under
// NDEBUG. MSG is typically an `fl::sstream` operator<< chain like
// `"text " << i`, which is only valid in the presence of a streamable
// LHS. A previous attempt to syntactically validate `(MSG)` via
// `sizeof((void)(MSG), 0)` broke the build because the chain has no
// streamable LHS outside the sstream call expression. Just no-op.
#define FASTLED_ASSERT(x, MSG) ((void)0)
#endif
