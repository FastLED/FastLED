#include "fl/time.h"
#include "fl/warn.h"

// Platform-specific headers
#if defined(__EMSCRIPTEN__)
    // WASM platform - use existing timer implementation
    #include <stdint.h>  // ok include
    extern "C" uint32_t millis(); // Defined in platforms/wasm/timer.cpp
#elif defined(ESP32) || defined(ESP8266)
    // ESP platforms - use Arduino millis()
    #include <Arduino.h>  // ok include
#elif defined(__AVR__)
    // AVR platforms - use Arduino millis()
    #include <Arduino.h>  // ok include
#elif defined(FASTLED_ARM)
    // ARM platforms - use Arduino millis()
    #include <Arduino.h>  // ok include
#elif defined(FASTLED_STUB_IMPL)
    // Stub platform - use existing stub implementation
    #include <stdint.h>  // ok include
    extern "C" uint32_t millis(); // Defined in platforms/stub/generic/led_sysdefs_generic.hpp
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native platforms - use std::chrono
    #include <chrono>  // ok include
    #include "fl/compiler_control.h"
    
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
    static const auto start_time = std::chrono::steady_clock::now();
    FL_DISABLE_WARNING_POP
#else
    // Default fallback - assume Arduino-compatible millis() is available
    #include <Arduino.h>  // ok include
#endif

#ifdef FASTLED_TESTING
    #include "fl/mutex.h"
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
    fl::lock_guard<fl::mutex> lock(get_time_mutex());
    get_time_provider() = provider;
}

void clear_time_provider() {
    fl::lock_guard<fl::mutex> lock(get_time_mutex());
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

///////////////////// PLATFORM IMPLEMENTATIONS //////////////////////////////////////

namespace {

/// Get platform-specific time in milliseconds
/// This function contains all platform-specific timing code
fl::u32 get_platform_time() {
#if defined(__EMSCRIPTEN__)
    // WASM platform - delegate to existing millis() implementation
    return static_cast<fl::u32>(millis());
    
#elif defined(ESP32) || defined(ESP8266)
    // ESP platforms - use Arduino millis()
    return static_cast<fl::u32>(millis());
    
#elif defined(__AVR__)
    // AVR platforms - use Arduino millis()
    return static_cast<fl::u32>(millis());
    
#elif defined(FASTLED_ARM)
    // ARM platforms - use Arduino millis()
    return static_cast<fl::u32>(millis());
    
#elif defined(FASTLED_STUB_IMPL)
    // Stub platform - delegate to existing stub millis() implementation
    return static_cast<fl::u32>(millis());
    
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native platforms - use std::chrono with consistent start time
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
    return static_cast<fl::u32>(elapsed.count());
    
#else
    // Default fallback - assume Arduino-compatible millis() is available
    return static_cast<fl::u32>(millis());
    
#endif
}

} // anonymous namespace

///////////////////// PUBLIC API //////////////////////////////////////

fl::u32 time() {
#ifdef FASTLED_TESTING
    // Check for injected time provider first
    {
        fl::lock_guard<fl::mutex> lock(get_time_mutex());
        const auto& provider = get_time_provider();
        if (provider) {
            return provider();
        }
    }
#endif
    
    // Use platform-specific implementation
    return get_platform_time();
}

} // namespace fl 
