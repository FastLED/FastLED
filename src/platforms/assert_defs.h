// ok no namespace fl
#pragma once

/// @file platforms/assert_defs.h
/// @brief Minimal assertion macros that avoid circular dependencies
///
/// IMPORTANT: This header MUST NOT include fl/log.h or any header that
/// leads to fl/stl/string.h, because:
/// - shared_ptr.h -> atomic.h -> thread.h -> platforms/thread.h
/// - platforms/thread.h (for STM32) -> mutex_stm32_rtos.h -> fl/stl/assert.h
/// - fl/stl/assert.h -> platforms/assert_defs.h -> (here)
///
/// If we include fl/log.h here, it causes:
/// fl/log.h -> fl/stl/strstream.h -> fl/str.h -> fl/stl/string.h -> shared_ptr.h
/// Which creates a circular dependency where shared_ptr isn't defined yet.
///
/// The solution is to provide a minimal assertion mechanism that doesn't
/// require string formatting. For richer logging, code should include
/// fl/log.h separately (after all the basic types are defined).

// Platform-specific assertion handlers (these are self-contained)
#ifdef __EMSCRIPTEN__
#include "platforms/wasm/js_assert.h"
#elif defined(ESP32)
#include "platforms/esp/esp_assert.h"
#endif

// Default assertion implementation for platforms without a specific handler
#if !defined(__EMSCRIPTEN__) && !defined(ESP32)

#ifndef FASTLED_USES_SYSTEM_ASSERT
#if defined(FASTLED_TESTING)
#define FASTLED_USES_SYSTEM_ASSERT 1
#else
#define FASTLED_USES_SYSTEM_ASSERT 0
#endif
#endif

namespace fl {
namespace detail {

/// @brief Minimal no-op stream sink for assertion messages
/// This class accepts stream-style << expressions and discards them.
/// It exists to provide syntactically valid code for no-op assertions.
/// IMPORTANT: This class must not depend on any FL types (string, etc.)
/// to avoid circular dependencies.
struct AssertSink {
    // Accept any type and return *this to allow chaining
    template<typename T>
    AssertSink& operator<<(const T&) { return *this; }

    // Explicit overloads for common types to avoid template instantiation overhead
    AssertSink& operator<<(const char*) { return *this; }
    AssertSink& operator<<(int) { return *this; }
    AssertSink& operator<<(unsigned int) { return *this; }
    AssertSink& operator<<(long) { return *this; }
    AssertSink& operator<<(unsigned long) { return *this; }
};

} // namespace detail
} // namespace fl

#if FASTLED_USES_SYSTEM_ASSERT
// Testing builds: use a simple while(1) loop to halt on assertion failure
// This avoids pulling in the full logging infrastructure
#define FASTLED_ASSERT(x, MSG)                                                 \
    do {                                                                       \
        if (!(x)) {                                                            \
            while (1) { }                                                      \
        }                                                                      \
    } while (0)
#define FASTLED_ASSERT_IF(COND, x, MSG)                                        \
    do {                                                                       \
        if (COND && !(x)) {                                                    \
            while (1) { }                                                      \
        }                                                                      \
    } while (0)
#else
// Release builds: assertions are no-ops for minimal overhead
// We use if(false) to ensure the MSG expression is syntactically valid
// but never evaluated (compiler optimizes this away completely)
// The AssertSink class accepts stream-style << expressions.
#define FASTLED_ASSERT(x, MSG)                                                 \
    do {                                                                       \
        if (false) {                                                           \
            (void)(x);                                                         \
            (void)(fl::detail::AssertSink() << MSG);                           \
        }                                                                      \
    } while (0)
#define FASTLED_ASSERT_IF(COND, x, MSG)                                        \
    do {                                                                       \
        if (false) {                                                           \
            (void)(COND);                                                      \
            (void)(x);                                                         \
            (void)(fl::detail::AssertSink() << MSG);                           \
        }                                                                      \
    } while (0)
#endif

#endif // !__EMSCRIPTEN__ && !ESP32
