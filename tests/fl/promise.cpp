#include "fl/promise.h"
#include "fl/promise_result.h"
#include "fl/unused.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"
#include "fl/stl/variant.h"


FL_TEST_CASE("fl::promise - Basic Operations") {
    FL_SUBCASE("Default constructor creates invalid promise") {
        fl::promise<int> p;
        FL_CHECK(!p.valid());
        FL_CHECK(!p.is_completed());
        FL_CHECK(!p.is_resolved());
        FL_CHECK(!p.is_rejected());
    }
    
    FL_SUBCASE("Static create() creates valid, pending promise") {
        auto p = fl::promise<int>::create();
        FL_CHECK(p.valid());
        FL_CHECK(!p.is_completed());
        FL_CHECK(!p.is_resolved());
        FL_CHECK(!p.is_rejected());
    }
    
    FL_SUBCASE("Clear makes promise invalid") {
        auto p = fl::promise<int>::create();
        FL_CHECK(p.valid());
        
        p.clear();
        FL_CHECK(!p.valid());
    }
}

FL_TEST_CASE("fl::promise - Static Factory Methods") {
    FL_SUBCASE("resolve() creates resolved promise") {
        auto p = fl::promise<int>::resolve(42);
        FL_CHECK(p.valid());
        FL_CHECK(p.is_completed());
        FL_CHECK(p.is_resolved());
        FL_CHECK(!p.is_rejected());
        FL_CHECK_EQ(p.value(), 42);
    }
    
    FL_SUBCASE("resolve() with move semantics") {
        fl::string test_str = "test string";
        auto p = fl::promise<fl::string>::resolve(fl::move(test_str));
        FL_CHECK(p.valid());
        FL_CHECK(p.is_completed());
        FL_CHECK(p.is_resolved());
        FL_CHECK_EQ(p.value(), "test string");
    }
    
    FL_SUBCASE("reject() creates rejected promise") {
        auto p = fl::promise<int>::reject(fl::Error("Test error"));
        FL_CHECK(p.valid());
        FL_CHECK(p.is_completed());
        FL_CHECK(!p.is_resolved());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "Test error");
    }
    
    FL_SUBCASE("reject() with Error object") {
        fl::Error err("Custom error");
        auto p = fl::promise<int>::reject(err);
        FL_CHECK(p.valid());
        FL_CHECK(p.is_completed());
        FL_CHECK(!p.is_resolved());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "Custom error");
    }
}

FL_TEST_CASE("fl::promise - Producer Interface") {
    FL_SUBCASE("complete_with_value() resolves promise") {
        auto p = fl::promise<int>::create();
        FL_CHECK(!p.is_completed());
        
        bool success = p.complete_with_value(123);
        FL_CHECK(success);
        FL_CHECK(p.is_completed());
        FL_CHECK(p.is_resolved());
        FL_CHECK(!p.is_rejected());
        FL_CHECK_EQ(p.value(), 123);
    }
    
    FL_SUBCASE("complete_with_value() with move semantics") {
        auto p = fl::promise<fl::string>::create();
        fl::string test_str = "moved string";
        
        bool success = p.complete_with_value(fl::move(test_str));
        FL_CHECK(success);
        FL_CHECK(p.is_completed());
        FL_CHECK(p.is_resolved());
        FL_CHECK_EQ(p.value(), "moved string");
    }
    
    FL_SUBCASE("complete_with_error() rejects promise") {
        auto p = fl::promise<int>::create();
        FL_CHECK(!p.is_completed());
        
        bool success = p.complete_with_error(fl::Error("Test error"));
        FL_CHECK(success);
        FL_CHECK(p.is_completed());
        FL_CHECK(!p.is_resolved());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "Test error");
    }
    
    FL_SUBCASE("complete_with_error() with Error object") {
        auto p = fl::promise<int>::create();
        fl::Error err("Custom error");
        
        bool success = p.complete_with_error(err);
        FL_CHECK(success);
        FL_CHECK(p.is_completed());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "Custom error");
    }
    
    FL_SUBCASE("Cannot complete promise twice") {
        auto p = fl::promise<int>::create();
        
        // First completion should succeed
        bool first = p.complete_with_value(42);
        FL_CHECK(first);
        FL_CHECK(p.is_resolved());
        FL_CHECK_EQ(p.value(), 42);
        
        // Second completion should fail
        bool second = p.complete_with_value(99);
        FL_CHECK(!second);
        FL_CHECK_EQ(p.value(), 42); // Value unchanged
        
        // Trying to complete with error should also fail
        bool third = p.complete_with_error(fl::Error("Should not work"));
        FL_CHECK(!third);
        FL_CHECK(p.is_resolved()); // Still resolved, not rejected
    }
}

