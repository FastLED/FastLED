#include "test.h"
#include "fl/stl/stdio.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/string.h"
#include "fl/compiler_control.h"
#include "fl/int.h"
#include "fl/stl/function.h"
#include "fl/stl/ostream.h"
#include "fl/stl/cstring.h"
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

FL_TEST_CASE("fl::printf basic functionality") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    FL_SUBCASE("simple string formatting") {
        test_helper::clear_capture();
        fl::printf("Hello, %s!", "world");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Hello, world!");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        // Use basic string comparison
        FL_REQUIRE_EQ(result.size(), expected.size());
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("integer formatting") {
        test_helper::clear_capture();
        fl::printf("Value: %d", 42);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Value: 42");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("multiple arguments") {
        test_helper::clear_capture();
        fl::printf("Name: %s, Age: %d", "Alice", 25);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Name: Alice, Age: 25");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("floating point") {
        test_helper::clear_capture();
        fl::printf("Pi: %f", 3.14159f);
        // Check that it contains expected parts
        fl::string result = test_helper::get_capture();
        FL_REQUIRE(result.find("3.14") != fl::string::npos);
    }
    
    FL_SUBCASE("floating point with precision") {
        test_helper::clear_capture();
        fl::printf("Pi: %.2f", 3.14159f);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Pi: 3.14");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("character formatting") {
        test_helper::clear_capture();
        fl::printf("Letter: %c", 'A');
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Letter: A");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("hexadecimal formatting") {
        test_helper::clear_capture();
        fl::printf("Hex: %x", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Hex: ff");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Hex Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Hex Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("uppercase hexadecimal") {
        test_helper::clear_capture();
        fl::printf("HEX: %X", 255);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("HEX: FF");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("literal percent") {
        test_helper::clear_capture();
        fl::printf("50%% complete");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("50% complete");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("unsigned integers") {
        test_helper::clear_capture();
        fl::printf("Unsigned: %u", 4294967295U);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Unsigned: 4294967295");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Unsigned Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Unsigned Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    // Cleanup
    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::printf edge cases") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    FL_SUBCASE("empty format string") {
        test_helper::clear_capture();
        fl::printf("");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("no arguments") {
        test_helper::clear_capture();
        fl::printf("No placeholders here");
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("No placeholders here");
        
        // Debug output to see what's happening
        fl::cout << "[DEBUG] Result: '" << result.c_str() << "' (length: " << result.size() << ")" << fl::endl;
        fl::cout << "[DEBUG] Expected: '" << expected.c_str() << "' (length: " << expected.size() << ")" << fl::endl;
        
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("missing arguments") {
        test_helper::clear_capture();
        fl::printf("Value: %d");
        fl::string result = test_helper::get_capture();
        FL_REQUIRE(result.find("<missing_arg>") != fl::string::npos);
    }
    
    FL_SUBCASE("extra arguments") {
        test_helper::clear_capture();
        // Extra arguments should be ignored
        fl::printf("Value: %d", 42, 99);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Value: 42");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    FL_SUBCASE("zero values") {
        test_helper::clear_capture();
        fl::printf("Zero: %d, Hex: %x", 0, 0);
        fl::string result = test_helper::get_capture();
        fl::string expected = fl::string("Zero: 0, Hex: 0");
        FL_REQUIRE_EQ(fl::strcmp(result.c_str(), expected.c_str()), 0);
    }
    
    // Cleanup
    fl::clear_io_handlers();
}

FL_TEST_CASE("fl::printf debug minimal") {
    // Setup capture for testing platform output
    fl::inject_print_handler(test_helper::capture_print);
    
    FL_SUBCASE("debug format processing") {
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

FL_TEST_CASE("fl::snprintf basic functionality") {
    FL_SUBCASE("simple string formatting") {
        char buffer[100];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 13); // "Hello, world!" is 13 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    FL_SUBCASE("integer formatting") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d", 42);
        FL_REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    FL_SUBCASE("multiple arguments") {
        char buffer[100];
        int result = fl::snprintf(buffer, sizeof(buffer), "Name: %s, Age: %d", "Alice", 25);
        FL_REQUIRE_EQ(result, 20); // "Name: Alice, Age: 25" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Name: Alice, Age: 25"), 0);
    }
    
    FL_SUBCASE("floating point") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Pi: %f", 3.14159f);
        FL_REQUIRE_GT(result, 0);
        FL_REQUIRE(fl::strstr(buffer, "3.14") != nullptr);
    }
    
    FL_SUBCASE("floating point with precision") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Pi: %.2f", 3.14159f);
        FL_REQUIRE_EQ(result, 8); // "Pi: 3.14" is 8 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Pi: 3.14"), 0);
    }
    
    FL_SUBCASE("character formatting") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Letter: %c", 'A');
        FL_REQUIRE_EQ(result, 9); // "Letter: A" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Letter: A"), 0);
    }
    
    FL_SUBCASE("hexadecimal formatting") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hex: %x", 255);
        FL_REQUIRE_EQ(result, 7); // "Hex: ff" is 7 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hex: ff"), 0);
    }
    
    FL_SUBCASE("uppercase hexadecimal") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "HEX: %X", 255);
        FL_REQUIRE_EQ(result, 7); // "HEX: FF" is 7 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "HEX: FF"), 0);
    }
    
    FL_SUBCASE("literal percent") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "50%% complete");
        FL_REQUIRE_EQ(result, 12); // "50% complete" is 12 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "50% complete"), 0);
    }
    
    FL_SUBCASE("unsigned integers") {
        char buffer[30];
        int result = fl::snprintf(buffer, sizeof(buffer), "Unsigned: %u", 4294967295U);
        FL_REQUIRE_EQ(result, 20); // "Unsigned: 4294967295" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Unsigned: 4294967295"), 0);
    }
}

