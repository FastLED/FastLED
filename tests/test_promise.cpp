#include "test.h"
#include "fl/promise.h"
#include "fl/promise_result.h"
#include "fl/unused.h"

using namespace fl;

TEST_CASE("fl::promise - Basic Operations") {
    SUBCASE("Default constructor creates invalid promise") {
        fl::promise<int> p;
        CHECK(!p.valid());
        CHECK(!p.is_completed());
        CHECK(!p.is_resolved());
        CHECK(!p.is_rejected());
    }
    
    SUBCASE("Static create() creates valid, pending promise") {
        auto p = fl::promise<int>::create();
        CHECK(p.valid());
        CHECK(!p.is_completed());
        CHECK(!p.is_resolved());
        CHECK(!p.is_rejected());
    }
    
    SUBCASE("Clear makes promise invalid") {
        auto p = fl::promise<int>::create();
        CHECK(p.valid());
        
        p.clear();
        CHECK(!p.valid());
    }
}

TEST_CASE("fl::promise - Static Factory Methods") {
    SUBCASE("resolve() creates resolved promise") {
        auto p = fl::promise<int>::resolve(42);
        CHECK(p.valid());
        CHECK(p.is_completed());
        CHECK(p.is_resolved());
        CHECK(!p.is_rejected());
        CHECK_EQ(p.value(), 42);
    }
    
    SUBCASE("resolve() with move semantics") {
        fl::string test_str = "test string";
        auto p = fl::promise<fl::string>::resolve(fl::move(test_str));
        CHECK(p.valid());
        CHECK(p.is_completed());
        CHECK(p.is_resolved());
        CHECK_EQ(p.value(), "test string");
    }
    
    SUBCASE("reject() creates rejected promise") {
        auto p = fl::promise<int>::reject(Error("Test error"));
        CHECK(p.valid());
        CHECK(p.is_completed());
        CHECK(!p.is_resolved());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "Test error");
    }
    
    SUBCASE("reject() with Error object") {
        Error err("Custom error");
        auto p = fl::promise<int>::reject(err);
        CHECK(p.valid());
        CHECK(p.is_completed());
        CHECK(!p.is_resolved());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "Custom error");
    }
}

TEST_CASE("fl::promise - Producer Interface") {
    SUBCASE("complete_with_value() resolves promise") {
        auto p = fl::promise<int>::create();
        CHECK(!p.is_completed());
        
        bool success = p.complete_with_value(123);
        CHECK(success);
        CHECK(p.is_completed());
        CHECK(p.is_resolved());
        CHECK(!p.is_rejected());
        CHECK_EQ(p.value(), 123);
    }
    
    SUBCASE("complete_with_value() with move semantics") {
        auto p = fl::promise<fl::string>::create();
        fl::string test_str = "moved string";
        
        bool success = p.complete_with_value(fl::move(test_str));
        CHECK(success);
        CHECK(p.is_completed());
        CHECK(p.is_resolved());
        CHECK_EQ(p.value(), "moved string");
    }
    
    SUBCASE("complete_with_error() rejects promise") {
        auto p = fl::promise<int>::create();
        CHECK(!p.is_completed());
        
        bool success = p.complete_with_error(Error("Test error"));
        CHECK(success);
        CHECK(p.is_completed());
        CHECK(!p.is_resolved());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "Test error");
    }
    
    SUBCASE("complete_with_error() with Error object") {
        auto p = fl::promise<int>::create();
        Error err("Custom error");
        
        bool success = p.complete_with_error(err);
        CHECK(success);
        CHECK(p.is_completed());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "Custom error");
    }
    
    SUBCASE("Cannot complete promise twice") {
        auto p = fl::promise<int>::create();
        
        // First completion should succeed
        bool first = p.complete_with_value(42);
        CHECK(first);
        CHECK(p.is_resolved());
        CHECK_EQ(p.value(), 42);
        
        // Second completion should fail
        bool second = p.complete_with_value(99);
        CHECK(!second);
        CHECK_EQ(p.value(), 42); // Value unchanged
        
        // Trying to complete with error should also fail
        bool third = p.complete_with_error(Error("Should not work"));
        CHECK(!third);
        CHECK(p.is_resolved()); // Still resolved, not rejected
    }
}

