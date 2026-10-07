// ok no header - public declarations remain in fl/task/executor.h

#include "fl/task/executor.h"
#include "fl/task/task_pump.h"
#include "fl/stl/atomic.h"
#include "fl/stl/functional.h"
#include "fl/stl/singleton.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/algorithm.h"
#include "fl/task/task.h"
#include "fl/stl/chrono.h"
#include "fl/log/log.h"

#include "fl/stl/new.h"
#include "fl/system/yield.h"
#include "platforms/coroutine_runtime.h"

namespace fl {
namespace task {

namespace detail {

namespace {
fl::atomic<TaskPump>& schedulerPump() FL_NO_EXCEPT {
    static fl::atomic<TaskPump> pump(nullptr);
    return pump;
}

fl::atomic<TaskPump>& executorPump() FL_NO_EXCEPT {
    static fl::atomic<TaskPump> pump(nullptr);
    return pump;
}
} // namespace

void set_scheduler_pump(TaskPump pump) FL_NO_EXCEPT {
    schedulerPump().store(pump, fl::memory_order_release);
}

void set_executor_pump(TaskPump pump) FL_NO_EXCEPT {
    executorPump().store(pump, fl::memory_order_release);
}

} // namespace detail

// Public API functions

namespace detail {

void pump_tasks() FL_NO_EXCEPT {
    if (auto pump = schedulerPump().load(fl::memory_order_acquire)) {
        pump();
    }
    // A timer callback may have registered the first executor runner above.
    if (auto pump = executorPump().load(fl::memory_order_acquire)) {
        pump();
    }
}

void run_impl(fl::u32 microseconds, ExecFlags flags, void (*pump)()) FL_NO_EXCEPT {
    // Re-entrancy guard: detect if run is called from within run
    bool& running = SingletonThreadLocal<bool>::instance();
    if (running) {
        FL_WARN_ONCE("task::run re-entrancy detected, skipping nested call");
        return;
    }
    running = true;
    auto guard = fl::make_scope_exit([&running]() { running = false; });

    const bool do_coroutines = flags & ExecFlags::COROUTINES;
    const bool do_system = flags & ExecFlags::SYSTEM;

    // Calculate start time with rollover protection
    fl::u32 begin_time = fl::micros();

    // Lambda to get elapsed time (rollover-safe)
    auto elapsed = [begin_time]() {
        return fl::micros() - begin_time;
    };

    // Lambda to get remaining time until deadline expires
    auto remaining = [elapsed, microseconds]() -> fl::u32 {
        fl::u32 e = elapsed();
        if (e >= microseconds) {
            return 0;
        }
        return microseconds - e;
    };

    // Lambda to check if deadline has expired
    auto expired = [remaining]() {
        return remaining() == 0;
    };

    do  {
        // TASKS: Scheduler (fl::task timers) + Executor (fetch, HTTP server, audio)
        if (pump) {
            pump();
        }

        // SYSTEM: OS-level yield.
        //
        // When the caller provided a non-zero microseconds budget we treat
        // this as an explicit spin-wait on hardware (typical DMA / driver
        // wait loops). In that case we MUST use a deep yield (>= 1 FreeRTOS
        // tick on ESP32) so that lower-priority network tasks — WiFi,
        // Ethernet lwIP, etc. — actually get CPU time. Previously this
        // path was gated on an active WiFi mode check, which missed
        // Ethernet-only deployments and also the pre-connection window
        // where `esp_wifi_get_mode` still returns WIFI_MODE_NULL. That
        // regression manifested as the ESP32-S3 I2S-vs-websockets
        // starvation reported in https://github.com/FastLED/FastLED/issues/2254
        // (Issue 1).
        //
        // When microseconds == 0 the caller wants the cheapest possible
        // yield (we are between other pumped subsystems and will loop
        // again immediately), so we keep the lightweight fl::yield() path.
        //
        // The sleep is clamped to whatever is left of the caller's budget,
        // mirroring the COROUTINES branch below. This used to be a hardcoded
        // pumpCoroutines(1000) that ignored `microseconds` entirely, so a
        // caller asking for run(250, SYSTEM) slept 1000 us and overshot its
        // own deadline by 750 us on every platform where pumpCoroutines
        // honors the requested duration (host/stub -- CoroutineRunner::run).
        //
        // ESP32 behavior is unchanged: pumpCoroutines() there converts us to
        // FreeRTOS ticks and floors at one tick for ANY non-zero request
        // (coroutine_esp32.impl.hpp, the `if (ticks == 0) ticks = 1` branch
        // added for #2254), so 250 and 1000 both yield exactly one tick. The
        // deep-yield guarantee #2254 depends on still holds.
        if (do_system) {
            if (microseconds > 0) {
                const fl::u32 time_left = remaining();
                if (time_left) {
                    fl::platforms::ICoroutineRuntime::instance().pumpCoroutines(
                        fl::min(1000u, time_left));
                }
            } else {
                fl::yield();
            }
        }

        // COROUTINES: Platform cooperative coroutines (pumpCoroutines)
        if (do_coroutines) {
            auto time_left = remaining();
            if (time_left) {
                fl::u32 sleep_us = fl::min(1000u, time_left);
                fl::platforms::ICoroutineRuntime::instance().pumpCoroutines(sleep_us);
            }
        }
    } while (!expired());
}

} // namespace detail

} // namespace task
} // namespace fl
