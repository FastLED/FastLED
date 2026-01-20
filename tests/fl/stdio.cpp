#include "doctest.h"
#include "fl/stl/stdio.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/string.h"
#include "fl/compiler_control.h"
#include "fl/stl/iostream.h"
#include "fl/str.h" // For std::sprintf and std::snprintf
#include "fl/stl/algorithm.h"
#include "fl/stl/new.h"
#include "fl/stl/ostream.h"
#include "fl/stl/cstring.h"
#include "fl/stl/move.h"
#include "fl/unused.h"





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
        fl::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        // Use basic string comparison
        REQUIRE_EQ(result.size(), expected.size());
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("integer formatting") {
        test_helper::clear_capture();
        fl::printf("Value: %d", 42);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Value: 42");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("multiple arguments") {
        test_helper::clear_capture();
        fl::printf("Name: %s, Age: %d", "Alice", 25);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Name: Alice, Age: 25");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
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
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("character formatting") {
        test_helper::clear_capture();
        fl::printf("Letter: %c", 'A');
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Letter: A");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("hexadecimal formatting") {
        test_helper::clear_capture();
        fl::printf("Hex: %x", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Hex: ff");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Hex Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Hex Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("uppercase hexadecimal") {
        test_helper::clear_capture();
        fl::printf("HEX: %X", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("HEX: FF");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("literal percent") {
        test_helper::clear_capture();
        fl::printf("50%% complete");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("50% complete");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("unsigned integers") {
        test_helper::clear_capture();
        fl::printf("Unsigned: %u", 4294967295U);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Unsigned: 4294967295");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Unsigned Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Unsigned Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
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
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("no arguments") {
        test_helper::clear_capture();
        fl::printf("No placeholders here");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("No placeholders here");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
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
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    SUBCASE("zero values") {
        test_helper::clear_capture();
        fl::printf("Zero: %d, Hex: %x", 0, 0);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Zero: 0, Hex: 0");
        REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
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
        fl::cout << "[DEBUG] Literal: '" << result1.c_str() << "'" << fl::endl;
        
        test_helper::clear_capture();
        
        // Test with just %s and a simple string
        fl::printf("%s", "hello");
        fl::string result2 = test_helper::get_capture();
        fl::cout << "[DEBUG] Simple %s: '" << result2.c_str() << "'" << fl::endl;
        
        test_helper::clear_capture();
        
        // Test the combination
        fl::printf("test %s", "hello");
        fl::string result3 = test_helper::get_capture();
        fl::cout << "[DEBUG] Combined: '" << result3.c_str() << "'" << fl::endl;
    }
    
    // Cleanup
    fl::clear_io_handlers();
}

TEST_CASE("fl::snprintf basic functionality") {
    SUBCASE("simple string formatting") {
        char buffer[100];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        REQUIRE_EQ(result, 13); // "Hello, world!" is 13 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    SUBCASE("integer formatting") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d", 42);
        REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    SUBCASE("multiple arguments") {
        char buffer[100];
        int result = fl::snprintf(buffer, sizeof(buffer), "Name: %s, Age: %d", "Alice", 25);
        REQUIRE_EQ(result, 20); // "Name: Alice, Age: 25" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Name: Alice, Age: 25"), 0);
    }
    
    SUBCASE("floating point") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Pi: %f", 3.14159f);
        REQUIRE_GT(result, 0);
        REQUIRE(fl::strstr(buffer, "3.14") != nullptr);
    }
    
    SUBCASE("floating point with precision") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Pi: %.2f", 3.14159f);
        REQUIRE_EQ(result, 8); // "Pi: 3.14" is 8 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Pi: 3.14"), 0);
    }
    
    SUBCASE("character formatting") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Letter: %c", 'A');
        REQUIRE_EQ(result, 9); // "Letter: A" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Letter: A"), 0);
    }
    
    SUBCASE("hexadecimal formatting") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hex: %x", 255);
        REQUIRE_EQ(result, 7); // "Hex: ff" is 7 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Hex: ff"), 0);
    }
    
    SUBCASE("uppercase hexadecimal") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "HEX: %X", 255);
        REQUIRE_EQ(result, 7); // "HEX: FF" is 7 characters
        REQUIRE_EQ(fl::strcmp(buffer, "HEX: FF"), 0);
    }
    
    SUBCASE("literal percent") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "50%% complete");
        REQUIRE_EQ(result, 12); // "50% complete" is 12 characters
        REQUIRE_EQ(fl::strcmp(buffer, "50% complete"), 0);
    }
    
    SUBCASE("unsigned integers") {
        char buffer[30];
        int result = fl::snprintf(buffer, sizeof(buffer), "Unsigned: %u", 4294967295U);
        REQUIRE_EQ(result, 20); // "Unsigned: 4294967295" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Unsigned: 4294967295"), 0);
    }
}

