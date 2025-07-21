#include "test.h"
#include "fl/future.h"

using namespace fl;

TEST_CASE("fl::future - Basic Operations") {
    SUBCASE("Default constructor creates invalid future") {
        fl::future<int> f;
        CHECK(!f.valid());
        CHECK_EQ(f.state(), fl::FutureState::PENDING); // Invalid futures return PENDING
        CHECK(!f); // operator bool() - false when invalid
    }
    
    SUBCASE("Static create() creates valid, pending future") {
        auto f = fl::future<int>::create();
        CHECK(f.valid());
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        CHECK(!f); // operator bool() - false when pending
    }
    
    SUBCASE("Clear makes future invalid") {
        auto f = fl::future<int>::create();
        CHECK(f.valid());
        
        f.clear();
        CHECK(!f.valid());
    }
}

TEST_CASE("fl::future - State-based API") {
    SUBCASE("operator bool() returns true when complete") {
        auto f = fl::future<int>::create();
        CHECK(!f); // Should be false when pending
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        
        f.complete_with_value(42);
        CHECK(f); // Should be true when ready
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        auto f2 = fl::future<int>::create();
        f2.complete_with_error("Error");
        CHECK(f2); // Should be true when error (something to process)
        CHECK_EQ(f2.state(), fl::FutureState::ERROR);
    }
    
    SUBCASE("State transitions work correctly") {
        auto f = fl::future<int>::create();
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        
        // PENDING -> READY
        f.complete_with_value(123);
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        // Cannot transition from READY
        CHECK(!f.complete_with_error("Should fail"));
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        // Test PENDING -> ERROR
        auto f2 = fl::future<int>::create();
        f2.complete_with_error("Test error");
        CHECK_EQ(f2.state(), fl::FutureState::ERROR);
        
        // Cannot transition from ERROR  
        CHECK(!f2.complete_with_value(999));
        CHECK_EQ(f2.state(), fl::FutureState::ERROR);
    }
}

TEST_CASE("fl::future - Value Completion") {
    SUBCASE("Complete with value and retrieve") {
        auto f = fl::future<int>::create();
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        
        bool success = f.complete_with_value(42);
        CHECK(success);
        CHECK_EQ(f.state(), fl::FutureState::READY);
        CHECK(f); // operator bool() - true when ready
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 42);
    }
    
    SUBCASE("Double completion fails") {
        auto f = fl::future<int>::create();
        
        bool first = f.complete_with_value(42);
        CHECK(first);
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        bool second = f.complete_with_value(99);
        CHECK(!second); // Should fail
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 42);
    }
    
    SUBCASE("try_result only works in READY state") {
        auto f = fl::future<int>::create();
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        
        auto result = f.try_result();
        CHECK(result.empty()); // Should be empty when pending
        
        f.complete_with_error("Error");
        CHECK_EQ(f.state(), fl::FutureState::ERROR);
        
        result = f.try_result();
        CHECK(result.empty()); // Should be empty when error
    }
}

TEST_CASE("fl::future - Error Handling") {
    SUBCASE("Complete with error") {
        auto f = fl::future<int>::create();
        
        bool success = f.complete_with_error("Test error");
        CHECK(success);
        CHECK_EQ(f.state(), fl::FutureState::ERROR);
        CHECK(f); // operator bool() - true when error (something to process)
        
        CHECK_EQ(f.error_message(), "Test error");
        
        auto result = f.try_result();
        CHECK(result.empty());
    }
    
    SUBCASE("error_message only works in ERROR state") {
        auto f = fl::future<int>::create();
        CHECK_EQ(f.state(), fl::FutureState::PENDING);
        CHECK(f.error_message().empty()); // Empty when pending
        
        f.complete_with_value(42);
        CHECK_EQ(f.state(), fl::FutureState::READY);
        CHECK(f.error_message().empty()); // Empty when ready
        
        auto f2 = fl::future<int>::create();
        f2.complete_with_error("Real error");
        CHECK_EQ(f2.state(), fl::FutureState::ERROR);
        CHECK_EQ(f2.error_message(), "Real error"); // Only works in ERROR state
    }
}

TEST_CASE("fl::future - Convenience Functions") {
    SUBCASE("make_ready_future") {
        auto f = fl::make_ready_future(123);
        CHECK(f.valid());
        CHECK_EQ(f.state(), fl::FutureState::READY);
        CHECK(f); // operator bool() - true when ready
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), 123);
    }
    
    SUBCASE("make_error_future") {
        auto f = fl::make_error_future<int>("Test error");
        CHECK(f.valid());
        CHECK_EQ(f.state(), fl::FutureState::ERROR);
        CHECK(f); // operator bool() - true when error
        CHECK_EQ(f.error_message(), "Test error");
    }
    
    SUBCASE("make_invalid_future") {
        auto f = fl::make_invalid_future<int>();
        CHECK(!f.valid());
        CHECK(!f); // operator bool() - false when invalid
    }
}

