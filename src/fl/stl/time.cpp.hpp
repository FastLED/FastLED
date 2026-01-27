#include "fl/stl/time.h"
#include "fl/warn.h"
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
#ifdef FASTLED_TESTING
    // Check for injected time provider first
    {
        fl::unique_lock<fl::mutex> lock(get_time_mutex());
        const auto& provider = get_time_provider();
        if (provider) {
            return provider();
        }
    }
#endif

    // Use platform-specific implementation
    return fl::platform::millis();
}

fl::u32 micros() {
    // Note: micros() does not support time injection
    return fl::platform::micros();
}

} // namespace fl 