TEST_CASE("fl::snprintf buffer management") {
    SUBCASE("exact buffer size") {
        char buffer[14]; // Exact size for "Hello, world!" + null terminator
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        REQUIRE_EQ(result, 13); // Should return full length
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    SUBCASE("buffer too small") {
        char buffer[10]; // Too small for "Hello, world!"
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        REQUIRE_EQ(result, 9); // Should return number of characters actually written
        REQUIRE_EQ(fl::strlen(buffer), 9); // Buffer should contain 9 chars + null terminator
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, wo"), 0); // Truncated but null-terminated
    }
    
    SUBCASE("buffer size 1") {
        char buffer[1];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        REQUIRE_EQ(result, 0); // Should return 0 characters written (only null terminator fits)
        REQUIRE_EQ(buffer[0], '\0'); // Should only contain null terminator
    }
    
    SUBCASE("null buffer") {
        int result = fl::snprintf(nullptr, 100, "Hello, %s!", "world");
        REQUIRE_EQ(result, 0); // Should return 0 for null buffer
    }
    
    SUBCASE("zero size") {
        char buffer[10];
        int result = fl::snprintf(buffer, 0, "Hello, %s!", "world");
        REQUIRE_EQ(result, 0); // Should return 0 for zero size
    }
    
    SUBCASE("very long string") {
        char buffer[10];
        int result = fl::snprintf(buffer, sizeof(buffer), "This is a very long string that will be truncated");
        REQUIRE_EQ(result, 9); // Should return number of characters actually written
        REQUIRE_EQ(fl::strlen(buffer), 9); // Buffer should contain 9 chars + null terminator
        REQUIRE_EQ(fl::strcmp(buffer, "This is a"), 0); // Truncated but null-terminated
    }
}

TEST_CASE("fl::snprintf edge cases") {
    SUBCASE("empty format string") {
        char buffer[10];
        int result = fl::snprintf(buffer, sizeof(buffer), "");
        REQUIRE_EQ(result, 0);
        REQUIRE_EQ(fl::strcmp(buffer, ""), 0);
    }
    
    SUBCASE("no arguments") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "No placeholders here");
        REQUIRE_EQ(result, 20); // "No placeholders here" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "No placeholders here"), 0);
    }
    
    SUBCASE("missing arguments") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d");
        REQUIRE_GT(result, 0);
        REQUIRE(fl::strstr(buffer, "<missing_arg>") != nullptr);
    }
    
    SUBCASE("extra arguments") {
        char buffer[50];
        // Extra arguments should be ignored
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d", 42, 99);
        REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    SUBCASE("zero values") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Zero: %d, Hex: %x", 0, 0);
        REQUIRE_EQ(result, 15); // "Zero: 0, Hex: 0" is 15 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Zero: 0, Hex: 0"), 0);
    }
    
    SUBCASE("negative integers") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Negative: %d", -42);
        REQUIRE_EQ(result, 13); // "Negative: -42" is 13 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Negative: -42"), 0);
    }
    
    SUBCASE("large integers") {
        char buffer[30];
        int result = fl::snprintf(buffer, sizeof(buffer), "Large: %d", 2147483647);
        REQUIRE_EQ(result, 17); // "Large: 2147483647" is 17 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Large: 2147483647"), 0);
    }
}

