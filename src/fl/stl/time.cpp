#include "fl/stl/time.h"
#include "fl/warn.h"

// Platform-specific headers
// NOTE: FASTLED_TESTING must be checked BEFORE FASTLED_STUB_IMPL because both can be defined
#if defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native platforms - use std::chrono
    #include <chrono>  // ok include
    #include "fl/compiler_control.h"

    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
    static const auto start_time = std::chrono::steady_clock::now();  // okay std namespace
    FL_DISABLE_WARNING_POP
#elif defined(__EMSCRIPTEN__)
    // WASM platform - use existing timer implementation
    #include <stdint.h>  // ok include
    extern "C" uint32_t millis(); // Defined in platforms/wasm/timer.cpp
    extern "C" uint32_t micros(); // Defined in platforms/wasm/timer.cpp
#elif defined(ESP32) || defined(ESP8266)
    // ESP platforms - use Arduino millis() and micros()
    #include <Arduino.h>  // ok include
#elif defined(__AVR__)
    // AVR platforms - use Arduino millis() and micros()
    #include <Arduino.h>  // ok include
#elif defined(FASTLED_ARM)
    // ARM platforms - use Arduino millis() and micros()
    #include <Arduino.h>  // ok include
#elif defined(FASTLED_STUB_IMPL)
    // Stub platform - use existing stub implementation
    #include <stdint.h>  // ok include
    extern "C" uint32_t millis(); // Defined in platforms/stub/time_stub.cpp
    extern "C" uint32_t micros(); // Defined in platforms/stub/time_stub.cpp
#else
    // Default fallback - assume Arduino-compatible millis() and micros() are available
    #include <Arduino.h>  // ok include
#endif

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

///////////////////// PLATFORM IMPLEMENTATIONS //////////////////////////////////////

namespace {

/// Get platform-specific time in milliseconds
/// This function contains all platform-specific timing code
fl::u32 get_platform_millis() {
#if defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native platforms - use std::chrono with consistent start time
    // NOTE: This must be checked BEFORE FASTLED_STUB_IMPL because both can be defined simultaneously
    auto current_time = std::chrono::steady_clock::now();  // okay std namespace
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);  // okay std namespace
    return static_cast<fl::u32>(elapsed.count());

#elif defined(__EMSCRIPTEN__)
    // WASM platform - delegate to existing millis() implementation
    return static_cast<fl::u32>(::millis());

#elif defined(ESP32) || defined(ESP8266)
    // ESP platforms - use Arduino millis()
    return static_cast<fl::u32>(::millis());

#elif defined(__AVR__)
    // AVR platforms - use Arduino millis()
    return static_cast<fl::u32>(::millis());

#elif defined(FASTLED_ARM)
    // ARM platforms - use Arduino millis()
    return static_cast<fl::u32>(::millis());

#elif defined(FASTLED_STUB_IMPL)
    // Stub platform - delegate to existing stub millis() implementation
    return static_cast<fl::u32>(::millis());

#else
    // Default fallback - assume Arduino-compatible millis() is available
    return static_cast<fl::u32>(::millis());

#endif
}

/// Get platform-specific time in microseconds
/// This function contains all platform-specific timing code
fl::u32 get_platform_micros() {
#if defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native platforms - use std::chrono with consistent start time
    // NOTE: This must be checked BEFORE FASTLED_STUB_IMPL because both can be defined simultaneously
    auto current_time = std::chrono::steady_clock::now();  // okay std namespace
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(current_time - start_time);  // okay std namespace
    return static_cast<fl::u32>(elapsed.count());

#elif defined(__EMSCRIPTEN__)
    // WASM platform - delegate to existing micros() implementation
    return static_cast<fl::u32>(::micros());

#elif defined(ESP32) || defined(ESP8266)
    // ESP platforms - use Arduino micros()
    return static_cast<fl::u32>(::micros());

#elif defined(__AVR__)
    // AVR platforms - use Arduino micros()
    return static_cast<fl::u32>(::micros());

#elif defined(FASTLED_ARM)
    // ARM platforms - use Arduino micros()
    return static_cast<fl::u32>(::micros());

#elif defined(FASTLED_STUB_IMPL)
    // Stub platform - delegate to existing stub micros() implementation
    return static_cast<fl::u32>(::micros());

#else
    // Default fallback - assume Arduino-compatible micros() is available
    return static_cast<fl::u32>(::micros());

#endif
}

} // anonymous namespace

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
    return get_platform_millis();
}

fl::u32 micros() {
    // Note: micros() does not support time injection in testing mode
    // If needed in the future, we could add a separate micros provider
    return get_platform_micros();
}

} // namespace fl 