TEST_CASE("fl::promise - Callback Interface") {
    SUBCASE("then() callback called immediately on resolved promise") {
        bool callback_called = false;
        int received_value = 0;
        
        auto p = fl::promise<int>::resolve(42);
        p.then([&](const int& value) {
            callback_called = true;
            received_value = value;
        });
        
        CHECK(callback_called);
        CHECK_EQ(received_value, 42);
    }
    
    SUBCASE("then() callback called after resolution") {
        bool callback_called = false;
        int received_value = 0;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) {
            callback_called = true;
            received_value = value;
        });
        
        CHECK(!callback_called); // Should not be called yet
        
        p.complete_with_value(123);
        CHECK(callback_called);
        CHECK_EQ(received_value, 123);
    }
    
    SUBCASE("catch_() callback called immediately on rejected promise") {
        bool callback_called = false;
        fl::string received_error;
        
        auto p = fl::promise<int>::reject(Error("Test error"));
        p.catch_([&](const Error& err) {
            callback_called = true;
            received_error = err.message;
        });
        
        CHECK(callback_called);
        CHECK_EQ(received_error, "Test error");
    }
    
    SUBCASE("catch_() callback called after rejection") {
        bool callback_called = false;
        fl::string received_error;
        
        auto p = fl::promise<int>::create();
        p.catch_([&](const Error& err) {
            callback_called = true;
            received_error = err.message;
        });
        
        CHECK(!callback_called); // Should not be called yet
        
        p.complete_with_error(Error("Async error"));
        CHECK(callback_called);
        CHECK_EQ(received_error, "Async error");
    }
    
    SUBCASE("then() returns reference for chaining") {
        auto p = fl::promise<int>::create();
        
        // Should be able to chain
        auto& ref = p.then([](const int& value) {
            FL_UNUSED(value);
            // Success callback
        }).catch_([](const Error& err) {
            FL_UNUSED(err);
            // Error callback  
        });
        
        // Reference should be the same promise
        CHECK(&ref == &p);
    }
    
    SUBCASE("catch_() returns reference for chaining") {
        auto p = fl::promise<int>::create();
        
        // Should be able to chain
        auto& ref = p.catch_([](const Error& err) {
            FL_UNUSED(err);
            // Error callback
        }).then([](const int& value) {
            FL_UNUSED(value);
            // Success callback
        });
        
        // Reference should be the same promise  
        CHECK(&ref == &p);
    }
}

TEST_CASE("fl::promise - Update and Callback Processing") {
    SUBCASE("update() processes callbacks after manual completion") {
        bool then_called = false;
        bool catch_called = false;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) { FL_UNUSED(value); then_called = true; });
        p.catch_([&](const Error& err) { FL_UNUSED(err); catch_called = true; });
        
        // Complete and then update
        p.complete_with_value(42);
        p.update();
        
        CHECK(then_called);
        CHECK(!catch_called);
    }
    
    SUBCASE("update() on invalid promise does nothing") {
        fl::promise<int> invalid_promise;
        // Should not crash
        invalid_promise.update();
        CHECK(!invalid_promise.valid());
    }
    
    SUBCASE("Callbacks only called once") {
        int call_count = 0;
        
        auto p = fl::promise<int>::create();
        p.then([&](const int& value) { FL_UNUSED(value); call_count++; });
        
        p.complete_with_value(42);
        CHECK_EQ(call_count, 1);
        
        // Multiple updates should not call callback again
        p.update();
        p.update();
        CHECK_EQ(call_count, 1);
    }
}