FL_TEST_CASE("fl::promise - Callback Interface") {
    FL_SUBCASE("then() callback called immediately on resolved promise") {
        bool callback_called = false;
        int received_value = 0;
        
        auto p = fl::promise<int>::resolve(42);
        p.then([&](const int& value) {
            callback_called = true;
            received_value = value;
        });
        
        FL_CHECK(callback_called);
        FL_CHECK_EQ(received_value, 42);
    }
    
    FL_SUBCASE("then() callback called after resolution") {
        bool callback_called = false;
        int received_value = 0;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) {
            callback_called = true;
            received_value = value;
        });
        
        FL_CHECK(!callback_called); // Should not be called yet
        
        p.complete_with_value(123);
        FL_CHECK(callback_called);
        FL_CHECK_EQ(received_value, 123);
    }
    
    FL_SUBCASE("catch_() callback called immediately on rejected promise") {
        bool callback_called = false;
        fl::string received_error;
        
        auto p = fl::promise<int>::reject(fl::Error("Test error"));
        p.catch_([&](const fl::Error& err) {
            callback_called = true;
            received_error = err.message;
        });
        
        FL_CHECK(callback_called);
        FL_CHECK_EQ(received_error, "Test error");
    }
    
    FL_SUBCASE("catch_() callback called after rejection") {
        bool callback_called = false;
        fl::string received_error;
        
        auto p = fl::promise<int>::create();
        p.catch_([&](const fl::Error& err) {
            callback_called = true;
            received_error = err.message;
        });
        
        FL_CHECK(!callback_called); // Should not be called yet
        
        p.complete_with_error(fl::Error("Async error"));
        FL_CHECK(callback_called);
        FL_CHECK_EQ(received_error, "Async error");
    }
    
    FL_SUBCASE("then() returns reference for chaining") {
        auto p = fl::promise<int>::create();
        
        // Should be able to chain
        auto& ref = p.then([](const int& value) {
            FL_UNUSED(value);
            // Success callback
        }).catch_([](const fl::Error& err) {
            FL_UNUSED(err);
            // Error callback  
        });
        
        // Reference should be the same promise
        FL_CHECK(&ref == &p);
    }
    
    FL_SUBCASE("catch_() returns reference for chaining") {
        auto p = fl::promise<int>::create();
        
        // Should be able to chain
        auto& ref = p.catch_([](const fl::Error& err) {
            FL_UNUSED(err);
            // Error callback
        }).then([](const int& value) {
            FL_UNUSED(value);
            // Success callback
        });
        
        // Reference should be the same promise  
        FL_CHECK(&ref == &p);
    }
}

FL_TEST_CASE("fl::promise - Update and Callback Processing") {
    FL_SUBCASE("update() processes callbacks after manual completion") {
        bool then_called = false;
        bool catch_called = false;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) { FL_UNUSED(value); then_called = true; });
        p.catch_([&](const fl::Error& err) { FL_UNUSED(err); catch_called = true; });
        
        // Complete and then update
        p.complete_with_value(42);
        p.update();
        
        FL_CHECK(then_called);
        FL_CHECK(!catch_called);
    }
    
    FL_SUBCASE("update() on invalid promise does nothing") {
        fl::promise<int> invalid_promise;
        // Should not crash
        invalid_promise.update();
        FL_CHECK(!invalid_promise.valid());
    }
    
    FL_SUBCASE("Callbacks only called once") {
        int call_count = 0;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) { FL_UNUSED(value); call_count++; });
        
        p.complete_with_value(42);
        FL_CHECK_EQ(call_count, 1);
        
        // Multiple updates should not call callback again
        p.update();
        p.update();
        FL_CHECK_EQ(call_count, 1);
    }
}

FL_TEST_CASE("fl::promise - Copy Semantics") {
    FL_SUBCASE("Promises are copyable") {
        auto p1 = fl::promise<int>::create();
        auto p2 = p1; // Copy constructor
        
        FL_CHECK(p1.valid());
        FL_CHECK(p2.valid());
        
        // Both should refer to the same promise
        p1.complete_with_value(42);
        FL_CHECK(p1.is_resolved());
        FL_CHECK(p2.is_resolved());
        FL_CHECK_EQ(p1.value(), 42);
        FL_CHECK_EQ(p2.value(), 42);
    }
    
    FL_SUBCASE("Copy assignment works") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::promise<int>::create();
        
        p2 = p1; // Copy assignment
        
        FL_CHECK(p1.valid());
        FL_CHECK(p2.valid());
        
        // Both should refer to the same promise
        p1.complete_with_value(123);
        FL_CHECK(p1.is_resolved());
        FL_CHECK(p2.is_resolved());
        FL_CHECK_EQ(p1.value(), 123);
        FL_CHECK_EQ(p2.value(), 123);
    }
    
    FL_SUBCASE("Callbacks work on copied promises") {
        bool callback1_called = false;
        bool callback2_called = false;
        
        auto p1 = fl::promise<int>::create();
        auto p2 = p1; // Copy
        
        p1.then([&](const int& value) {
            FL_UNUSED(value);   
            callback1_called = true;
        });
        p2.then([&](const int& value) {
            FL_UNUSED(value);
            callback2_called = true;
        });
        
        p1.complete_with_value(42);
        
        // NOTE: Current implementation only stores one callback per promise
        // The second then() call overwrites the first callback
        // Only the last callback set will be called
        FL_CHECK(!callback1_called);  // First callback was overwritten
        FL_CHECK(callback2_called);   // Second callback is called
    }
}

FL_TEST_CASE("fl::promise - Move Semantics") {
    FL_SUBCASE("Promises are moveable") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::move(p1); // Move constructor
        
        FL_CHECK(!p1.valid()); // p1 should be invalid after move
        FL_CHECK(p2.valid());  // p2 should be valid
        
        p2.complete_with_value(42);
        FL_CHECK(p2.is_resolved());
        FL_CHECK_EQ(p2.value(), 42);
    }
    
    FL_SUBCASE("Move assignment works") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::promise<int>::create();
        
        p2 = fl::move(p1); // Move assignment
        
        FL_CHECK(!p1.valid()); // p1 should be invalid after move
        FL_CHECK(p2.valid());  // p2 should be valid
        
        p2.complete_with_value(123);
        FL_CHECK(p2.is_resolved());
        FL_CHECK_EQ(p2.value(), 123);
    }
}

FL_TEST_CASE("fl::promise - Convenience Functions") {
    FL_SUBCASE("make_resolved_promise() works") {
        auto p = fl::make_resolved_promise(42);
        FL_CHECK(p.valid());
        FL_CHECK(p.is_resolved());
        FL_CHECK_EQ(p.value(), 42);
    }
    
    FL_SUBCASE("make_rejected_promise() with string works") {
        auto p = fl::make_rejected_promise<int>("Test error");
        FL_CHECK(p.valid());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "Test error");
    }
    
    FL_SUBCASE("make_rejected_promise() with const char* works") {
        auto p = fl::make_rejected_promise<int>("C string error");
        FL_CHECK(p.valid());
        FL_CHECK(p.is_rejected());
        FL_CHECK_EQ(p.error().message, "C string error");
    }
}

FL_TEST_CASE("fl::promise - Error Type") {
    FL_SUBCASE("Error default constructor") {
        fl::Error err;
        FL_CHECK(err.message.empty());
    }
    
    FL_SUBCASE("Error with string") {
        fl::string msg = "Test message";
        fl::Error err(msg);
        FL_CHECK_EQ(err.message, "Test message");
    }
    
    FL_SUBCASE("Error with const char*") {
        fl::Error err("C string message");
        FL_CHECK_EQ(err.message, "C string message");
    }
    
    FL_SUBCASE("Error with move string") {
        fl::string msg = "Move message";
        fl::Error err(fl::move(msg));
        FL_CHECK_EQ(err.message, "Move message");
    }
}

FL_TEST_CASE("fl::promise - Edge Cases") {
    FL_SUBCASE("Invalid promise methods return safe defaults") {
        fl::promise<int> invalid;
        
        FL_CHECK(!invalid.valid());
        FL_CHECK(!invalid.is_completed());
        FL_CHECK(!invalid.is_resolved());
        FL_CHECK(!invalid.is_rejected());
        
        // Should return default-constructed values for invalid promise
        FL_CHECK_EQ(invalid.value(), int{});
        FL_CHECK_EQ(invalid.error().message, fl::string{});
        
        // Methods should safely handle invalid state
        FL_CHECK(!invalid.complete_with_value(42));
        FL_CHECK(!invalid.complete_with_error(fl::Error("error")));
        
        // Chaining should return reference even for invalid promise
        auto& ref = invalid.then([](const int&) {}).catch_([](const fl::Error&) {});
        FL_CHECK(&ref == &invalid);
    }
    
    FL_SUBCASE("Multiple callbacks on same promise") {
        bool callback1_called = false;
        bool callback2_called = false;
        int value1 = 0, value2 = 0;
        
        auto p = fl::promise<int>::create();
        
        // Add multiple then callbacks
        p.then([&](const int& value) {
            callback1_called = true;
            value1 = value;
        });
        
        p.then([&](const int& value) {
            callback2_called = true;
            value2 = value;
        });
        
        p.complete_with_value(42);
        
        // Only the last callback is stored and called
        // (This is a design limitation of the lightweight implementation)
        FL_CHECK(!callback1_called);  // First callback was overwritten
        FL_CHECK(callback2_called);   // Only the last callback is called
        FL_CHECK_EQ(value2, 42);
    }
}

FL_TEST_CASE("fl::promise - Complex Types") {
    FL_SUBCASE("Promise with string type") {
        auto p = fl::promise<fl::string>::create();
        bool callback_called = false;
        fl::string received;
        
        p.then([&](const fl::string& value) {
            callback_called = true;
            received = value;
        });
        
        p.complete_with_value(fl::string("test string"));
        
        FL_CHECK(callback_called);
        FL_CHECK_EQ(received, "test string");
    }
    
    FL_SUBCASE("Promise with custom struct") {
        struct TestData {
            int x;
            fl::string name;
            
            bool operator==(const TestData& other) const {
                return x == other.x && name == other.name;
            }
        };
        
        auto p = fl::promise<TestData>::create();
        bool callback_called = false;
        TestData received{};
        
        p.then([&](const TestData& value) {
            callback_called = true;
            received = value;
        });
        
        TestData test_data{42, "test"};
        p.complete_with_value(test_data);
        
        FL_CHECK(callback_called);
        FL_CHECK(received == test_data);
    }
}

