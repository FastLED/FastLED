// ok no namespace fl
#pragma once

// IWYU pragma: private

/// Async-signal-safe watchdog and phase diagnostics for Apple dylib runners.
///
/// A SIGALRM can interrupt dyld while it holds internal loader locks. The
/// implementation therefore keeps its handler to write(2) + _exit(2), both
/// async-signal-safe, and emits phase markers before loader transitions.

#include "fl/stl/noexcept.h"

namespace apple_runner_watchdog {

enum class Phase {
    kStartup = 0,
    kCrashHandlerSetup,
    kPathResolution,
    kDynamicLoad,
    kSymbolLookup,
    kInvocation,
    kTeardown,
    kCompleted,
};

void set_phase(Phase phase) FL_NO_EXCEPT;
void setup(double timeout_seconds = 20.0) FL_NO_EXCEPT;
void cancel() FL_NO_EXCEPT;

} // namespace apple_runner_watchdog