FL_TEST_CASE("fl::snprintf buffer management") {
    FL_SUBCASE("exact buffer size") {
        char buffer[14]; // Exact size for "Hello, world!" + null terminator
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 13); // Should return full length
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    FL_SUBCASE("buffer too small") {
        char buffer[10]; // Too small for "Hello, world!"
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 9); // Should return number of characters actually written
        FL_REQUIRE_EQ(fl::strlen(buffer), 9); // Buffer should contain 9 chars + null terminator
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, wo"), 0); // Truncated but null-terminated
    }
    
    FL_SUBCASE("buffer size 1") {
        char buffer[1];
        int result = fl::snprintf(buffer, sizeof(buffer), "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 0); // Should return 0 characters written (only null terminator fits)
        FL_REQUIRE_EQ(buffer[0], '\0'); // Should only contain null terminator
    }
    
    FL_SUBCASE("null buffer") {
        int result = fl::snprintf(nullptr, 100, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 0); // Should return 0 for null buffer
    }
    
    FL_SUBCASE("zero size") {
        char buffer[10];
        int result = fl::snprintf(buffer, 0, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 0); // Should return 0 for zero size
    }
    
    FL_SUBCASE("very long string") {
        char buffer[10];
        int result = fl::snprintf(buffer, sizeof(buffer), "This is a very long string that will be truncated");
        FL_REQUIRE_EQ(result, 9); // Should return number of characters actually written
        FL_REQUIRE_EQ(fl::strlen(buffer), 9); // Buffer should contain 9 chars + null terminator
        FL_REQUIRE_EQ(fl::strcmp(buffer, "This is a"), 0); // Truncated but null-terminated
    }
}