TEST_CASE("fl::promise - Copy Semantics") {
    SUBCASE("Promises are copyable") {
        auto p1 = fl::promise<int>::create();
        auto p2 = p1; // Copy constructor
        
        CHECK(p1.valid());
        CHECK(p2.valid());
        
        // Both should refer to the same promise
        p1.complete_with_value(42);
        CHECK(p1.is_resolved());
        CHECK(p2.is_resolved());
        CHECK_EQ(p1.value(), 42);
        CHECK_EQ(p2.value(), 42);
    }
    
    SUBCASE("Copy assignment works") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::promise<int>::create();
        
        p2 = p1; // Copy assignment
        
        CHECK(p1.valid());
        CHECK(p2.valid());
        
        // Both should refer to the same promise
        p1.complete_with_value(123);
        CHECK(p1.is_resolved());
        CHECK(p2.is_resolved());
        CHECK_EQ(p1.value(), 123);
        CHECK_EQ(p2.value(), 123);
    }
    
    SUBCASE("Callbacks work on copied promises") {
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
        CHECK(!callback1_called);  // First callback was overwritten
        CHECK(callback2_called);   // Second callback is called
    }
}

TEST_CASE("fl::promise - Move Semantics") {
    SUBCASE("Promises are moveable") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::move(p1); // Move constructor
        
        CHECK(!p1.valid()); // p1 should be invalid after move
        CHECK(p2.valid());  // p2 should be valid
        
        p2.complete_with_value(42);
        CHECK(p2.is_resolved());
        CHECK_EQ(p2.value(), 42);
    }
    
    SUBCASE("Move assignment works") {
        auto p1 = fl::promise<int>::create();
        auto p2 = fl::promise<int>::create();
        
        p2 = fl::move(p1); // Move assignment
        
        CHECK(!p1.valid()); // p1 should be invalid after move
        CHECK(p2.valid());  // p2 should be valid
        
        p2.complete_with_value(123);
        CHECK(p2.is_resolved());
        CHECK_EQ(p2.value(), 123);
    }
}

TEST_CASE("fl::promise - Convenience Functions") {
    SUBCASE("make_resolved_promise() works") {
        auto p = make_resolved_promise(42);
        CHECK(p.valid());
        CHECK(p.is_resolved());
        CHECK_EQ(p.value(), 42);
    }
    
    SUBCASE("make_rejected_promise() with string works") {
        auto p = make_rejected_promise<int>("Test error");
        CHECK(p.valid());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "Test error");
    }
    
    SUBCASE("make_rejected_promise() with const char* works") {
        auto p = make_rejected_promise<int>("C string error");
        CHECK(p.valid());
        CHECK(p.is_rejected());
        CHECK_EQ(p.error().message, "C string error");
    }
}

TEST_CASE("fl::promise - Error Type") {
    SUBCASE("Error default constructor") {
        Error err;
        CHECK(err.message.empty());
    }
    
    SUBCASE("Error with string") {
        fl::string msg = "Test message";
        Error err(msg);
        CHECK_EQ(err.message, "Test message");
    }
    
    SUBCASE("Error with const char*") {
        Error err("C string message");
        CHECK_EQ(err.message, "C string message");
    }
    
    SUBCASE("Error with move string") {
        fl::string msg = "Move message";
        Error err(fl::move(msg));
        CHECK_EQ(err.message, "Move message");
    }
}

