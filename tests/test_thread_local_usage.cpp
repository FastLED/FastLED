#include <doctest.h>
#include "fl/thread_local.h"
#include "fl/string.h"

using namespace fl;

TEST_CASE("ThreadLocal basic functionality") {
    SUBCASE("Default construction") {
        ThreadLocal<int> tl_int;
        CHECK(tl_int.access() == 0);  // Default constructed int should be 0
    }
    
    SUBCASE("Construction with default value") {
        ThreadLocal<int> tl_int(42);
        CHECK(tl_int.access() == 42);
    }
    
    SUBCASE("Assignment and access") {
        ThreadLocal<int> tl_int;
        tl_int = 100;
        CHECK(tl_int.access() == 100);
        
        // Test operator conversion
        int value = tl_int;
        CHECK(value == 100);
    }
    
    SUBCASE("Set method") {
        ThreadLocal<int> tl_int;
        tl_int.set(200);
        CHECK(tl_int.access() == 200);
    }
    
    SUBCASE("With custom type") {
        struct TestStruct {
            int x = 10;
            int y = 20;
        };
        
        ThreadLocal<TestStruct> tl_struct;
        CHECK(tl_struct.access().x == 10);
        CHECK(tl_struct.access().y == 20);
        
        tl_struct.access().x = 100;
        CHECK(tl_struct.access().x == 100);
    }
}

TEST_CASE("ThreadLocal with string type") {
    ThreadLocal<fl::string> tl_string("default");
    CHECK(tl_string.access() == "default");
    
    tl_string = "modified";
    CHECK(tl_string.access() == "modified");
}

#if FASTLED_USE_THREAD_LOCAL
// This test would only be meaningful with real threading support
// For demonstration purposes only - would need actual threading to test properly
TEST_CASE("ThreadLocal real implementation note") {
    ThreadLocal<int> tl_int(123);
    CHECK(tl_int.access() == 123);
    // In a real multi-threaded environment, each thread would have its own copy
    // This is a placeholder to show the API usage
}
#else
TEST_CASE("ThreadLocal fake implementation note") {
    ThreadLocal<int> tl_int(456);
    CHECK(tl_int.access() == 456);
    // In fake mode, all "threads" share the same global data
    // This is the current default behavior
}
#endif