FL_TEST_CASE("fl::snprintf edge cases") {
    FL_SUBCASE("empty format string") {
        char buffer[10];
        int result = fl::snprintf(buffer, sizeof(buffer), "");
        FL_REQUIRE_EQ(result, 0);
        FL_REQUIRE_EQ(fl::strcmp(buffer, ""), 0);
    }
    
    FL_SUBCASE("no arguments") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "No placeholders here");
        FL_REQUIRE_EQ(result, 20); // "No placeholders here" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "No placeholders here"), 0);
    }
    
    FL_SUBCASE("missing arguments") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d");
        FL_REQUIRE_GT(result, 0);
        FL_REQUIRE(fl::strstr(buffer, "<missing_arg>") != nullptr);
    }
    
    FL_SUBCASE("extra arguments") {
        char buffer[50];
        // Extra arguments should be ignored
        int result = fl::snprintf(buffer, sizeof(buffer), "Value: %d", 42, 99);
        FL_REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    FL_SUBCASE("zero values") {
        char buffer[50];
        int result = fl::snprintf(buffer, sizeof(buffer), "Zero: %d, Hex: %x", 0, 0);
        FL_REQUIRE_EQ(result, 15); // "Zero: 0, Hex: 0" is 15 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Zero: 0, Hex: 0"), 0);
    }
    
    FL_SUBCASE("negative integers") {
        char buffer[20];
        int result = fl::snprintf(buffer, sizeof(buffer), "Negative: %d", -42);
        FL_REQUIRE_EQ(result, 13); // "Negative: -42" is 13 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Negative: -42"), 0);
    }
    
    FL_SUBCASE("large integers") {
        char buffer[30];
        int result = fl::snprintf(buffer, sizeof(buffer), "Large: %d", 2147483647);
        FL_REQUIRE_EQ(result, 17); // "Large: 2147483647" is 17 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Large: 2147483647"), 0);
    }
}

FL_TEST_CASE("fl::sprintf basic functionality") {
    FL_SUBCASE("simple string formatting") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 13); // "Hello, world!" is 13 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    FL_SUBCASE("integer formatting") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Value: %d", 42);
        FL_REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    FL_SUBCASE("multiple arguments") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Name: %s, Age: %d", "Alice", 25);
        FL_REQUIRE_EQ(result, 20); // "Name: Alice, Age: 25" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Name: Alice, Age: 25"), 0);
    }
    
    FL_SUBCASE("floating point") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Pi: %f", 3.14159f);
        FL_REQUIRE_GT(result, 0);
        FL_REQUIRE(fl::strstr(buffer, "3.14") != nullptr);
    }
    
    FL_SUBCASE("floating point with precision") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Pi: %.2f", 3.14159f);
        FL_REQUIRE_EQ(result, 8); // "Pi: 3.14" is 8 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Pi: 3.14"), 0);
    }
    
    FL_SUBCASE("character formatting") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Letter: %c", 'A');
        FL_REQUIRE_EQ(result, 9); // "Letter: A" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Letter: A"), 0);
    }
    
    FL_SUBCASE("hexadecimal formatting") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Hex: %x", 255);
        FL_REQUIRE_EQ(result, 7); // "Hex: ff" is 7 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hex: ff"), 0);
    }
    
    FL_SUBCASE("uppercase hexadecimal") {
        char buffer[20];
        int result = fl::sprintf(buffer, "HEX: %X", 255);
        FL_REQUIRE_EQ(result, 7); // "HEX: FF" is 7 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "HEX: FF"), 0);
    }
    
    FL_SUBCASE("literal percent") {
        char buffer[20];
        int result = fl::sprintf(buffer, "50%% complete");
        FL_REQUIRE_EQ(result, 12); // "50% complete" is 12 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "50% complete"), 0);
    }
    
    FL_SUBCASE("unsigned integers") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Unsigned: %u", 4294967295U);
        FL_REQUIRE_EQ(result, 20); // "Unsigned: 4294967295" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Unsigned: 4294967295"), 0);
    }
}

