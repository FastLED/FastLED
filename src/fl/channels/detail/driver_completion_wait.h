#pragma once

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

/// Optional completion consumers. Registering a consumer links its backend;
/// waiting alone never constructs a channel registry or a peripheral.
using DriverCompletionWait = bool (*)(u32 timeoutMs);

/// Internal singleton hooks; return the previous hook to allow scoped tests.
DriverCompletionWait setManagedCompletionWait(DriverCompletionWait wait) FL_NO_EXCEPT;
DriverCompletionWait setLegacyCompletionWait(DriverCompletionWait wait) FL_NO_EXCEPT;

/// Wait for installed consumers using one deadline (zero means indefinitely).
/// False reports a consumer error or an exhausted timeout.
bool waitForDriverCompletion(u32 timeoutMs) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl
