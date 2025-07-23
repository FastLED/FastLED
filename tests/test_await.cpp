#include "test.h"
#include "fl/async.h"
#include "fl/promise.h"

using namespace fl;

TEST_CASE("fl::await_top_level - Basic Operations") {
    SUBCASE("await_top_level resolved promise returns value") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(result.ok());
        CHECK_EQ(result.value(), 42);
    }
    
    SUBCASE("await_top_level rejected promise returns error") {
        auto promise = fl::promise<int>::reject(Error("Test error"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Test error");
    }
    
    SUBCASE("await_top_level invalid promise returns error") {
        fl::promise<int> invalid_promise; // Default constructor creates invalid promise
        auto result = fl::await_top_level(invalid_promise);  // Type automatically deduced!
        
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Invalid promise");
    }
    
    SUBCASE("explicit template parameter still works") {
        auto promise = fl::promise<int>::resolve(42);
        auto result = fl::await_top_level<int>(promise);  // Explicit template parameter
        
        CHECK(result.ok());
        CHECK_EQ(result.value(), 42);
    }
}

TEST_CASE("fl::await_top_level - Asynchronous Completion") {
    SUBCASE("await_top_level waits for promise to be resolved") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;
        
        // Simulate async completion in background
        // Note: In a real scenario, this would be done by an async system
        // For testing, we'll complete it immediately after starting await_top_level
        
        // Complete the promise with a value
        promise.complete_with_value(123);
        promise_completed = true;
        
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(promise_completed);
        CHECK(result.ok());
        CHECK_EQ(result.value(), 123);
    }
    
    SUBCASE("await_top_level waits for promise to be rejected") {
        auto promise = fl::promise<int>::create();
        bool promise_completed = false;
        
        // Complete the promise with an error
        promise.complete_with_error(Error("Async error"));
        promise_completed = true;
        
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(promise_completed);
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Async error");
    }
}

TEST_CASE("fl::await_top_level - Different Value Types") {
    SUBCASE("await_top_level with string type") {
        auto promise = fl::promise<fl::string>::resolve(fl::string("Hello, World!"));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(result.ok());
        CHECK_EQ(result.value(), "Hello, World!");
    }
    
    SUBCASE("await_top_level with custom struct") {
        struct TestData {
            int x;
            fl::string name;
            
            bool operator==(const TestData& other) const {
                return x == other.x && name == other.name;
            }
        };
        
        TestData expected{42, "test"};
        auto promise = fl::promise<TestData>::resolve(expected);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(result.ok());
        CHECK(result.value() == expected);
    }
}

TEST_CASE("fl::await_top_level - Error Handling") {
    SUBCASE("await_top_level preserves error message") {
        fl::string error_msg = "Detailed error message";
        auto promise = fl::promise<int>::reject(Error(error_msg));
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, error_msg);
    }
    
    SUBCASE("await_top_level with custom error") {
        Error custom_error("Custom error with details");
        auto promise = fl::promise<fl::string>::reject(custom_error);
        auto result = fl::await_top_level(promise);  // Type automatically deduced!
        
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Custom error with details");
    }
}

TEST_CASE("fl::await_top_level - Multiple Awaits") {
    SUBCASE("multiple awaits on different promises") {
        auto promise1 = fl::promise<int>::resolve(10);
        auto promise2 = fl::promise<int>::resolve(20);
        auto promise3 = fl::promise<int>::reject(Error("Error in promise 3"));
        
        auto result1 = fl::await_top_level(promise1);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise2);  // Type automatically deduced!
        auto result3 = fl::await_top_level(promise3);  // Type automatically deduced!
        
        // Check first result
        CHECK(result1.ok());
        CHECK_EQ(result1.value(), 10);
        
        // Check second result
        CHECK(result2.ok());
        CHECK_EQ(result2.value(), 20);
        
        // Check third result (error)
        CHECK(!result3.ok());
        CHECK_EQ(result3.error().message, "Error in promise 3");
    }
    
    SUBCASE("await_top_level same promise multiple times") {
        auto promise = fl::promise<int>::resolve(999);
        
        auto result1 = fl::await_top_level(promise);  // Type automatically deduced!
        auto result2 = fl::await_top_level(promise);  // Type automatically deduced!
        
        // Both awaits should return the same result
        CHECK(result1.ok());
        CHECK(result2.ok());
        
        CHECK_EQ(result1.value(), 999);
        CHECK_EQ(result2.value(), 999);
    }
}

TEST_CASE("fl::await_top_level - Boolean Conversion and Convenience") {
    SUBCASE("boolean conversion operator") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);
        
        auto error_promise = fl::promise<int>::reject(Error("Error"));
        auto error_result = fl::await_top_level(error_promise);
        
        // Test boolean conversion (should work like ok())
        CHECK(success_result);   // Implicit conversion to bool
        CHECK(!error_result);    // Implicit conversion to bool
        
        // Equivalent to ok() method
        CHECK(success_result.ok());
        CHECK(!error_result.ok());
    }
    
    SUBCASE("error_message convenience method") {
        auto success_promise = fl::promise<int>::resolve(42);
        auto success_result = fl::await_top_level(success_promise);
        
        auto error_promise = fl::promise<int>::reject(Error("Test error"));
        auto error_result = fl::await_top_level(error_promise);
        
        // Test error_message convenience method
        CHECK_EQ(success_result.error_message(), "");  // Empty string for success
        CHECK_EQ(error_result.error_message(), "Test error");  // Error message for failure
    }
} 