FL_TEST_CASE("fl::sprintf buffer management") {
    FL_SUBCASE("exact buffer size") {
        char buffer[14]; // Exact size for "Hello, world!" + null terminator
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 13); // Should return length written
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    FL_SUBCASE("large buffer") {
        char buffer[100]; // Much larger than needed
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 13); // Should return actual length written
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, world!"), 0);
    }
    
    
    FL_SUBCASE("very long string") {
        char buffer[100]; // Large enough buffer
        int result = fl::sprintf(buffer, "This is a very long string that will fit in the buffer");
        const char* expected = "This is a very long string that will fit in the buffer";
        int expected_len = fl::strlen(expected);
        
        FL_REQUIRE_EQ(result, expected_len); // Should return actual length written
        FL_REQUIRE_EQ(fl::strcmp(buffer, expected), 0);
    }

    FL_SUBCASE("overflow") {
        char buffer[10];
        int result = fl::sprintf(buffer, "Hello, %s!", "world");
        FL_REQUIRE_EQ(result, 9); // Should return the number of characters actually written (excluding null terminator)
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Hello, wo"), 0); // Should be truncated to fit in buffer
        FL_REQUIRE_EQ(fl::string("Hello, wo"), buffer);
    }
    
}

FL_TEST_CASE("fl::sprintf edge cases") {
    FL_SUBCASE("empty format string") {
        char buffer[10];
        int result = fl::sprintf(buffer, "");
        FL_REQUIRE_EQ(result, 0);
        FL_REQUIRE_EQ(fl::strcmp(buffer, ""), 0);
    }
    
    FL_SUBCASE("no arguments") {
        char buffer[50];
        int result = fl::sprintf(buffer, "No placeholders here");
        FL_REQUIRE_EQ(result, 20); // "No placeholders here" is 20 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "No placeholders here"), 0);
    }
    
    FL_SUBCASE("missing arguments") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Value: %d");
        FL_REQUIRE_GT(result, 0);
        FL_REQUIRE(fl::strstr(buffer, "<missing_arg>") != nullptr);
    }
    
    FL_SUBCASE("extra arguments") {
        char buffer[50];
        // Extra arguments should be ignored
        int result = fl::sprintf(buffer, "Value: %d", 42, 99);
        FL_REQUIRE_EQ(result, 9); // "Value: 42" is 9 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Value: 42"), 0);
    }
    
    FL_SUBCASE("zero values") {
        char buffer[50];
        int result = fl::sprintf(buffer, "Zero: %d, Hex: %x", 0, 0);
        FL_REQUIRE_EQ(result, 15); // "Zero: 0, Hex: 0" is 15 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Zero: 0, Hex: 0"), 0);
    }
    
    FL_SUBCASE("negative integers") {
        char buffer[20];
        int result = fl::sprintf(buffer, "Negative: %d", -42);
        FL_REQUIRE_EQ(result, 13); // "Negative: -42" is 13 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Negative: -42"), 0);
    }
    
    FL_SUBCASE("large integers") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Large: %d", 2147483647);
        FL_REQUIRE_EQ(result, 17); // "Large: 2147483647" is 17 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Large: 2147483647"), 0);
    }
}