TEST_CASE("fl::promise - Edge Cases") {
    SUBCASE("Invalid promise methods return safe defaults") {
        fl::promise<int> invalid;
        
        CHECK(!invalid.valid());
        CHECK(!invalid.is_completed());
        CHECK(!invalid.is_resolved());
        CHECK(!invalid.is_rejected());
        
        // Should return default-constructed values for invalid promise
        CHECK_EQ(invalid.value(), int{});
        CHECK_EQ(invalid.error().message, fl::string{});
        
        // Methods should safely handle invalid state
        CHECK(!invalid.complete_with_value(42));
        CHECK(!invalid.complete_with_error(Error("error")));
        
        // Chaining should return reference even for invalid promise
        auto& ref = invalid.then([](const int&) {}).catch_([](const Error&) {});
        CHECK(&ref == &invalid);
    }
    
    SUBCASE("Multiple callbacks on same promise") {
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
        CHECK(!callback1_called);  // First callback was overwritten
        CHECK(callback2_called);   // Only the last callback is called
        CHECK_EQ(value2, 42);
    }
}

TEST_CASE("fl::promise - Complex Types") {
    SUBCASE("Promise with string type") {
        auto p = fl::promise<fl::string>::create();
        bool callback_called = false;
        fl::string received;
        
        p.then([&](const fl::string& value) {
            callback_called = true;
            received = value;
        });
        
        p.complete_with_value(fl::string("test string"));
        
        CHECK(callback_called);
        CHECK_EQ(received, "test string");
    }
    
    SUBCASE("Promise with custom struct") {
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
        
        CHECK(callback_called);
        CHECK(received == test_data);
    }
}

TEST_CASE("fl::PromiseResult - Basic Construction") {
    SUBCASE("construct with success value") {
        PromiseResult<int> result(42);
        
        CHECK(result.ok());
        CHECK(static_cast<bool>(result));
        CHECK_EQ(result.value(), 42);
        CHECK_EQ(result.error_message(), "");
    }
    
    SUBCASE("construct with error") {
        Error err("Test error");
        PromiseResult<int> result(err);
        
        CHECK(!result.ok());
        CHECK(!static_cast<bool>(result));
        CHECK_EQ(result.error().message, "Test error");
        CHECK_EQ(result.error_message(), "Test error");
    }
    
    SUBCASE("construct with move semantics") {
        fl::string text = "Hello World";
        PromiseResult<fl::string> result(fl::move(text));
        
        CHECK(result.ok());
        CHECK_EQ(result.value(), "Hello World");
    }
}

TEST_CASE("fl::PromiseResult - Value Access") {
    SUBCASE("safe value access on success") {
        PromiseResult<int> result(100);
        
        CHECK(result.ok());
        
        // Test const access
        const PromiseResult<int>& const_result = result;
        const int& const_value = const_result.value();
        CHECK_EQ(const_value, 100);
        
        // Test mutable access
        int& mutable_value = result.value();
        CHECK_EQ(mutable_value, 100);
        
        // Test modification
        mutable_value = 200;
        CHECK_EQ(result.value(), 200);
    }
    
    SUBCASE("value access on error in release builds") {
        PromiseResult<int> result(Error("Test error"));
        
        CHECK(!result.ok());
        
        // In release builds, this should return a static empty object
        // In debug builds, this would crash (which we can't test automatically)
        #ifndef DEBUG
            // Only test this in release builds to avoid crashes
            const int& value = result.value();
            // Should return a default-constructed int (0)
            CHECK_EQ(value, 0);
        #endif
    }
    
    SUBCASE("string value access") {
        PromiseResult<fl::string> result(fl::string("Test"));
        
        CHECK(result.ok());
        CHECK_EQ(result.value(), "Test");
        
        // Modify string
        result.value() = "Modified";
        CHECK_EQ(result.value(), "Modified");
    }
}

TEST_CASE("fl::PromiseResult - Error Access") {
    SUBCASE("safe error access on error") {
        Error original_error("Network timeout");
        PromiseResult<int> result(original_error);
        
        CHECK(!result.ok());
        
        const Error& error = result.error();
        CHECK_EQ(error.message, "Network timeout");
    }
    
    SUBCASE("error access on success in release builds") {
        PromiseResult<int> result(42);
        
        CHECK(result.ok());
        
        // In release builds, this should return a static descriptive error
        // In debug builds, this would crash (which we can't test automatically)
        #ifndef DEBUG
            const Error& error = result.error();
            // Should return a descriptive error message
            CHECK(error.message.find("success value") != fl::string::npos);
        #endif
    }
    
    SUBCASE("error_message convenience method") {
        // Test with error
        PromiseResult<int> error_result(Error("Connection failed"));
        CHECK_EQ(error_result.error_message(), "Connection failed");
        
        // Test with success
        PromiseResult<int> success_result(42);
        CHECK_EQ(success_result.error_message(), "");
    }
}

