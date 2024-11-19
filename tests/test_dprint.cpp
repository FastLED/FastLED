
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fl/dbg.h"
#include "namespace.h"

FASTLED_USING_NAMESPACE

TEST_CASE("dprint") {
    // Test basic string output
    FASTLED_DPRINT() << "Test message";
    
    // Test multiple types in one chain
    FASTLED_DPRINT() << "Number: " << 42 << " Float: " << 3.14f;
    
    // Test with endl
    FASTLED_DPRINT() << "Line 1" << std::endl << "Line 2";
    
    // Since this is a dummy implementation, we're mainly testing that it compiles
    // and doesn't crash. No actual output verification is needed.
    CHECK(true); // Verify test ran
}


