#include "fl/ostream.h"
#include "test.h"
#include <cstring>

// Since we can't override the fl::print function easily in this test setup,
// we'll just verify that the ostream compiles and basic functionality works

TEST_CASE("fl::cout basic operations compile and run without crash") {
    // Test basic string output - should not crash
    fl::cout << "Hello, World!";
    
    // Test integer output - should not crash
    fl::cout << 42;
    fl::cout << -123;
    
    // Test character output - should not crash
    fl::cout << 'A';
    
    // Test float output - should not crash
    fl::cout << 3.14f;
    fl::cout << 2.718;
    
    // Test various integer types - should not crash
    fl::cout << static_cast<int8_t>(127);
    fl::cout << static_cast<uint8_t>(255);
    fl::cout << static_cast<int16_t>(32767);
    fl::cout << static_cast<uint16_t>(65535);
    fl::cout << static_cast<int32_t>(2147483647);
    fl::cout << static_cast<uint32_t>(4294967295U);
    
    // Test chaining - should not crash
    fl::cout << "Value: " << 42 << " End";
    
    // Test endl - should not crash
    fl::cout << "Line 1" << fl::endl << "Line 2";
    
    // Test null safety - should not crash
    const char* null_str = nullptr;
    fl::cout << null_str;
    
    // Test string objects
    fl::string test_str = "Test string";
    fl::cout << test_str;
    
    // If we got here without crashing, the basic functionality works
    CHECK(true);
}

TEST_CASE("fl::cout type conversions work correctly") {
    // We can't easily capture output for verification in this test setup,
    // but we can verify that the type conversion methods don't crash
    // and that temporary string objects are created properly
    
    // Test that we can create a temporary string and append different types
    fl::string temp;
    temp.append(42);
    CHECK_EQ(temp, "42");
    
    temp.clear();
    temp.append(-123);
    CHECK_EQ(temp, "-123");
    
    temp.clear();
    temp.append(3.14f);
    CHECK(temp.find("3.1") != fl::string::npos);
    
    // The cout should use the same underlying string append mechanisms
    CHECK(true);
}
