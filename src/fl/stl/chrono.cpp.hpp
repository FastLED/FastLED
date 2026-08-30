#include "fl/stl/chrono.h"
#include "fl/log/log.h"
#include "fl/stl/mutex.h"
#include "platforms/time_platform.h"

#ifdef FASTLED_TESTING
    #include "fl/stl/mutex.h"
#endif

namespace fl {

///////////////////// TESTING SUPPORT //////////////////////////////////////

#ifdef FASTLED_TESTING

namespace {
    // Thread-safe storage for injected time provider
    fl::mutex& get_time_mutex() {
        static fl::mutex mutex;
        return mutex;
    }

    time_provider_t& get_time_provider() {
        static time_provider_t provider;
        return provider;
    }

    bool get_injected_millis(fl::u32* value) FL_NO_EXCEPT {
        fl::unique_lock<fl::mutex> lock(get_time_mutex());
        const auto& provider = get_time_provider();
        if (!provider) {
            return false;
        }
        *value = provider();
        return true;
    }
}

void inject_time_provider(const time_provider_t& provider) {
    fl::unique_lock<fl::mutex> lock(get_time_mutex());
    get_time_provider() = provider;
}

void clear_time_provider() {
    fl::unique_lock<fl::mutex> lock(get_time_mutex());
    get_time_provider() = time_provider_t{}; // Clear the function
}

// MockTimeProvider implementation
MockTimeProvider::MockTimeProvider(fl::u32 initial_time)
    : mCurrentTime(initial_time) {
}

void MockTimeProvider::advance(fl::u32 milliseconds) {
    mCurrentTime += milliseconds;
}

void MockTimeProvider::set_time(fl::u32 milliseconds) {
    mCurrentTime = milliseconds;
}

fl::u32 MockTimeProvider::current_time() const {
    return mCurrentTime;
}

fl::u32 MockTimeProvider::operator()() const {
    return mCurrentTime;
}

#endif // FASTLED_TESTING

///////////////////// PUBLIC API //////////////////////////////////////

fl::u32 millis() {
#if FL_CHRONO_HAS_TEST_TIME_PROVIDER
    fl::u32 injected_millis = 0;
    if (get_injected_millis(&injected_millis)) {
        return injected_millis;
    }
#endif

    // Use platform-specific implementation
    return fl::platforms::millis();
}

fl::u32 micros() {
#if FL_CHRONO_HAS_TEST_TIME_PROVIDER
    fl::u32 injected_millis = 0;
    if (get_injected_millis(&injected_millis)) {
        return injected_millis * 1000u;
    }
#endif

    return fl::platforms::micros();
}

namespace {
    struct Micros64State {
        fl::u64 accumulated = 0;
        fl::u32 last_micros = 0;
        bool initialized = false;
        fl::mutex mutex;
    };

    Micros64State& get_micros64_state() FL_NO_EXCEPT {
        static Micros64State state;
        return state;
    }
}

#if FL_CHRONO_HAS_TEST_TIME_PROVIDER
void micros64_reset() FL_NO_EXCEPT {
    Micros64State& state = get_micros64_state();
    fl::unique_lock<fl::mutex> lock(state.mutex);
    state.accumulated = 0;
    state.last_micros = 0;
    state.initialized = false;
}
#endif

fl::u64 micros64() FL_NO_EXCEPT {
    Micros64State& state = get_micros64_state();
    fl::unique_lock<fl::mutex> lock(state.mutex);
    const fl::u32 current = fl::micros();
    if (!state.initialized) {
        state.accumulated = current;
        state.last_micros = current;
        state.initialized = true;
        return state.accumulated;
    }
    state.accumulated += static_cast<fl::u32>(current - state.last_micros);
    state.last_micros = current;
    return state.accumulated;
}

chrono::steady_clock::time_point chrono::steady_clock::now() FL_NO_EXCEPT {
    return time_point(duration(static_cast<fl::i64>(fl::micros64())));
}

chrono::system_clock::time_point chrono::system_clock::now() FL_NO_EXCEPT {
    return time_point(duration(static_cast<fl::i64>(fl::micros64())));
}

namespace {
    // Thread-safe 64-bit millisecond counter state
    struct Millis64State {
        fl::u64 accumulated = 0;
        fl::u32 last_millis = 0;
        bool initialized = false;
        fl::mutex mutex;
    };

    Millis64State& get_millis64_state() {
        static Millis64State state;
        return state;
    }
}

void millis64_reset() {
    Millis64State& state = get_millis64_state();
    fl::unique_lock<fl::mutex> lock(state.mutex);
    state.accumulated = 0;
    state.last_millis = 0;
    state.initialized = false;
}

fl::u64 millis64() {
    Millis64State& state = get_millis64_state();
    fl::u32 current_millis = fl::millis();
    fl::unique_lock<fl::mutex> lock(state.mutex);

    if (!state.initialized) {
        // First call, set initial value
        state.accumulated = current_millis;
        state.last_millis = current_millis;
        state.initialized = true;
        return state.accumulated;
    }

    // Detect wraparound: current < last means 32-bit counter wrapped
    // Delta calculation handles wraparound correctly via unsigned arithmetic
    fl::u32 delta = current_millis - state.last_millis;

    // Accumulate the delta into 64-bit counter
    state.accumulated += delta;

    // Update last value for next call
    state.last_millis = current_millis;

    return state.accumulated;
}

} // namespace fl
