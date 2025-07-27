#pragma once

#include "fl/thread.h"
#include "fl/int.h"
#include "fl/align.h"

#if FASTLED_MULTITHREADED
#include <atomic>
#endif

namespace fl {



#if FASTLED_MULTITHREADED
template <typename T>
using atomic = std::atomic<T>;
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

template <typename T> class AtomicFake {
  public:
    AtomicFake() : mValue{} {}
    explicit AtomicFake(T value) : mValue(value) {}
    
    // Non-copyable and non-movable
    AtomicFake(const AtomicFake&) = delete;
    AtomicFake& operator=(const AtomicFake&) = delete;
    AtomicFake(AtomicFake&&) = delete;
    AtomicFake& operator=(AtomicFake&&) = delete;
    
    // Basic atomic operations - fake implementation (not actually atomic)
    T load() const {
        return mValue;
    }
    
    void store(T value) {
        mValue = value;
    }
    
    T exchange(T value) {
        T old = mValue;
        mValue = value;
        return old;
    }
    
    bool compare_exchange_weak(T& expected, T desired) {
        if (mValue == expected) {
            mValue = desired;
            return true;
        } else {
            expected = mValue;
            return false;
        }
    }
    
    bool compare_exchange_strong(T& expected, T desired) {
        return compare_exchange_weak(expected, desired);
    }
    
    // Assignment operator
    T operator=(T value) {
        store(value);
        return value;
    }
    
    // Conversion operator
    operator T() const {
        return load();
    }
    
    // Arithmetic operators (for integral and floating point types)
    T operator++() {
        return ++mValue;
    }
    
    T operator++(int) {
        return mValue++;
    }
    
    T operator--() {
        return --mValue;
    }
    
    T operator--(int) {
        return mValue--;
    }
    
    T operator+=(T value) {
        mValue += value;
        return mValue;
    }
    
    T operator-=(T value) {
        mValue -= value;
        return mValue;
    }
    
    T operator&=(T value) {
        mValue &= value;
        return mValue;
    }
    
    T operator|=(T value) {
        mValue |= value;
        return mValue;
    }
    
    T operator^=(T value) {
        mValue ^= value;
        return mValue;
    }
    
    // Fetch operations
    T fetch_add(T value) {
        T old = mValue;
        mValue += value;
        return old;
    }
    
    T fetch_sub(T value) {
        T old = mValue;
        mValue -= value;
        return old;
    }
    
    T fetch_and(T value) {
        T old = mValue;
        mValue &= value;
        return old;
    }
    
    T fetch_or(T value) {
        T old = mValue;
        mValue |= value;
        return old;
    }
    
    T fetch_xor(T value) {
        T old = mValue;
        mValue ^= value;
        return old;
    }
    
  private:
    FL_ALIGN_AS(T) T mValue;
};

} // namespace fl
