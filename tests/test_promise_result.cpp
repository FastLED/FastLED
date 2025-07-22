#include "test.h"
#include "fl/promise_result.h"
#include "fl/string.h"

using namespace fl;

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
