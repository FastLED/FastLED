// IWYU pragma: private

#include "fl/channels/detail/driver_completion_wait.h"
#include "fl/stl/chrono.h"
#include "fl/stl/singleton.h"

namespace fl {
namespace detail {

namespace {
struct DriverCompletionWaitHooks {
    DriverCompletionWait managed = nullptr;
    DriverCompletionWait legacy = nullptr;
};
} // namespace

DriverCompletionWait setManagedCompletionWait(DriverCompletionWait wait) FL_NO_EXCEPT {
    auto &hooks = Singleton<DriverCompletionWaitHooks>::instance();
    const auto previous = hooks.managed;
    hooks.managed = wait;
    return previous;
}

DriverCompletionWait setLegacyCompletionWait(DriverCompletionWait wait) FL_NO_EXCEPT {
    auto &hooks = Singleton<DriverCompletionWaitHooks>::instance();
    const auto previous = hooks.legacy;
    hooks.legacy = wait;
    return previous;
}

bool waitForDriverCompletion(u32 timeoutMs) FL_NO_EXCEPT {
    // Snapshot hooks: each singleton registers before accepting submissions.
    // No backend symbol is referenced from this dispatch implementation.
    const auto hooks = Singleton<DriverCompletionWaitHooks>::instance();
    const u32 started = millis();
    if (hooks.managed && !hooks.managed(timeoutMs)) {
        return false;
    }
    if (!hooks.legacy) {
        return true;
    }
    if (timeoutMs == 0) {
        return hooks.legacy(0);
    }
    const u32 elapsed = millis() - started;
    if (elapsed >= timeoutMs) {
        // Never pass zero: the public contract interprets it as infinity.
        return false;
    }
    return hooks.legacy(timeoutMs - elapsed);
}

} // namespace detail
} // namespace fl