TEST_CASE("fl::sprintf basic functionality") {
    SUBCASE("simple string formatting") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        REQUIRE_EQ(result, 13); // "Hello, world!" is 13 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    SUBCASE("integer formatting") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Value: %d", 42);
        REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    SUBCASE("multiple arguments") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Name: %s, Age: %d", "Alice", 25);
        REQUIRE_EQ(result, 20); // "Name: Alice, Age: 25" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Name: Alice, Age: 25"), 0);
    }
    
    SUBCASE("floating point") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Pi: %f", 3.14159f);
        REQUIRE_GT(result, 0);
        REQUIRE(fl::strstr(buffer, "3.14") != nullptr);
    }
    
    SUBCASE("floating point with precision") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Pi: %.2f", 3.14159f);
        REQUIRE_EQ(result, 8); // "Pi: 3.14" is 8 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Pi: 3.14"), 0);
    }
    
    SUBCASE("character formatting") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Letter: %c", 'A');
        REQUIRE_EQ(result, 9); // "Letter: A" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Letter: A"), 0);
    }
    
    SUBCASE("hexadecimal formatting") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Hex: %x", 255);
        REQUIRE_EQ(result, 7); // "Hex: ff" is 7 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Hex: ff"), 0);
    }
    
    SUBCASE("uppercase hexadecimal") {
        char buffer[20];
        int result = fl::sprintf(buffer, "HEX: %X", 255);
        REQUIRE_EQ(result, 7); // "HEX: FF" is 7 characters
        REQUIRE_EQ(fl::strcmp(buffer, "HEX: FF"), 0);
    }
    
    SUBCASE("literal percent") {
        char buffer[20];
        int result = fl::sprintf(buffer, "50%% complete");
        REQUIRE_EQ(result, 12); // "50% complete" is 12 characters
        REQUIRE_EQ(fl::strcmp(buffer, "50% complete"), 0);
    }
    
    SUBCASE("unsigned integers") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Unsigned: %u", 4294967295U);
        REQUIRE_EQ(result, 20); // "Unsigned: 4294967295" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Unsigned: 4294967295"), 0);
    }
}

TEST_CASE("fl::sprintf buffer management") {
    SUBCASE("exact buffer size") {
        char buffer[14]; // Exact size for "Hello, world!" + null terminator
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        REQUIRE_EQ(result, 13); // Should return length written
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    SUBCASE("large buffer") {
        char buffer[100]; // Much larger than needed
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        REQUIRE_EQ(result, 13); // Should return actual length written
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    
    SUBCASE("very long string") {
        char buffer[100]; // Large enough buffer
        int result = fl::sprintf(buffer, "This is a very long string that will fit in the buffer");
        const char* expected = "This is a very long string that will fit in the buffer";
        int expected_len = fl::strlen(expected);
        
        REQUIRE_EQ(result, expected_len); // Should return actual length written
        REQUIRE_EQ(fl::strcmp(buffer, expected), 0);
    }

    SUBCASE("overflow") {
        char buffer[10];
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        REQUIRE_EQ(result, 9); // Should return the number of characters actually written (excluding null terminator)
        REQUIRE_EQ(fl::strcmp(buffer, "Hello, wo"), 0); // Should be truncated to fit in buffer
        REQUIRE_EQ(fl::string("Hello, wo"), buffer);
    }
    
}

TEST_CASE("fl::sprintf edge cases") {
    SUBCASE("empty format string") {
        char buffer[10];
        int result = fl::sprintf(buffer, "");
        REQUIRE_EQ(result, 0);
        REQUIRE_EQ(fl::strcmp(buffer, ""), 0);
    }
    
    SUBCASE("no arguments") {
        char buffer[50];
        int result = fl::sprintf(buffer, "No placeholders here");
        REQUIRE_EQ(result, 20); // "No placeholders here" is 20 characters
        REQUIRE_EQ(fl::strcmp(buffer, "No placeholders here"), 0);
    }
    
    SUBCASE("missing arguments") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Value: %d");
        REQUIRE_GT(result, 0);
        REQUIRE(fl::strstr(buffer, "<missing_arg>") != nullptr);
    }
    
    SUBCASE("extra arguments") {
        char buffer[50];
        // Extra arguments should be ignored
        int result = fl::sprintf(buffer, "Value: %d", 42, 99);
        REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    SUBCASE("zero values") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Zero: %d, Hex: %x", 0, 0);
        REQUIRE_EQ(result, 15); // "Zero: 0, Hex: 0" is 15 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Zero: 0, Hex: 0"), 0);
    }
    
    SUBCASE("negative integers") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Negative: %d", -42);
        REQUIRE_EQ(result, 13); // "Negative: -42" is 13 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Negative: -42"), 0);
    }
    
    SUBCASE("large integers") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Large: %d", 2147483647);
        REQUIRE_EQ(result, 17); // "Large: 2147483647" is 17 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Large: 2147483647"), 0);
    }
}

TEST_CASE("fl::sprintf comprehensive functionality") {
    // These tests verify that sprintf works correctly with various buffer sizes
    // and formatting scenarios
    
    SUBCASE("small string") {
        char buffer[10];
        int result = fl::sprintf(buffer, "Test");
        REQUIRE_EQ(result, 4); // "Test" is 4 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Test"), 0);
    }
    
    SUBCASE("medium string with formatting") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Medium: %d", 123);
        REQUIRE_EQ(result, 11); // "Medium: 123" is 11 characters
        REQUIRE_EQ(fl::strcmp(buffer, "Medium: 123"), 0);
    }
    
    SUBCASE("large string with multiple arguments") {
        char buffer[200];
        int result = fl::sprintf(buffer, "Large buffer test with number: %d and string: %s", 42, "hello");
        const char* expected = "Large buffer test with number: 42 and string: hello";
        int expected_len = fl::strlen(expected);
        
        REQUIRE_EQ(result, expected_len);
        REQUIRE_EQ(fl::strcmp(buffer, expected), 0);
    }
    
    SUBCASE("exact content length") {
        char buffer[10]; // Exactly "hello" + extra space + null terminator
        int result = fl::sprintf(buffer, "hello");
        REQUIRE_EQ(result, 5); // "hello" is 5 characters
        REQUIRE_EQ(fl::strcmp(buffer, "hello"), 0);
    }
    
    SUBCASE("complex formatting") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        REQUIRE_GT(result, 0);
        REQUIRE(fl::strstr(buffer, "Int: 123") != nullptr);
        REQUIRE(fl::strstr(buffer, "Float: 3.14") != nullptr);
        REQUIRE(fl::strstr(buffer, "Hex: ff") != nullptr);
        REQUIRE(fl::strstr(buffer, "Char: A") != nullptr);
    }
}

