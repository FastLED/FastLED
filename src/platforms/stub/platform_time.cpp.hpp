#pragma once

// IWYU pragma: private

// Only compile for stub platform (exclude WASM which has its own implementation)
#include "platforms/wasm/is_wasm.h"

#if defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)

#include "fl/stl/thread.h"
#include "fl/stl/atomic.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/function.h"
#include "fl/stl/chrono.h"  // platform-specific time implementation
#include "fl/stl/noexcept.h"
#include "platforms/stub/time_stub.h"

// Forward declare delay override (defined in time_stub.cpp.hpp)
extern fl::function<void(fl::u32)> g_delay_override;

namespace {
    // Shared start time for millis/micros consistency
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
    static const auto start_time = std::chrono::system_clock::now();  // okay std namespace
    FL_DISABLE_WARNING_POP
}

namespace fl {
namespace platforms {

void delay(fl::u32 ms) FL_NO_EXCEPT {
    // Check for test override first (for fast testing)
    if (g_delay_override) {
        g_delay_override(ms);
        return;
    }
    fl::this_thread::sleep_for(fl::chrono::milliseconds(ms));
}

void delayMicroseconds(fl::u32 us) FL_NO_EXCEPT {
    // No override for microseconds (precise hardware timing)
    fl::this_thread::sleep_for(fl::chrono::microseconds(us));
}

// ---------------------------------------------------------------------------
// Single root clock.
//
// millis() and micros() both derive from micros64() rather than each taking
// their own independent reading of the host clock. Two independent reads could
// disagree — micros() could report 1'999'999 while a millis() taken a moment
// later reports 2000 — and there is only ever one thing to stub.
//
// The 64-bit root also keeps the derivation honest: the u32 return types are a
// truncation applied at the very end (preserving the existing wrap semantics
// callers expect), not a limit baked into the source of truth.
// ---------------------------------------------------------------------------

// Test seam. Atomic because it is read from every thread that asks the time —
// including the stub watchdog's timer thread — while the installing test
// writes it from the main thread. A plain function pointer (rather than
// fl::function, as g_delay_override uses) keeps that read lock-free, which
// matters because micros() sits on hot paths.
fl::atomic<fl::u64 (*)()>& clockOverrideRef() FL_NO_EXCEPT {
    static fl::atomic<fl::u64 (*)()> fn{nullptr};
    return fn;
}

fl::u64 micros64() FL_NO_EXCEPT {
    fl::u64 (*fn)() = clockOverrideRef().load();
    if (fn) {
        return fn();
    }
    auto current = std::chrono::system_clock::now();  // okay std namespace
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(  // okay std namespace
        current - start_time);
    return static_cast<fl::u64>(elapsed.count());
}

fl::u32 millis() FL_NO_EXCEPT {
    return static_cast<fl::u32>(micros64() / 1000u);
}

fl::u32 micros() FL_NO_EXCEPT {
    return static_cast<fl::u32>(micros64());
}


}  // namespace platforms
}  // namespace fl

// Test-facing clock override, mirroring setDelayFunction/clearDelayFunction.
// Installing a source freezes host time: it advances only when the test says
// so, which is what makes timing assertions deterministic instead of a bet on
// the scheduler. Pass nullptr (or call clearClockFunction) to restore real time.
void setClockFunction(fl::u64 (*micros64Func)()) FL_NO_EXCEPT {
    fl::platforms::clockOverrideRef().store(micros64Func);
}

void clearClockFunction() FL_NO_EXCEPT {
    fl::platforms::clockOverrideRef().store(nullptr);
}

bool isClockOverrideActive() FL_NO_EXCEPT {
    return fl::platforms::clockOverrideRef().load() != nullptr;
}

#endif  // FASTLED_STUB_IMPL && !__EMSCRIPTEN__