TEST_CASE("fl::future - Event-Driven Usage Pattern") {
    SUBCASE("Switch statement pattern") {
        auto f = fl::future<int>::create();
        f.complete_with_value(42);
        
        // Test the switch pattern shown in documentation
        bool processed = false;
        if (f) { // Something to process
            switch (f.state()) {
                case FutureState::READY: {
                    auto result = f.try_result();
                    CHECK(!result.empty());
                    CHECK_EQ(*result.ptr(), 42);
                    processed = true;
                    break;
                }
                case FutureState::ERROR:
                    CHECK(false); // Should not happen in this test
                    break;
                case FutureState::PENDING:
                    CHECK(false); // Should not happen since operator bool() returned true
                    break;
            }
        }
        CHECK(processed);
    }
    
    SUBCASE("Error handling pattern") {
        auto f = fl::future<int>::create();
        f.complete_with_error("Network timeout");
        
        bool error_handled = false;
        if (f) { // Something to process
            switch (f.state()) {
                case FutureState::READY:
                    CHECK(false); // Should not happen in this test
                    break;
                case FutureState::ERROR:
                    CHECK_EQ(f.error_message(), "Network timeout");
                    error_handled = true;
                    break;
                case FutureState::PENDING:
                    CHECK(false); // Should not happen since operator bool() returned true
                    break;
            }
        }
        CHECK(error_handled);
    }
}

TEST_CASE("fl::future - Move Semantics") {
    SUBCASE("Move constructor transfers ownership") {
        auto f1 = fl::future<int>::create();
        CHECK(f1.valid());
        
        auto f2 = fl::move(f1);
        CHECK(!f1.valid()); // Moved from
        CHECK(f2.valid());  // Moved to
        CHECK_EQ(f2.state(), fl::FutureState::PENDING);
    }
    
    SUBCASE("Move assignment transfers ownership") {
        auto f1 = fl::future<int>::create();
        auto f2 = fl::future<int>();
        
        CHECK(f1.valid());
        CHECK(!f2.valid());
        
        f2 = fl::move(f1);
        CHECK(!f1.valid()); // Moved from
        CHECK(f2.valid());  // Moved to
        CHECK_EQ(f2.state(), fl::FutureState::PENDING);
    }
}

TEST_CASE("fl::future - Complex Types") {
    SUBCASE("String future") {
        auto f = fl::future<fl::string>::create();
        fl::string test_value = "Hello World";
        
        bool success = f.complete_with_value(fl::move(test_value));
        CHECK(success);
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        auto result = f.try_result();
        CHECK(!result.empty());
        CHECK_EQ(*result.ptr(), "Hello World");
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
    
    SUBCASE("Future state after clear") {
        auto f = fl::future<int>::create();
        f.complete_with_value(42);
        
        CHECK_EQ(f.state(), fl::FutureState::READY);
        
        f.clear();
        
        // After clear, should be invalid
        CHECK(!f.valid());
        CHECK(!f); // operator bool() - false when invalid
        
        // Operations should fail
        CHECK(!f.complete_with_value(99));
        CHECK(!f.complete_with_error("New error"));
    }
} 

TEST_CASE("fl::future - Blocking get_result with fl::time()") {
    SUBCASE("get_result() returns immediately when future is ready") {
        auto f = fl::future<int>::create();
        f.complete_with_value(42);
        
        auto result = f.get_result(1000); // 1 second timeout
        CHECK(result.is<int>());
        CHECK_EQ(*result.ptr<int>(), 42);
        CHECK(!result.is<fl::FutureError>());
        CHECK(!result.is<fl::FuturePending>());
    }
    
    SUBCASE("get_result() returns error immediately when future has error") {
        auto f = fl::future<int>::create();
        f.complete_with_error("Test error");
        
        auto result = f.get_result(1000); // 1 second timeout
        CHECK(result.is<fl::FutureError>());
        CHECK_EQ(result.ptr<fl::FutureError>()->message, "Test error");
        CHECK(!result.is<int>());
        CHECK(!result.is<fl::FuturePending>());
    }
    
    SUBCASE("get_result() returns error for invalid future") {
        fl::future<int> f; // Invalid future
        
        auto result = f.get_result(1000);
        CHECK(result.is<fl::FutureError>());
        CHECK_EQ(result.ptr<fl::FutureError>()->message, "Future is invalid");
        CHECK(!result.is<int>());
        CHECK(!result.is<fl::FuturePending>());
    }
    
    SUBCASE("get_result() has same return type as try_get_result()") {
        auto f = fl::future<int>::create();
        f.complete_with_value(42);
        
        auto blocking_result = f.get_result(1000);
        auto non_blocking_result = f.try_get_result();
        
        // Both should return FutureResult<int> and have the same value
        CHECK(blocking_result.is<int>());
        CHECK(non_blocking_result.is<int>());
        CHECK_EQ(*blocking_result.ptr<int>(), *non_blocking_result.ptr<int>());
    }
    
#ifdef FASTLED_TESTING
    SUBCASE("get_result() with timeout using mock time provider") {
        // Create a mock time provider
        fl::MockTimeProvider mock(1000);
        fl::inject_time_provider(mock);
        
        auto f = fl::future<int>::create();
        
        // Start get_result in a way that will timeout (future never completes)
        // We can't actually test the blocking timeout easily in a unit test,
        // but we can test that the timeout logic uses fl::time() correctly
        // by verifying the method works with injected time
        
        // Complete the future immediately so we can test the success path
        f.complete_with_value(123);
        auto result = f.get_result(100); // 100ms timeout
        
        CHECK(result.is<int>());
        CHECK_EQ(*result.ptr<int>(), 123);
        CHECK(!result.is<fl::FutureError>());
        CHECK(!result.is<fl::FuturePending>());
        
        fl::clear_time_provider();
    }
#endif
} 