FL_TEST_CASE("fl::PromiseResult - Basic Construction") {
    FL_SUBCASE("construct with success value") {
        fl::PromiseResult<int> result(42);
        
        FL_CHECK(result.ok());
        FL_CHECK(static_cast<bool>(result));
        FL_CHECK_EQ(result.value(), 42);
        FL_CHECK_EQ(result.error_message(), "");
    }
    
    FL_SUBCASE("construct with error") {
        fl::Error err("Test error");
        fl::PromiseResult<int> result(err);
        
        FL_CHECK(!result.ok());
        FL_CHECK(!static_cast<bool>(result));
        FL_CHECK_EQ(result.error().message, "Test error");
        FL_CHECK_EQ(result.error_message(), "Test error");
    }
    
    FL_SUBCASE("construct with move semantics") {
        fl::string text = "Hello World";
        fl::PromiseResult<fl::string> result(fl::move(text));
        
        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), "Hello World");
    }
}

FL_TEST_CASE("fl::PromiseResult - Value Access") {
    FL_SUBCASE("safe value access on success") {
        fl::PromiseResult<int> result(100);
        
        FL_CHECK(result.ok());
        
        // Test const access
        const fl::PromiseResult<int>& const_result = result;
        const int& const_value = const_result.value();
        FL_CHECK_EQ(const_value, 100);
        
        // Test mutable access
        int& mutable_value = result.value();
        FL_CHECK_EQ(mutable_value, 100);
        
        // Test modification
        mutable_value = 200;
        FL_CHECK_EQ(result.value(), 200);
    }
    
    FL_SUBCASE("value access on error in release builds") {
        fl::PromiseResult<int> result(fl::Error("Test error"));
        
        FL_CHECK(!result.ok());
        
        // In release builds, this should return a static empty object
        // In debug builds, this would crash (which we can't test automatically)
        #ifndef DEBUG
            // Only test this in release builds to avoid crashes
            const int& value = result.value();
            // Should return a default-constructed int (0)
            FL_CHECK_EQ(value, 0);
        #endif
    }
    
    FL_SUBCASE("string value access") {
        fl::PromiseResult<fl::string> result(fl::string("Test"));
        
        FL_CHECK(result.ok());
        FL_CHECK_EQ(result.value(), "Test");
        
        // Modify string
        result.value() = "Modified";
        FL_CHECK_EQ(result.value(), "Modified");
    }
}

FL_TEST_CASE("fl::PromiseResult - Error Access") {
    FL_SUBCASE("safe error access on error") {
        fl::Error original_error("Network timeout");
        fl::PromiseResult<int> result(original_error);
        
        FL_CHECK(!result.ok());
        
        const fl::Error& error = result.error();
        FL_CHECK_EQ(error.message, "Network timeout");
    }
    
    FL_SUBCASE("error access on success in release builds") {
        fl::PromiseResult<int> result(42);
        
        FL_CHECK(result.ok());
        
        // In release builds, this should return a static descriptive error
        // In debug builds, this would crash (which we can't test automatically)
        #ifndef DEBUG
            const fl::Error& error = result.error();
            // Should return a descriptive error message
            FL_CHECK(error.message.find("success value") != fl::string::npos);
        #endif
    }
    
    FL_SUBCASE("error_message convenience method") {
        // Test with error
        fl::PromiseResult<int> error_result(fl::Error("Connection failed"));
        FL_CHECK_EQ(error_result.error_message(), "Connection failed");
        
        // Test with success
        fl::PromiseResult<int> success_result(42);
        FL_CHECK_EQ(success_result.error_message(), "");
    }
}