TEST_CASE("fl::sprintf vs fl::snprintf comparison") {
    // Test that sprintf behaves similarly to snprintf when buffer is large enough
    
    SUBCASE("identical behavior for basic formatting") {
        char buffer1[50];
        char buffer2[50];
        
        int result1 = fl::sprintf(buffer1, "Test: %d, %s", 42, "hello");
        int result2 = fl::snprintf(buffer2, 50, "Test: %d, %s", 42, "hello");
        
        REQUIRE_EQ(result1, result2);
        REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    SUBCASE("sprintf writes full string when buffer is large enough") {
        char buffer1[100];
        char buffer2[100];
        
        int result1 = fl::sprintf(buffer1, "This is a moderately long string");
        int result2 = fl::snprintf(buffer2, 100, "This is a moderately long string");
        
        REQUIRE_EQ(result1, result2);
        REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    SUBCASE("identical behavior for complex formatting") {
        char buffer1[100];
        char buffer2[100];
        
        int result1 = fl::sprintf(buffer1, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        int result2 = fl::snprintf(buffer2, 100, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        
        REQUIRE_EQ(result1, result2);
        REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
}


TEST_CASE("fl::LogLevel control") {
    // Test log level control and ScopedLogDisable RAII class

    SUBCASE("default log level is DEBUG") {
        // Ensure we start with DEBUG level
        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_DEBUG);
    }

    SUBCASE("setLogLevel and getLogLevel") {
        // Save original level
        uint8_t originalLevel = fl::getLogLevel();

        fl::setLogLevel(fl::LOG_LEVEL_NONE);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);

        fl::setLogLevel(fl::LOG_LEVEL_ERROR);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_ERROR);

        fl::setLogLevel(fl::LOG_LEVEL_WARN);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_WARN);

        fl::setLogLevel(fl::LOG_LEVEL_INFO);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_INFO);

        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_DEBUG);

        // Restore original level
        fl::setLogLevel(originalLevel);
    }

    SUBCASE("ScopedLogDisable disables logging") {
        // Setup capture for testing platform output
        fl::inject_println_handler(test_helper::capture_print);
        test_helper::clear_capture();

        // Ensure we start with DEBUG level (logging enabled)
        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_DEBUG);

        // Output should be captured when logging is enabled
        fl::println("before scope");
        fl::string result1 = test_helper::get_capture();
        REQUIRE(result1.find("before scope") != fl::string::npos);

        test_helper::clear_capture();

        {
            fl::ScopedLogDisable guard;
            REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);

            // Output should be suppressed
            fl::println("inside scope - should be suppressed");
            fl::string result2 = test_helper::get_capture();
            REQUIRE_EQ(result2.size(), 0);
        }

        // After scope ends, logging should be restored
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_DEBUG);

        fl::println("after scope");
        fl::string result3 = test_helper::get_capture();
        REQUIRE(result3.find("after scope") != fl::string::npos);

        // Cleanup
        fl::clear_io_handlers();
    }

    SUBCASE("ScopedLogDisable restores previous level") {
        // Test that ScopedLogDisable restores the previous level, not just DEBUG
        fl::setLogLevel(fl::LOG_LEVEL_WARN);
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_WARN);

        {
            fl::ScopedLogDisable guard;
            REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);
        }

        // Should restore to WARN, not DEBUG
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_WARN);

        // Restore to DEBUG for other tests
        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
    }

    SUBCASE("nested ScopedLogDisable") {
        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);

        {
            fl::ScopedLogDisable outer;
            REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);

            {
                fl::ScopedLogDisable inner;
                REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);
            }

            // Inner restored to outer's saved state (NONE)
            REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_NONE);
        }

        // Outer restored to original DEBUG level
        REQUIRE_EQ(fl::getLogLevel(), fl::LOG_LEVEL_DEBUG);
    }

    SUBCASE("LOG_LEVEL_NONE suppresses print") {
        fl::inject_print_handler(test_helper::capture_print);
        test_helper::clear_capture();

        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
        fl::print("test1");
        fl::string result1 = test_helper::get_capture();
        REQUIRE(result1.find("test1") != fl::string::npos);

        test_helper::clear_capture();

        fl::setLogLevel(fl::LOG_LEVEL_NONE);
        fl::print("test2");
        fl::string result2 = test_helper::get_capture();
        REQUIRE_EQ(result2.size(), 0);

        // Restore
        fl::setLogLevel(fl::LOG_LEVEL_DEBUG);
        fl::clear_io_handlers();
    }
}

// Note: std::snprintf comparison test was removed to maintain FL compatibility
// fl::snprintf is tested independently in the tests above
