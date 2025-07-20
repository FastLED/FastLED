#include "test.h"
#include "fl/future.h"
#include "fl/string.h"

using namespace fl;

TEST_CASE("Future variant basic usage") {
    // Test pending future
    auto future = fl::future<int>::create();
    auto result = future.try_get_result();
    
    // Should be pending initially
    REQUIRE(result.is<FuturePending>());
    REQUIRE(!result.is<int>());
    REQUIRE(!result.is<FutureError>());
    
    // Complete with value
    future.complete_with_value(42);
    result = future.try_get_result();
    
    // Should now have value
    REQUIRE(result.is<int>());
    REQUIRE(!result.is<FuturePending>());
    REQUIRE(!result.is<FutureError>());
    REQUIRE_EQ(42, *result.ptr<int>());
}

TEST_CASE("Future variant error handling") {
    auto future = fl::future<fl::string>::create();
    future.complete_with_error("Network timeout");
    
    auto result = future.try_get_result();
    
    // Should be error state
    REQUIRE(result.is<FutureError>());
    REQUIRE(!result.is<fl::string>());
    REQUIRE(!result.is<FuturePending>());
    
    auto error = result.ptr<FutureError>();
    REQUIRE_EQ(fl::string("Network timeout"), error->message);
}

TEST_CASE("Future variant visitor pattern") {
    struct TestVisitor {
        int value_called = 0;
        int error_called = 0;
        int pending_called = 0;
        fl::string last_error;
        int last_value = 0;
        
        void accept(const int& value) {
            value_called++;
            last_value = value;
        }
        
        void accept(const FutureError& error) {
            error_called++;
            last_error = error.message;
        }
        
        void accept(const FuturePending& pending) {
            pending_called++;
        }
    };
    
    // Test pending
    auto future1 = fl::future<int>::create();
    auto result1 = future1.try_get_result();
    TestVisitor visitor1;
    result1.visit(visitor1);
    REQUIRE_EQ(1, visitor1.pending_called);
    REQUIRE_EQ(0, visitor1.value_called);
    REQUIRE_EQ(0, visitor1.error_called);
    
    // Test value
    auto future2 = fl::future<int>::create();
    future2.complete_with_value(123);
    auto result2 = future2.try_get_result();
    TestVisitor visitor2;
    result2.visit(visitor2);
    REQUIRE_EQ(0, visitor2.pending_called);
    REQUIRE_EQ(1, visitor2.value_called);
    REQUIRE_EQ(0, visitor2.error_called);
    REQUIRE_EQ(123, visitor2.last_value);
    
    // Test error
    auto future3 = fl::future<int>::create();
    future3.complete_with_error("Test error");
    auto result3 = future3.try_get_result();
    TestVisitor visitor3;
    result3.visit(visitor3);
    REQUIRE_EQ(0, visitor3.pending_called);
    REQUIRE_EQ(0, visitor3.value_called);
    REQUIRE_EQ(1, visitor3.error_called);
    REQUIRE_EQ(fl::string("Test error"), visitor3.last_error);
}

TEST_CASE("Future variant convenience functions") {
    // Test make_ready_future
    auto ready_future = make_ready_future<int>(99);
    auto result = ready_future.try_get_result();
    REQUIRE(result.is<int>());
    REQUIRE_EQ(99, *result.ptr<int>());
    
    // Test make_error_future
    auto error_future = make_error_future<int>("Test error");
    auto error_result = error_future.try_get_result();
    REQUIRE(error_result.is<FutureError>());
    REQUIRE_EQ(fl::string("Test error"), error_result.ptr<FutureError>()->message);
    
    // Test make_invalid_future  
    auto invalid_future = make_invalid_future<int>();
    auto invalid_result = invalid_future.try_get_result();
    REQUIRE(invalid_result.is<FuturePending>());
}

TEST_CASE("Future variant legacy compatibility") {
    // Test that old API still works
    auto future = fl::future<int>::create();
    
    // Legacy method should return empty optional when pending
    auto legacy_result = future.try_result();
    REQUIRE(legacy_result.empty());
    
    // Complete with value
    future.complete_with_value(42);
    legacy_result = future.try_result();
    REQUIRE(!legacy_result.empty());
    REQUIRE_EQ(42, *legacy_result.ptr());
    
    // Check that new API also works
    auto new_result = future.try_get_result();
    REQUIRE(new_result.is<int>());
    REQUIRE_EQ(42, *new_result.ptr<int>());
}

TEST_CASE("Future variant move semantics") {
    auto future = fl::future<fl::string>::create();
    future.complete_with_value(fl::string("Hello World"));
    
    auto result = future.try_get_result();
    REQUIRE(result.is<fl::string>());
    
    fl::string value = *result.ptr<fl::string>();
    REQUIRE_EQ(fl::string("Hello World"), value);
}

TEST_CASE("Future variant invalid future") {
    // Default-constructed future should be invalid
    fl::future<int> invalid_future;
    REQUIRE(!invalid_future.valid());
    
    auto result = invalid_future.try_get_result();
    REQUIRE(result.is<FuturePending>());
} 