FL_TEST_CASE("fl::PromiseResult - Type Conversions") {
    FL_SUBCASE("boolean conversion") {
        fl::PromiseResult<int> success(42);
        fl::PromiseResult<int> failure(fl::Error("Error"));
        
        // Test explicit bool conversion
        FL_CHECK(static_cast<bool>(success));
        FL_CHECK(!static_cast<bool>(failure));
        
        // Test in if statements
        if (success) {
            FL_CHECK(true); // Should reach here
        } else {
            FL_CHECK(false); // Should not reach here
        }
        
        if (failure) {
            FL_CHECK(false); // Should not reach here
        } else {
            FL_CHECK(true); // Should reach here
        }
    }
    
    FL_SUBCASE("variant access") {
        fl::PromiseResult<int> result(42);
        
        const auto& variant = result.variant();
        FL_CHECK(variant.is<int>());
        FL_CHECK_EQ(variant.get<int>(), 42);
    }
}

FL_TEST_CASE("fl::PromiseResult - Helper Functions") {
    FL_SUBCASE("make_success") {
        auto result1 = fl::make_success(42);
        FL_CHECK(result1.ok());
        FL_CHECK_EQ(result1.value(), 42);
        
        fl::string text = "Hello";
        auto result2 = fl::make_success(fl::move(text));
        FL_CHECK(result2.ok());
        FL_CHECK_EQ(result2.value(), "Hello");
    }
    
    FL_SUBCASE("make_error with Error object") {
        fl::Error err("Custom error");
        auto result = fl::make_error<int>(err);
        
        FL_CHECK(!result.ok());
        FL_CHECK_EQ(result.error().message, "Custom error");
    }
    
    FL_SUBCASE("make_error with string") {
        auto result1 = fl::make_error<int>(fl::string("String error"));
        FL_CHECK(!result1.ok());
        FL_CHECK_EQ(result1.error().message, "String error");
        
        auto result2 = fl::make_error<int>("C-string error");
        FL_CHECK(!result2.ok());
        FL_CHECK_EQ(result2.error().message, "C-string error");
    }
}

FL_TEST_CASE("fl::PromiseResult - Complex Types") {
    FL_SUBCASE("custom struct") {
        struct TestStruct {
            int x;
            fl::string name;
            
            TestStruct() : x(0), name("") {}
            TestStruct(int x_, const fl::string& name_) : x(x_), name(name_) {}
            
            bool operator==(const TestStruct& other) const {
                return x == other.x && name == other.name;
            }
        };
        
        TestStruct original{42, "test"};
        fl::PromiseResult<TestStruct> result(original);
        
        FL_CHECK(result.ok());
        
        const TestStruct& retrieved = result.value();
        FL_CHECK(retrieved == original);
        FL_CHECK_EQ(retrieved.x, 42);
        FL_CHECK_EQ(retrieved.name, "test");
        
        // Test modification
        TestStruct& mutable_struct = result.value();
        mutable_struct.x = 99;
        FL_CHECK_EQ(result.value().x, 99);
    }
}

FL_TEST_CASE("fl::PromiseResult - Copy and Move Semantics") {
    FL_SUBCASE("copy construction") {
        fl::PromiseResult<int> original(42);
        fl::PromiseResult<int> copy(original);
        
        FL_CHECK(copy.ok());
        FL_CHECK_EQ(copy.value(), 42);
        
        // Modify copy, original should be unchanged
        copy.value() = 100;
        FL_CHECK_EQ(original.value(), 42);
        FL_CHECK_EQ(copy.value(), 100);
    }
    
    FL_SUBCASE("copy assignment") {
        fl::PromiseResult<int> original(42);
        fl::PromiseResult<int> copy = fl::make_error<int>("temp");
        
        copy = original;
        
        FL_CHECK(copy.ok());
        FL_CHECK_EQ(copy.value(), 42);
    }
    
    FL_SUBCASE("move construction") {
        fl::string text = "Move me";
        fl::PromiseResult<fl::string> original(fl::move(text));
        fl::PromiseResult<fl::string> moved(fl::move(original));
        
        FL_CHECK(moved.ok());
        FL_CHECK_EQ(moved.value(), "Move me");
    }
} 
