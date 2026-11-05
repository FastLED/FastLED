#pragma once

#include "fl/channels/worker_config.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"

// Optional defaults for engines that opt in. Supply identical build flags in
// every translation unit; individual peripherals may honor or ignore them.
// Core uses fl::Core policies; Auto (-1) retains the backend's legacy policy.
#ifndef FL_ENGINE_WORKER_CORE
#define FL_ENGINE_WORKER_CORE -1
#endif

// Portable fl::Priority policy, never a raw scheduler priority. High means
// at networking priority or above, biased above; Medium is below networking.
// Low is generally not recommended for timing-sensitive engine workers.
// Auto (0) retains the backend's existing default: High on the LCD/I2S worker.
#ifndef FL_ENGINE_WORKER_PRIORITY
#define FL_ENGINE_WORKER_PRIORITY 0
#endif

namespace fl {
namespace detail {

// Accept integer policy values or the matching enum, never fractional values
// or an unrelated enum whose numeric value happens to fit the range.
using EngineWorkerCoreType = fl::decay_t<decltype(FL_ENGINE_WORKER_CORE)>;
using EngineWorkerPriorityType = fl::decay_t<decltype(FL_ENGINE_WORKER_PRIORITY)>;
FL_STATIC_ASSERT((fl::is_integral<EngineWorkerCoreType>::value ||
                      fl::is_same<EngineWorkerCoreType, Core>::value),
                 "FL_ENGINE_WORKER_CORE must be a valid fl::Core policy");
FL_STATIC_ASSERT((fl::is_integral<EngineWorkerPriorityType>::value ||
                      fl::is_same<EngineWorkerPriorityType, Priority>::value),
                 "FL_ENGINE_WORKER_PRIORITY must be a valid fl::Priority policy");

// Validate before narrowing into the byte-sized enums (256 must not wrap Auto).
// Unsigned values are range-checked first: 2^64-3 would otherwise cast to -3.
FL_STATIC_ASSERT((fl::is_signed<EngineWorkerCoreType>::value ||
                      fl::is_same<EngineWorkerCoreType, Core>::value ||
                      static_cast<fl::u64>(FL_ENGINE_WORKER_CORE) <= 1) &&
                     static_cast<fl::i64>(FL_ENGINE_WORKER_CORE) >= -3 &&
                     static_cast<fl::i64>(FL_ENGINE_WORKER_CORE) <= 1,
                 "FL_ENGINE_WORKER_CORE must be a valid fl::Core policy");
FL_STATIC_ASSERT(static_cast<fl::i64>(FL_ENGINE_WORKER_PRIORITY) >= 0 &&
                     static_cast<fl::i64>(FL_ENGINE_WORKER_PRIORITY) <= 3,
                 "FL_ENGINE_WORKER_PRIORITY must be a valid fl::Priority policy");

constexpr Core kEngineWorkerDefaultCore = static_cast<Core>(FL_ENGINE_WORKER_CORE);
constexpr Priority kEngineWorkerDefaultPriority = static_cast<Priority>(FL_ENGINE_WORKER_PRIORITY);

} // namespace detail
} // namespace fl
