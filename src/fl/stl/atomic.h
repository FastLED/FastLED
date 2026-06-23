#pragma once

#include "fl/stl/thread_config.h"  // For FASTLED_MULTITHREADED (no circular deps)
#include "fl/stl/int.h"

// Determine if we need real atomics:
// 1. Multi-threaded mode (pthread support)
// 2. ESP32 platforms (ISRs require atomic operations, and ESP32 has fast hardware atomics)
// Note: ESP8266 excluded - lacks native atomic support, would require linking libatomic
#if FASTLED_MULTITHREADED || defined(FL_IS_ESP32)
#define FASTLED_USE_REAL_ATOMICS 1
#include "platforms/atomic.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"
#else
#define FASTLED_USE_REAL_ATOMICS 0
#endif

namespace fl {

#if FASTLED_USE_REAL_ATOMICS
template <typename T>
using atomic = AtomicReal<T>;
#else
template <typename T> class AtomicFake;
template <typename T>
using atomic = AtomicFake<T>;
#endif

using atomic_bool = atomic<bool>;
using atomic_int = atomic<int>;
using atomic_uint = atomic<unsigned int>;
using atomic_u32 = atomic<fl::u32>;
using atomic_i32 = atomic<fl::i32>;

///////////////////// IMPLEMENTATION //////////////////////////////////////

// Forward declare memory_order enum (defined in platforms/shared/atomic.h for real atomics)
#if !FASTLED_USE_REAL_ATOMICS
enum memory_order { // ok plain enum
    memory_order_relaxed,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};
#endif

template <typename T> class AtomicFake {
  public:
    AtomicFake() FL_NOEXCEPT : mValue{} {}
    explicit AtomicFake(T value) FL_NOEXCEPT : mValue(value) {}

    // Non-copyable and non-movable
    AtomicFake(const AtomicFake&) FL_NOEXCEPT = delete;
    AtomicFake& operator=(const AtomicFake&) FL_NOEXCEPT = delete;
    AtomicFake(AtomicFake&&) FL_NOEXCEPT = delete;
    AtomicFake& operator=(AtomicFake&&) FL_NOEXCEPT = delete;

    // Basic atomic operations - fake implementation (not actually atomic)
    // Memory order parameters are accepted but ignored (no-op in single-threaded mode)
    T load(memory_order = memory_order_seq_cst) const FL_NOEXCEPT {
        return mValue;
    }

    void store(T value, memory_order = memory_order_seq_cst) FL_NOEXCEPT {
        mValue = value;
    }
    
    T exchange(T value, memory_order = memory_order_seq_cst) FL_NOEXCEPT {
        T old = mValue;
        mValue = value;
        return old;
    }
    
    bool compare_exchange_weak(T& expected, T desired, memory_order = memory_order_seq_cst) FL_NOEXCEPT {
        if (mValue == expected) {
            mValue = desired;
            return true;
        } else {
            expected = mValue;
            return false;
        }
    }

    bool compare_exchange_strong(T& expected, T desired, memory_order = memory_order_seq_cst) FL_NOEXCEPT {
        return compare_exchange_weak(expected, desired);
    }
    
    // Assignment operator
    T operator=(T value) FL_NOEXCEPT {
        store(value);
        return value;
    }
    
    // Conversion operator
    operator T() const FL_NOEXCEPT {
        return load();
    }
    
    // Arithmetic operators (for integral and floating point types)
    // Avoid deprecated volatile increment/decrement (C++20)
    T operator++() FL_NOEXCEPT {
        T temp = mValue + 1;
        mValue = temp;
        return temp;
    }
    
    T operator++(int) FL_NOEXCEPT {
        T temp = mValue;
        mValue = temp + 1;
        return temp;
    }
    
    T operator--() FL_NOEXCEPT {
        T temp = mValue - 1;
        mValue = temp;
        return temp;
    }
    
    T operator--(int) FL_NOEXCEPT {
        T temp = mValue;
        mValue = temp - 1;
        return temp;
    }
    
    T operator+=(T value) FL_NOEXCEPT {
        T temp = mValue + value;
        mValue = temp;
        return temp;
    }
    
    T operator-=(T value) FL_NOEXCEPT {
        T temp = mValue - value;
        mValue = temp;
        return temp;
    }
    
    T operator&=(T value) FL_NOEXCEPT {
        T temp = mValue & value;
        mValue = temp;
        return temp;
    }
    
    T operator|=(T value) FL_NOEXCEPT {
        T temp = mValue | value;
        mValue = temp;
        return temp;
    }
    
    T operator^=(T value) FL_NOEXCEPT {
        T temp = mValue ^ value;
        mValue = temp;
        return temp;
    }
    
    // Fetch operations
    T fetch_add(T value) FL_NOEXCEPT {
        T old = mValue;
        T temp = old + value;
        mValue = temp;
        return old;
    }
    
    T fetch_sub(T value) FL_NOEXCEPT {
        T old = mValue;
        T temp = old - value;
        mValue = temp;
        return old;
    }
    
    T fetch_and(T value) FL_NOEXCEPT {
        T old = mValue;
        T temp = old & value;
        mValue = temp;
        return old;
    }
    
    T fetch_or(T value) FL_NOEXCEPT {
        T old = mValue;
        T temp = old | value;
        mValue = temp;
        return old;
    }
    
    T fetch_xor(T value) FL_NOEXCEPT {
        T old = mValue;
        T temp = old ^ value;
        mValue = temp;
        return old;
    }
    
  private:
    T volatile mValue;
};

} // namespace fl
