#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Worker task affinity. This does not change peripheral interrupt affinity.
enum class Core : fl::i8 {
    Auto = -1,    ///< Inherit the engine default, otherwise its legacy policy.
    Any = -2,        ///< Allow the worker to run on any configured RTOS core.
    CallingCore = -3, ///< Capture the core that creates the worker.
    Core0 = 0,
    Core1 = 1
};

/// Portable worker scheduling policy, resolved by the selected backend.
/// Auto inherits an existing shared worker setting or an honored compile-time
/// engine default. Otherwise a new LCD/I2S worker defaults to High.
/// These values are policies, not RTOS priority numbers.
enum class Priority : fl::u8 {
    Auto,
    High,   ///< At networking priority or above, biased above when supported.
    Medium, ///< Below networking priority, resolved by the backend scheduler.
    Low     ///< Generally not recommended for timing-sensitive engine workers.
};

/// Shared engine settings, configured before the first transmission.
/// Default fields inherit the engine's configuration. Explicit conflicting
/// requests are rejected. Configuration calls must be serialized with show().
struct WorkerConfig {
    Core core = Core::Auto;
    Priority priority = Priority::Auto;

    constexpr bool isDefault() const FL_NO_EXCEPT {
        return priority == Priority::Auto && core == Core::Auto;
    }
};

/// Worker scheduling evidence. Unavailable workers retain the sentinels;
/// core -1 means no affinity, core -2 means no worker has been created.
struct WorkerDiagnostics {
    int core = -2;
    int priority = -1;
    int observedCore = -1;
    fl::u32 submittedChunks = 0;
    fl::u32 completedChunks = 0;
    fl::u32 totalChunks = 0;
    fl::u32 maxEncodeUs = 0;
    fl::u32 maxSubmitUs = 0;
    bool streamError = false;
};

} // namespace fl