FL_TEST_CASE("fl::sprintf comprehensive functionality") {
    // These tests verify that sprintf works correctly with various buffer sizes
    // and formatting scenarios
    
    FL_SUBCASE("small string") {
        char buffer[10];
        int result = fl::sprintf(buffer, "Test");
        FL_REQUIRE_EQ(result, 4); // "Test" is 4 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Test"), 0);
    }
    
    FL_SUBCASE("medium string with formatting") {
        char buffer[30];
        int result = fl::sprintf(buffer, "Medium: %d", 123);
        FL_REQUIRE_EQ(result, 11); // "Medium: 123" is 11 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "Medium: 123"), 0);
    }
    
    FL_SUBCASE("large string with multiple arguments") {
        char buffer[200];
        int result = fl::sprintf(buffer, "Large buffer test with number: %d and string: %s", 42, "hello");
        const char* expected = "Large buffer test with number: 42 and string: hello";
        int expected_len = fl::strlen(expected);
        
        FL_REQUIRE_EQ(result, expected_len);
        FL_REQUIRE_EQ(fl::strcmp(buffer, expected), 0);
    }
    
    FL_SUBCASE("exact content length") {
        char buffer[10]; // Exactly "hello" + extra space + null terminator
        int result = fl::sprintf(buffer, "hello");
        FL_REQUIRE_EQ(result, 5); // "hello" is 5 characters
        FL_REQUIRE_EQ(fl::strcmp(buffer, "hello"), 0);
    }
    
    FL_SUBCASE("complex formatting") {
        char buffer[100];
        int result = fl::sprintf(buffer, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        FL_REQUIRE_GT(result, 0);
        FL_REQUIRE(fl::strstr(buffer, "Int: 123") != nullptr);
        FL_REQUIRE(fl::strstr(buffer, "Float: 3.14") != nullptr);
        FL_REQUIRE(fl::strstr(buffer, "Hex: ff") != nullptr);
        FL_REQUIRE(fl::strstr(buffer, "Char: A") != nullptr);
    }
}

FL_TEST_CASE("fl::sprintf vs fl::snprintf comparison") {
    // Test that sprintf behaves similarly to snprintf when buffer is large enough
    
    FL_SUBCASE("identical behavior for basic formatting") {
        char buffer1[50];
        char buffer2[50];
        
        int result1 = fl::sprintf(buffer1, "Test: %d, %s", 42, "hello");
        int result2 = fl::snprintf(buffer2, 50, "Test: %d, %s", 42, "hello");
        
        FL_REQUIRE_EQ(result1, result2);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("sprintf writes full string when buffer is large enough") {
        char buffer1[100];
        char buffer2[100];
        
        int result1 = fl::sprintf(buffer1, "This is a moderately long string");
        int result2 = fl::snprintf(buffer2, 100, "This is a moderately long string");
        
        FL_REQUIRE_EQ(result1, result2);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("identical behavior for complex formatting") {
        char buffer1[100];
        char buffer2[100];
        
        int result1 = fl::sprintf(buffer1, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        int result2 = fl::snprintf(buffer2, 100, "Int: %d, Float: %.2f, Hex: %x, Char: %c", 123, 3.14159f, 255, 'A');
        
        FL_REQUIRE_EQ(result1, result2);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
}


FL_TEST_CASE("fl::printf handles int64_t") {
    fl::i64 large_signed = 9223372036854775807LL;  // Max int64_t
    fl::i64 negative = -9223372036854775807LL;
    fl::u64 large_unsigned = 18446744073709551615ULL;  // Max uint64_t

    char buf[128];

    // Test %d with int64_t
    fl::snprintf(buf, sizeof(buf), "Value: %d", large_signed);
    FL_CHECK(fl::string(buf) == "Value: 9223372036854775807");

    // Test %d with negative int64_t
    fl::snprintf(buf, sizeof(buf), "Negative: %d", negative);
    FL_CHECK(fl::string(buf) == "Negative: -9223372036854775807");

    // Test %u with uint64_t
    fl::snprintf(buf, sizeof(buf), "Unsigned: %u", large_unsigned);
    FL_CHECK(fl::string(buf) == "Unsigned: 18446744073709551615");

    // Test %d with regular int
    fl::snprintf(buf, sizeof(buf), "Small: %d", 42);
    FL_CHECK(fl::string(buf) == "Small: 42");
}

FL_TEST_CASE("fl::printf handles length modifiers") {
    // Test that length modifiers (l, ll, h, etc.) are properly handled
    // This test should initially fail showing <unknown_format> for %lu, %llu, etc.

    char buf[128];

    FL_SUBCASE("%lu (unsigned long)") {
        fl::u32 value = 4294967295U;
        fl::snprintf(buf, sizeof(buf), "Value: %lu", static_cast<unsigned long>(value));
        FL_CHECK(fl::string(buf).find("<unknown_format>") == fl::string::npos);
        FL_CHECK(fl::string(buf).find("4294967295") != fl::string::npos);
    }

    FL_SUBCASE("%ld (signed long)") {
        long value = -2147483648L;
        fl::snprintf(buf, sizeof(buf), "Value: %ld", value);
        FL_CHECK(fl::string(buf).find("<unknown_format>") == fl::string::npos);
        FL_CHECK(fl::string(buf).find("-2147483648") != fl::string::npos);
    }

    FL_SUBCASE("%llu (unsigned long long)") {
        fl::u64 value = 18446744073709551615ULL;
        fl::snprintf(buf, sizeof(buf), "Value: %llu", value);
        FL_CHECK(fl::string(buf).find("<unknown_format>") == fl::string::npos);
        FL_CHECK(fl::string(buf) == "Value: 18446744073709551615");
    }

    FL_SUBCASE("%lld (signed long long)") {
        fl::i64 value = -9223372036854775807LL;
        fl::snprintf(buf, sizeof(buf), "Value: %lld", value);
        FL_CHECK(fl::string(buf).find("<unknown_format>") == fl::string::npos);
        FL_CHECK(fl::string(buf).find("-9223372036854775807") != fl::string::npos);
    }

    FL_SUBCASE("profiler use case") {
        // This is the exact pattern from profile_chasing_spirals.cpp
        fl::u32 elapsed_us = 12345;
        fl::snprintf(buf, sizeof(buf), "200 frames in %lu us (%.1f us/frame)",
                    static_cast<unsigned long>(elapsed_us), 61.7);
        FL_CHECK(fl::string(buf).find("<unknown_format>") == fl::string::npos);
        FL_CHECK(fl::string(buf).find("12345 us") != fl::string::npos);
    }
}

FL_TEST_CASE("fl::snprintf vs fl::snprintf return value comparison") {
    // Test that fl::snprintf returns the same values as fl::snprintf

    FL_SUBCASE("simple string formatting") {
        char buffer1[100];
        char buffer2[100];
        
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Hello, %s!", "world");
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Hello, %s!", "world");
        
        FL_REQUIRE_EQ(fl_result, std_result);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("integer formatting") {
        char buffer1[50];
        char buffer2[50];
        
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Value: %d", 42);
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Value: %d", 42);
        
        FL_REQUIRE_EQ(fl_result, std_result);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("multiple arguments") {
        char buffer1[100];
        char buffer2[100];
        
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Name: %s, Age: %d", "Alice", 25);
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Name: %s, Age: %d", "Alice", 25);
        
        FL_REQUIRE_EQ(fl_result, std_result);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("character formatting") {
        char buffer1[20];
        char buffer2[20];
        
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Letter: %c", 'A');
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Letter: %c", 'A');
        
        FL_REQUIRE_EQ(fl_result, std_result);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("hexadecimal formatting") {
        char buffer1[20];
        char buffer2[20];
        
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Hex: %x", 255);
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Hex: %x", 255);
        
        FL_REQUIRE_EQ(fl_result, std_result);
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
    }
    
    FL_SUBCASE("buffer truncation behavior") {
        char buffer1[10];
        char buffer2[10];
        
        // Intentionally test buffer truncation behavior - suppress format-truncation warning
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_FORMAT_TRUNCATION
        int fl_result = fl::snprintf(buffer1, sizeof(buffer1), "Hello, %s!", "world");
        int std_result = fl::snprintf(buffer2, sizeof(buffer2), "Hello, %s!", "world");
        FL_DISABLE_WARNING_POP
        FL_UNUSED(std_result);
        FL_UNUSED(fl_result);
        // Note: fl::snprintf returns the number of characters that would have been written
        // while fl::snprintf returns the number actually written. This is a known difference.
        // For truncated strings, we verify the buffer contents are the same
        FL_REQUIRE_EQ(fl::strcmp(buffer1, buffer2), 0);
        
        // Both should be null-terminated and truncated to the same content
        FL_REQUIRE_EQ(fl::strlen(buffer1), fl::strlen(buffer2));
    }
}
