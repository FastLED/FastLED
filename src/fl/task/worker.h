#pragma once

/// @file fl/task/worker.h
/// @brief One-shot functor worker — post work to a specific CPU core and
///        block the calling thread for completion.
///
/// Designed for offloading a short-lived functor (e.g. one tick of audio
/// processing) onto a pinned FreeRTOS task so its CPU cost lands on a
/// different physical core than the caller. The caller blocks in a
/// semaphore while the worker runs, so the caller's host core is free for
/// OS/WiFi/IRQ work during the block.
///
/// Platform matrix:
///
/// | Platform                         | run(fn) behaviour                              |
/// |----------------------------------|------------------------------------------------|
/// | ESP32 dual-core (dev / S3 / P4)  | Hops fn onto a FreeRTOS task pinned to `core`. |
/// | ESP32 single-core (S2/C2/C3/...) | Synchronous call on the calling thread.        |
/// | Non-ESP32 (AVR / ARM / host)     | Synchronous call on the calling thread.        |
///
/// The worker is explicitly one-shot-per-call: fn cannot yield or resume;
/// it must run to completion. The semaphore handshake assumes a single
/// in-flight functor at any time. Concurrent calls to run() from multiple
/// threads against the same Worker are serialised through an internal
/// mutex — last caller wins the semaphore; everyone blocks in order.
///
/// Typical usage:
/// @code
/// fl::task::Worker worker(0);      // pin to core 0 on dual-core ESP32
/// worker.run([&]() {
///     processor->update(sample);   // runs on core 0; caller blocks here
/// });
/// // caller resumes once worker signals done
/// @endcode

// allow-include-after-namespace

#include "fl/stl/functional.h"
#include "fl/stl/string.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace task {

class WorkerImpl;  // forward-decl; defined in worker.cpp.hpp per platform

/// @brief One-shot functor worker pinnable to a specific CPU core.
class Worker {
public:
    /// Construct a worker.
    /// @param core  CPU core ID to pin the worker task to. On platforms
    ///              without usable SMP affinity (single-core ESP32,
    ///              AVR/ARM), the value is ignored and run() is
    ///              synchronous. -1 means "no pinning".
    /// @param name  FreeRTOS task name (debug/trace only).
    /// @param stack_size Stack size in bytes for the underlying FreeRTOS task
    ///              on platforms that spawn one. Ignored elsewhere.
    explicit Worker(int core = -1,
                    const char* name = "fl_worker",
                    size_t stack_size = 4096) FL_NOEXCEPT;

    ~Worker() FL_NOEXCEPT;

    Worker(const Worker&) FL_NOEXCEPT = delete;
    Worker& operator=(const Worker&) FL_NOEXCEPT = delete;
    Worker(Worker&&) FL_NOEXCEPT = delete;
    Worker& operator=(Worker&&) FL_NOEXCEPT = delete;

    /// Post fn to the worker and block the caller until it returns.
    /// On platforms without useful cross-core affinity this is equivalent
    /// to calling fn() directly on the caller's thread.
    void run(fl::function<void()> fn) FL_NOEXCEPT;

    /// @return true if this worker spawned a real pinned task (dual-core
    ///         ESP32) and will actually cross a core boundary on run().
    ///         false on single-core or non-ESP32, where run() is synchronous.
    bool isPinned() const FL_NOEXCEPT;

    /// @return The core ID this worker was requested to pin to (may be -1).
    int coreId() const FL_NOEXCEPT { return mCoreId; }

private:
    int mCoreId;
    WorkerImpl* mImpl;  ///< Owned; platform-specific payload. Null on
                        ///< synchronous-fallback platforms.
};

} // namespace task
} // namespace fl
