#include "doctest.h"
#include "fl/printf.h"
#include "fl/strstream.h"
#include "fl/io.h"
#include <iostream>

// Test helper for capturing platform output
namespace test_helper {
    // Forward declarations to satisfy -Werror=missing-declarations
    void capture_print(const char* str);
    void clear_capture();
    fl::string get_capture();
    
    static fl::string captured_output;
    
    void capture_print(const char* str) {
        captured_output += str;
    }
    
    void clear_capture() {
        captured_output.clear();
    }
    
    fl::string get_capture() {
        return captured_output;
    }
}

TEST_CASE("fl::printf basic functionality") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    SUBCASE("simple string formatting") {
        test_helper::clear_capture();
        fl::printf("Hello, %s!", "world");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Hello, world!");
        
        // Debug output to see what's happening
        std::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << std::endl;
        std::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << std::endl;
        
        // Use basic string comparison
        REQUIRE_EQ(result.size(), expected.size());
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("integer formatting") {
        test_helper::clear_capture();
        fl::printf("Value: %d", 42);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Value: 42");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("multiple arguments") {
        test_helper::clear_capture();
        fl::printf("Name: %s, Age: %d", "Alice", 25);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Name: Alice, Age: 25");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("floating point") {
        test_helper::clear_capture();
        fl::printf("Pi: %f", 3.14159f);
        // Check that it contains expected parts
        fl::string result = test_helper::get_capture();
        REQUIRE(result.find("3.14") != fl::string::npos);
    }
    
    SUBCASE("floating point with precision") {
        test_helper::clear_capture();
        fl::printf("Pi: %.2f", 3.14159f);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Pi: 3.14");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("character formatting") {
        test_helper::clear_capture();
        fl::printf("Letter: %c", 'A');
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Letter: A");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("hexadecimal formatting") {
        test_helper::clear_capture();
        fl::printf("Hex: %x", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Hex: ff");
        
        // Debug output to see what's happening
        std::cout << "[DEBUG] Hex Result: '" << result.c_str() << "' (length: " << result.size() << ")" << std::endl;
        std::cout << "[DEBUG] Hex Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << std::endl;
        
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("uppercase hexadecimal") {
        test_helper::clear_capture();
        fl::printf("HEX: %X", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("HEX: FF");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("literal percent") {
        test_helper::clear_capture();
        fl::printf("50%% complete");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("50% complete");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("unsigned integers") {
        test_helper::clear_capture();
        fl::printf("Unsigned: %u", 4294967295U);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Unsigned: 4294967295");
        
        // Debug output to see what's happening
        std::cout << "[DEBUG] Unsigned Result: '" << result.c_str() << "' (length: " << result.size() << ")" << std::endl;
        std::cout << "[DEBUG] Unsigned Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << std::endl;
        
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    // Cleanup
    fl::clear_io_handlers();
}

TEST_CASE("fl::printf edge cases") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    SUBCASE("empty format string") {
        test_helper::clear_capture();
        fl::printf("");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("no arguments") {
        test_helper::clear_capture();
        fl::printf("No placeholders here");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("No placeholders here");
        
        // Debug output to see what's happening
        std::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << std::endl;
        std::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << std::endl;
        
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("missing arguments") {
        test_helper::clear_capture();
        fl::printf("Value: %d");
        fl::string result = test_helper::get_capture();
        REQUIRE(result.find("<missing_arg>") != fl::string::npos);
    }
    
    SUBCASE("extra arguments") {
        test_helper::clear_capture();
        // Extra arguments should be ignored
        fl::printf("Value: %d", 42, 99);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Value: 42");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("zero values") {
        test_helper::clear_capture();
        fl::printf("Zero: %d, Hex: %x", 0, 0);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Zero: 0, Hex: 0");
        REQUIRE_EQ(strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    // Cleanup
    fl::clear_io_handlers();
}

TEST_CASE("fl::printf debug minimal") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    SUBCASE("debug format processing") {
        test_helper::clear_capture();
        
        // Test with just a literal string first
        fl::printf("test");
        fl::string result1 = test_helper::get_capture();
        std::cout << "[DEBUG] Literal: '" << result1.c_str() << "'" << std::endl;
        
        test_helper::clear_capture();
        
        // Test with just %s and a simple string
        fl::printf("%s", "hello");
        fl::string result2 = test_helper::get_capture();
        std::cout << "[DEBUG] Simple %s: '" << result2.c_str() << "'" << std::endl;
        
        test_helper::clear_capture();
        
        // Test the combination
        fl::printf("test %s", "hello");
        fl::string result3 = test_helper::get_capture();
        std::cout << "[DEBUG] Combined: '" << result3.c_str() << "'" << std::endl;
    }
    
    // Cleanup
    fl::clear_io_handlers();
}
