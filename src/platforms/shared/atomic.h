#pragma once

// Atomic operations using GCC/Clang compiler intrinsics
// This implementation uses __atomic builtins which are supported by:
// - GCC 4.7+
// - Clang 3.1+
// - MinGW-W64
// - Most modern compilers
//
// No standard library headers required - pure compiler intrinsics

namespace fl {

// Memory ordering constants for clarity
// These map to __ATOMIC_* constants but are defined for documentation
enum memory_order {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
};

// Atomic type using GCC/Clang __atomic builtins
// Only implements operations needed by shared_ptr reference counting
template <typename T>
class AtomicReal {
public:
    AtomicReal() : mValue{} {}
    explicit AtomicReal(T value) : mValue(value) {}

    // Non-copyable and non-movable (matches std::atomic behavior)
    AtomicReal(const AtomicReal&) = delete;
    AtomicReal& operator=(const AtomicReal&) = delete;
    AtomicReal(AtomicReal&&) = delete;
    AtomicReal& operator=(AtomicReal&&) = delete;

    // Load operation
    T load() const {
        return __atomic_load_n(&mValue, __ATOMIC_ACQUIRE);
    }

    // Store operation
    void store(T value) {
        __atomic_store_n(&mValue, value, __ATOMIC_RELEASE);
    }

    // Pre-increment: ++atomic
    // Returns the NEW value after increment
    T operator++() {
        return __atomic_add_fetch(&mValue, 1, __ATOMIC_ACQ_REL);
    }

    // Pre-decrement: --atomic
    // Returns the NEW value after decrement
    T operator--() {
        return __atomic_sub_fetch(&mValue, 1, __ATOMIC_ACQ_REL);
    }

    // Assignment operator
    T operator=(T value) {
        store(value);
        return value;
    }

    // Conversion operator - returns current value
    operator T() const {
        return load();
    }

    // Fetch operations (used by various FastLED code)
    T fetch_add(T value) {
        return __atomic_fetch_add(&mValue, value, __ATOMIC_ACQ_REL);
    }

    T fetch_sub(T value) {
        return __atomic_fetch_sub(&mValue, value, __ATOMIC_ACQ_REL);
    }

private:
    mutable T mValue;  // mutable to allow load() on const objects
};

} // namespace fl
