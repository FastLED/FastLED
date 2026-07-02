#pragma once
// allow-include-after-namespace

// IWYU pragma: private

/// @file slab_mutex.h
/// @brief Minimal mutex for the allocator layer (FastLED#3588).
///
/// fl/stl/allocator.h sits below vector/function in the include graph,
/// and the full fl/stl/mutex.h platform chain circles back into those
/// headers — so the slab allocator gets its own tiny lock with zero
/// fl:: dependencies beyond noexcept.
///
/// Semantics: plain mutual exclusion, non-recursive, never used from
/// ISRs (slab-backed containers are task-level constructs).

#include "fl/stl/noexcept.h"

#if defined(ESP32) || defined(ESP_PLATFORM)

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace detail {

class slab_mutex {
  public:
    slab_mutex() FL_NO_EXCEPT : mHandle(xSemaphoreCreateMutex()) {}
    ~slab_mutex() FL_NO_EXCEPT {
        if (mHandle) {
            vSemaphoreDelete(mHandle);
        }
    }
    slab_mutex(const slab_mutex &) FL_NO_EXCEPT = delete;
    slab_mutex &operator=(const slab_mutex &) FL_NO_EXCEPT = delete;

    void lock() FL_NO_EXCEPT {
        // Scheduler not running yet (static init) → single-threaded,
        // skip. Never called from ISRs by contract.
        if (mHandle && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            (void)xSemaphoreTake(mHandle, portMAX_DELAY);
        }
    }
    void unlock() FL_NO_EXCEPT {
        if (mHandle && xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            (void)xSemaphoreGive(mHandle);
        }
    }

  private:
    SemaphoreHandle_t mHandle;
};

} // namespace detail
} // namespace fl

#elif defined(FASTLED_TESTING) || defined(FL_IS_STUB) || defined(_WIN32) || \
    defined(__linux__) || defined(__APPLE__)

#include "fl/stl/atomic.h"

namespace fl {
namespace detail {

/// Host build: atomic spinlock. Slab critical sections are short
/// (bitset scans); on a preemptive host OS brief spinning is fine and
/// avoids pulling <mutex> or the fl/stl/mutex.h chain (include cycle)
/// into the allocator layer.
class slab_mutex {
  public:
    slab_mutex() FL_NO_EXCEPT : mFlag(0) {}
    slab_mutex(const slab_mutex &) FL_NO_EXCEPT = delete;
    slab_mutex &operator=(const slab_mutex &) FL_NO_EXCEPT = delete;

    void lock() FL_NO_EXCEPT {
        int expected = 0;
        while (!mFlag.compare_exchange_weak(expected, 1)) {
            expected = 0;
        }
    }
    void unlock() FL_NO_EXCEPT { mFlag.store(0); }

  private:
    fl::atomic<int> mFlag;
};

} // namespace detail
} // namespace fl

#else

namespace fl {
namespace detail {

/// Single-threaded MCUs: no-op.
class slab_mutex {
  public:
    slab_mutex() FL_NO_EXCEPT = default;
    void lock() FL_NO_EXCEPT {}
    void unlock() FL_NO_EXCEPT {}
};

} // namespace detail
} // namespace fl

#endif

namespace fl {
namespace detail {

/// RAII guard for slab_mutex.
class slab_lock_guard {
  public:
    explicit slab_lock_guard(slab_mutex &m) FL_NO_EXCEPT : mMutex(m) {
        mMutex.lock();
    }
    ~slab_lock_guard() FL_NO_EXCEPT { mMutex.unlock(); }
    slab_lock_guard(const slab_lock_guard &) FL_NO_EXCEPT = delete;
    slab_lock_guard &operator=(const slab_lock_guard &) FL_NO_EXCEPT = delete;

  private:
    slab_mutex &mMutex;
};

} // namespace detail
} // namespace fl
