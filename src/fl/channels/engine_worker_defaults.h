#pragma once

#include "fl/channels/worker_config.h"
#include "fl/stl/static_assert.h"

// Optional defaults for engines that opt in. Supply identical build flags in
// every translation unit; individual peripherals may honor or ignore them.
// Core uses fl::Core policies; Auto (-1) retains the backend's legacy policy.
#ifndef FL_ENGINE_WORKER_CORE
#define FL_ENGINE_WORKER_CORE -1
#endif

// Portable fl::Priority policy, never a raw scheduler priority. High means
// at networking priority or above, biased above; Medium is below networking.
// Low is generally not recommended for timing-sensitive engine workers.
// Auto (0) retains the backend's existing default.
#ifndef FL_ENGINE_WORKER_PRIORITY
#define FL_ENGINE_WORKER_PRIORITY 0
#endif

namespace fl {
namespace detail {

// Validate before narrowing into the byte-sized enums (256 must not wrap Auto).
FL_STATIC_ASSERT(static_cast<fl::i64>(FL_ENGINE_WORKER_CORE) >= -3 &&
                     static_cast<fl::i64>(FL_ENGINE_WORKER_CORE) <= 1,
                 "FL_ENGINE_WORKER_CORE must be a valid fl::Core policy");
FL_STATIC_ASSERT(static_cast<fl::i64>(FL_ENGINE_WORKER_PRIORITY) >= 0 &&
                     static_cast<fl::i64>(FL_ENGINE_WORKER_PRIORITY) <= 3,
                 "FL_ENGINE_WORKER_PRIORITY must be a valid fl::Priority policy");

constexpr Core engineWorkerDefaultCore() FL_NO_EXCEPT {
    return static_cast<Core>(FL_ENGINE_WORKER_CORE);
}

constexpr Priority engineWorkerDefaultPriority() FL_NO_EXCEPT {
    return static_cast<Priority>(FL_ENGINE_WORKER_PRIORITY);
}

} // namespace detail
} // namespace fl
