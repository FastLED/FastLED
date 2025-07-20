#include "test.h"
#include "fl/future.h"

using namespace fl;

TEST_CASE("fl::future - Basic Operations") {
    SUBCASE("Default constructor creates invalid future") {
        fl::future<int> f;
        CHECK(!f.valid());
        CHECK(!f.is_ready());
        CHECK(!f.has_error());
        CHECK(!f.is_pending());
    }
    
    SUBCASE("Static create() creates valid, pending future") {
        auto f = fl::future<int>::create();
        CHECK(f.valid());
        CHECK(!f.is_ready());
        CHECK(!f.has_error());
        CHECK(f.is_pending());
    }
    
    SUBCASE("Reset makes future invalid") {
        auto f = fl::future<int>::create();
        CHECK(f.valid());
        
        f.reset();
        CHECK(!f.valid());
    }
    
    SUBCASE("operator bool() returns true when ready") {
        auto f = fl::future<int>::create();
        CHECK(!f); // Should be false when pending
        
        f.complete_with_value(42);
        CHECK(f); // Should be true when ready
        
        auto f2 = fl::future<int>::create();
        f2.complete_with_error("Error");
        CHECK(!f2); // Should be false when in error state
    }
}

TEST_CASE("fl::future - Value Completion") {
    SUBCASE("Complete with value and retrieve") {
        auto f = fl::future<int>::create();
        CHECK(f.is_pending());
        
        bool success = f.complete_with_value(42);
        CHECK(success);
        CHECK(f.is_ready());
        CHECK(!f.has_error());
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 42);
    }
    
    SUBCASE("Double completion fails") {
        auto f = fl::future<int>::create();
        
        bool first = f.complete_with_value(42);
        CHECK(first);
        
        bool second = f.complete_with_value(99);
        CHECK(!second); // Should fail
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 42);
    }
}

TEST_CASE("fl::future - Error Handling") {
    SUBCASE("Complete with error") {
        auto f = fl::future<int>::create();
        
        bool success = f.complete_with_error("Test error");
        CHECK(success);
        CHECK(!f.is_ready());
        CHECK(f.has_error());
        
        CHECK_EQ(f.error_message(), "Test error");
        
        auto result = f.try_result();
        CHECK(result.empty());
    }
}

TEST_CASE("fl::future - Convenience Functions") {
    SUBCASE("make_ready_future") {
        auto f = fl::make_ready_future(123);
        CHECK(f.valid());
        CHECK(f.is_ready());
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 123);
    }
    
    SUBCASE("make_error_future") {
        auto f = fl::make_error_future<int>("Test error");
        CHECK(f.valid());
        CHECK(f.has_error());
        CHECK_EQ(f.error_message(), "Test error");
    }
    
    SUBCASE("make_invalid_future") {
        auto f = fl::make_invalid_future<int>();
        CHECK(!f.valid());
    }
}

TEST_CASE("fl::future - Move Semantics") {
    SUBCASE("Move constructor transfers ownership") {
        auto f1 = fl::future<int>::create();
        CHECK(f1.valid());
        
        auto f2 = fl::move(f1);
        CHECK(!f1.valid()); // Moved from
        CHECK(f2.valid());  // Moved to
        CHECK(f2.is_pending());
    }
    
    SUBCASE("Move assignment transfers ownership") {
        auto f1 = fl::future<int>::create();
        auto f2 = fl::future<int>();
        
        CHECK(f1.valid());
        CHECK(!f2.valid());
        
        f2 = fl::move(f1);
        CHECK(!f1.valid()); // Moved from
        CHECK(f2.valid());  // Moved to
        CHECK(f2.is_pending());
    }
}

TEST_CASE("fl::future - Complex Types") {
    SUBCASE("String future") {
        auto f = fl::future<fl::string>::create();
        fl::string test_value = "Hello World";
        
        bool success = f.complete_with_value(fl::move(test_value));
        CHECK(success);
        CHECK(f.is_ready());
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), "Hello World");
    }
    
    SUBCASE("Pending future returns empty result") {
        auto f = fl::future<int>::create();
        CHECK(f.is_pending());
        
        auto result = f.try_result();
        CHECK(result.empty());
    }
}

TEST_CASE("fl::future - Edge Cases") {
    SUBCASE("Operations on invalid future") {
        fl::future<int> f; // Invalid by default
        
        CHECK(!f.valid());
        CHECK(!f.complete_with_value(42));
        CHECK(!f.complete_with_error("Error"));
        
        auto result = f.try_result();
        CHECK(result.empty());
        
        CHECK(f.error_message().empty());
    }
    
    SUBCASE("Cannot complete with value after error") {
        auto f = fl::future<int>::create();
        
        f.complete_with_error("Error first");
        CHECK(f.has_error());
        
        bool value_completion = f.complete_with_value(42);
        CHECK(!value_completion); // Should fail
        CHECK(f.has_error()); // Should still be in error state
    }
    
    SUBCASE("Future state after reset") {
        auto f = fl::future<int>::create();
        f.complete_with_value(42);
        
        CHECK(f.is_ready());
        
        f.reset();
        
        // After reset, should be invalid
        CHECK(!f.valid());
        CHECK(!f.is_ready());
        CHECK(!f.has_error());
        CHECK(!f.is_pending());
        
        // Operations should fail
        CHECK(!f.complete_with_value(99));
        CHECK(!f.complete_with_error("New error"));
    }
}

TEST_CASE("fl::future - Event-Driven Usage Pattern") {
    SUBCASE("Async simulation with event loop") {
        // Simulate async calculation
        auto calculation_future = fl::future<int>::create();
        
        // Start "async" work (simulate background completion)
        int counter = 0;
        auto simulate_async_work = [&]() {
            counter++;
            if (counter >= 5) { // Simulate completion after 5 "ticks"
                calculation_future.complete_with_value(42);
                return true; // Done
            }
            return false; // Still working
        };
        
        // Event loop simulation
        bool completed = false;
        for (int i = 0; i < 10 && !completed; i++) {
            // Check if calculation is ready (non-blocking) using operator bool()
            if (calculation_future) { // Concise syntax!
                auto result = calculation_future.try_result();
                CHECK(!result.empty());
                CHECK_EQ(*result.ptr(), 42);
                completed = true;
            } else if (calculation_future.has_error()) {
                CHECK(false); // Should not error in this test
                completed = true;
            } else {
                // Continue async work
                simulate_async_work();
            }
        }
        
        CHECK(completed);
    }
    
    SUBCASE("Multiple futures pattern") {
        auto future1 = fl::future<int>::create();
        auto future2 = fl::future<fl::string>::create();
        auto future3 = fl::future<float>::create();
        
        // Complete futures in different orders
        future2.complete_with_value("Hello");
        future3.complete_with_error("Math error");
        future1.complete_with_value(100);
        
        // Check all futures using operator bool()
        CHECK(future1); // Concise ready check
        auto result1 = future1.try_result();
        CHECK(!result1.empty());
        CHECK_EQ(*result1.ptr(), 100);
        
        CHECK(future2); // Concise ready check
        auto result2 = future2.try_result();
        CHECK(!result2.empty());
        CHECK_EQ(*result2.ptr(), "Hello");
        
        CHECK(future3.has_error());
        CHECK_EQ(future3.error_message(), "Math error");
        auto result3 = future3.try_result();
        CHECK(result3.empty());
    }
} 