TEST_CASE("fl::PromiseResult - Type Conversions") {
    SUBCASE("boolean conversion") {
        PromiseResult<int> success(42);
        PromiseResult<int> failure(Error("Error"));
        
        // Test explicit bool conversion
        CHECK(static_cast<bool>(success));
        CHECK(!static_cast<bool>(failure));
        
        // Test in if statements
        if (success) {
            CHECK(true); // Should reach here
        } else {
            CHECK(false); // Should not reach here
        }
        
        if (failure) {
            CHECK(false); // Should not reach here
        } else {
            CHECK(true); // Should reach here
        }
    }
    
    SUBCASE("variant access") {
        PromiseResult<int> result(42);
        
        const auto& variant = result.variant();
        CHECK(variant.is<int>());
        CHECK_EQ(variant.get<int>(), 42);
    }
}

TEST_CASE("fl::PromiseResult - Helper Functions") {
    SUBCASE("make_success") {
        auto result1 = make_success(42);
        CHECK(result1.ok());
        CHECK_EQ(result1.value(), 42);
        
        fl::string text = "Hello";
        auto result2 = make_success(fl::move(text));
        CHECK(result2.ok());
        CHECK_EQ(result2.value(), "Hello");
    }
    
    SUBCASE("make_error with Error object") {
        Error err("Custom error");
        auto result = make_error<int>(err);
        
        CHECK(!result.ok());
        CHECK_EQ(result.error().message, "Custom error");
    }
    
    SUBCASE("make_error with string") {
        auto result1 = make_error<int>(fl::string("String error"));
        CHECK(!result1.ok());
        CHECK_EQ(result1.error().message, "String error");
        
        auto result2 = make_error<int>("C-string error");
        CHECK(!result2.ok());
        CHECK_EQ(result2.error().message, "C-string error");
    }
}

TEST_CASE("fl::PromiseResult - Complex Types") {
    SUBCASE("custom struct") {
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
        PromiseResult<TestStruct> result(original);
        
        CHECK(result.ok());
        
        const TestStruct& retrieved = result.value();
        CHECK(retrieved == original);
        CHECK_EQ(retrieved.x, 42);
        CHECK_EQ(retrieved.name, "test");
        
        // Test modification
        TestStruct& mutable_struct = result.value();
        mutable_struct.x = 99;
        CHECK_EQ(result.value().x, 99);
    }
}

TEST_CASE("fl::PromiseResult - Copy and Move Semantics") {
    SUBCASE("copy construction") {
        PromiseResult<int> original(42);
        PromiseResult<int> copy(original);
        
        CHECK(copy.ok());
        CHECK_EQ(copy.value(), 42);
        
        // Modify copy, original should be unchanged
        copy.value() = 100;
        CHECK_EQ(original.value(), 42);
        CHECK_EQ(copy.value(), 100);
    }
    
    SUBCASE("copy assignment") {
        PromiseResult<int> original(42);
        PromiseResult<int> copy = make_error<int>("temp");
        
        copy = original;
        
        CHECK(copy.ok());
        CHECK_EQ(copy.value(), 42);
    }
    
    SUBCASE("move construction") {
        fl::string text = "Move me";
        PromiseResult<fl::string> original(fl::move(text));
        PromiseResult<fl::string> moved(fl::move(original));
        
        CHECK(moved.ok());
        CHECK_EQ(moved.value(), "Move me");
    }
} 
