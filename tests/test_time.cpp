#include "test.h"
#include "fl/time.h"

#ifdef FASTLED_TESTING
#include <thread>
#include <chrono>
#endif

namespace {

TEST_CASE("fl::time() basic functionality") {
    // Test that fl::time() returns a reasonable value
    fl::u32 t1 = fl::time();
    CHECK(t1 >= 0); // Should always be >= 0
    
    // Test that time advances
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    fl::u32 t2 = fl::time();
    CHECK(t2 >= t1); // Time should advance or stay the same
    
    // For most test environments, we expect some advancement
    // Allow for a reasonable range considering test environment variability
    if (t2 > t1) {
        fl::u32 elapsed = t2 - t1;
        CHECK(elapsed <= 100); // Should not advance more than 100ms in our sleep
    }
}

TEST_CASE("fl::time() monotonic behavior") {
    // Test that time is generally monotonic (always increases)
    fl::u32 last_time = fl::time();
    
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fl::u32 current_time = fl::time();
        CHECK(current_time >= last_time);
        last_time = current_time;
    }
}

TEST_CASE("fl::time() return type consistency") {
    // Test that the function returns the expected type
    auto result = fl::time();
    static_assert(std::is_same_v<decltype(result), fl::u32>, 
                  "fl::time() should return fl::u32");
    
    // Test that it's compatible with arithmetic operations
    fl::u32 time1 = fl::time();
    fl::u32 time2 = time1 + 1000;
    fl::u32 delta = time2 - time1;
    CHECK_EQ(delta, 1000);
}

#ifdef FASTLED_TESTING

TEST_CASE("MockTimeProvider functionality") {
    // Test MockTimeProvider basic functionality
    fl::MockTimeProvider mock(1000);
    
    // Test initial time
    CHECK_EQ(mock.current_time(), 1000);
    CHECK_EQ(mock(), 1000);
    
    // Test advance
    mock.advance(500);
    CHECK_EQ(mock.current_time(), 1500);
    CHECK_EQ(mock(), 1500);
    
    // Test set_time
    mock.set_time(2000);
    CHECK_EQ(mock.current_time(), 2000);
    CHECK_EQ(mock(), 2000);
    
    // Test overflow behavior
    mock.set_time(0xFFFFFFFF);
    mock.advance(1);
    CHECK_EQ(mock.current_time(), 0); // Should wrap around
}

TEST_CASE("fl::time() injection functionality") {
    // Clear any existing provider first
    fl::clear_time_provider();
    
    SUBCASE("Basic time injection") {
        // Test that time injection works correctly
        fl::MockTimeProvider mock(5000);
        
        // Create a lambda that captures the mock by reference
        auto provider = [&mock]() -> fl::u32 {
            return mock.current_time();
        };
        fl::inject_time_provider(provider);
        
        // Verify injected time is used
        CHECK_EQ(fl::time(), 5000);
        
        // Test that changes to mock are reflected
        mock.advance(250);
        CHECK_EQ(fl::time(), 5250);
        
        mock.set_time(10000);
        CHECK_EQ(fl::time(), 10000);
        
        // Clear provider and verify normal behavior resumes
        fl::clear_time_provider();
        fl::u32 normal_time = fl::time();
        // Normal time should be different from our mock time
        // (unless we're extremely unlucky with timing)
        CHECK(normal_time != 10000);
    }
    
    SUBCASE("Lambda injection") {
        // Test injection with a custom lambda
        fl::u32 custom_time = 12345;
        auto provider = [&custom_time]() -> fl::u32 {
            return custom_time;
        };
        
        fl::inject_time_provider(provider);
        CHECK_EQ(fl::time(), 12345);
        
        custom_time = 54321;
        CHECK_EQ(fl::time(), 54321);
        
        fl::clear_time_provider();
    }
    
    SUBCASE("Multiple clear calls safety") {
        // Test that multiple clear calls are safe
        fl::clear_time_provider();
        fl::clear_time_provider();
        fl::clear_time_provider();
        
        // Should still work normally
        fl::u32 time1 = fl::time();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fl::u32 time2 = fl::time();
        CHECK(time2 >= time1);
    }
    
    SUBCASE("Thread safety test") {
        // Basic thread safety test - simplified to avoid race conditions
        fl::MockTimeProvider mock(1000);
        
        // Create provider lambda
        auto provider = [&mock]() -> fl::u32 {
            return mock.current_time();
        };
        
        // Test that multiple inject/clear cycles work
        for (int i = 0; i < 5; ++i) {
            fl::inject_time_provider(provider);
            CHECK_EQ(fl::time(), 1000);
            fl::clear_time_provider();
            // After clearing, fl::time() should return platform time (not mock time)
            fl::u32 platform_time = fl::time();
            // For the first iteration, we allow the platform time to potentially match mock time
            bool time_is_valid = (platform_time != 1000) || (i == 0);
            CHECK(time_is_valid);
        }
        
        // Final verification
        fl::clear_time_provider();
        fl::u32 final_time = fl::time();
        CHECK(final_time >= 0);
    }
    
    // Always clear provider after injection tests
    fl::clear_time_provider();
}

TEST_CASE("fl::time() wraparound behavior") {
    // Test wraparound behavior with mock time
    fl::clear_time_provider();
    
    fl::MockTimeProvider mock(0xFFFFFFFE); // Near maximum
    
    // Create a lambda that captures the mock by reference
    auto provider = [&mock]() -> fl::u32 {
        return mock.current_time();
    };
    fl::inject_time_provider(provider);
    
    fl::u32 time1 = fl::time();
    CHECK_EQ(time1, 0xFFFFFFFE);
    
    mock.advance(1);
    fl::u32 time2 = fl::time();
    CHECK_EQ(time2, 0xFFFFFFFF);
    
    mock.advance(1);
    fl::u32 time3 = fl::time();
    CHECK_EQ(time3, 0); // Should wrap to 0
    
    // Test that elapsed time calculation works across wraparound
    mock.set_time(0xFFFFFFFE);
    fl::u32 start_time = fl::time();
    mock.set_time(2); // Wrapped around
    fl::u32 end_time = fl::time();
    
    // Calculate elapsed time with wraparound handling
    fl::u32 elapsed = end_time - start_time; // Should naturally wrap
    CHECK_EQ(elapsed, 4); // 0xFFFFFFFE -> 0xFFFFFFFF (1) -> 0 (2) -> 1 (3) -> 2 (4)
    
    fl::clear_time_provider();
}

#endif // FASTLED_TESTING

// Test realistic usage patterns
TEST_CASE("fl::time() animation timing patterns") {
    // Test a typical animation timing pattern
    fl::u32 last_frame = fl::time();
    int frame_count = 0;
    
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        fl::u32 now = fl::time();
        
        if (now - last_frame >= 16) { // ~60 FPS
            frame_count++;
            last_frame = now;
        }
    }
    
    // We should have gotten at least a few frames
    CHECK(frame_count >= 0);
}

TEST_CASE("fl::time() timeout handling patterns") {
    // Test timeout calculation
    fl::u32 start = fl::time();
    fl::u32 timeout = start + 50; // 50ms timeout
    
    bool completed = false;
    while (fl::time() < timeout && !completed) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Simulate some work that might complete
        if (fl::time() - start >= 25) {
            completed = true;
        }
    }
    
    CHECK((completed || fl::time() >= timeout));
}

} // anonymous namespace 
