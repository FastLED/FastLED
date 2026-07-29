#include "test.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
// Note: fl/stl/cstdio.h intentionally NOT included directly — workaround
// for zackees/zccache#619 (Windows PCH path-spelling drift). ostream.h
// below pulls cstdio.h in transitively via the same path the PCH uses,
// so all fl::print/println/flush/write_bytes symbols stay in scope.
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/ostream.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

FL_TEST_FILE(FL_FILEPATH) {





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

///////////////////////////////////////////////////////////////////////////////
// Generic "{}" placeholder (issue #3174)
///////////////////////////////////////////////////////////////////////////////

FL_TEST_CASE("fl::snprintf generic {} placeholder with scalar types") {
    char buf[128];

    FL_SUBCASE("signed integer") {
        fl::snprintf(buf, sizeof(buf), "Value: {}", 42);
        FL_CHECK(fl::string(buf) == "Value: 42");
    }

    FL_SUBCASE("negative integer") {
        fl::snprintf(buf, sizeof(buf), "Value: {}", -7);
        FL_CHECK(fl::string(buf) == "Value: -7");
    }

    FL_SUBCASE("unsigned integer") {
        unsigned int value = 4000000000U;
        fl::snprintf(buf, sizeof(buf), "Value: {}", value);
        FL_CHECK(fl::string(buf) == "Value: 4000000000");
    }

    FL_SUBCASE("64-bit integers") {
        fl::i64 signed64 = -9223372036854775807LL;
        fl::u64 unsigned64 = 18446744073709551615ULL;
        fl::snprintf(buf, sizeof(buf), "{} / {}", signed64, unsigned64);
        FL_CHECK(fl::string(buf) == "-9223372036854775807 / 18446744073709551615");
    }

    FL_SUBCASE("bool renders as true/false") {
        fl::snprintf(buf, sizeof(buf), "{} and {}", true, false);
        FL_CHECK(fl::string(buf) == "true and false");
    }

    FL_SUBCASE("char renders as a character, not a number") {
        fl::snprintf(buf, sizeof(buf), "Letter: {}", 'A');
        FL_CHECK(fl::string(buf) == "Letter: A");
    }

    FL_SUBCASE("C string") {
        fl::snprintf(buf, sizeof(buf), "Hello, {}!", "world");
        FL_CHECK(fl::string(buf) == "Hello, world!");
    }

    FL_SUBCASE("const char* variable") {
        const char* name = "Alice";
        fl::snprintf(buf, sizeof(buf), "Name: {}", name);
        FL_CHECK(fl::string(buf) == "Name: Alice");
    }

    FL_SUBCASE("null const char* uses the (null) sentinel") {
        const char* name = nullptr;
        fl::snprintf(buf, sizeof(buf), "Name: {}", name);
        FL_CHECK(fl::string(buf) == "Name: (null)");
    }

    FL_SUBCASE("fl::string needs no .c_str()") {
        fl::string name("Bob");
        fl::snprintf(buf, sizeof(buf), "Name: {}", name);
        FL_CHECK(fl::string(buf) == "Name: Bob");
    }

    FL_SUBCASE("fl::string_view") {
        fl::string_view view("Carol");
        fl::snprintf(buf, sizeof(buf), "Name: {}", view);
        FL_CHECK(fl::string(buf) == "Name: Carol");
    }

    FL_SUBCASE("pointer renders as 0x hex like %p") {
        int value = 5;
        int* ptr = &value;
        fl::snprintf(buf, sizeof(buf), "Ptr: {}", ptr);
        fl::string result(buf);
        FL_CHECK(result.find("Ptr: 0x") == 0);
        // No sentinel and no crash: an actual address was rendered.
        FL_CHECK(result.find("<") == fl::string::npos);
        FL_CHECK(result.size() > 7);
    }

    FL_SUBCASE("null pointer renders as 0x0") {
        // to_hex(0) is deterministic, so assert the exact string. A prefix-only
        // check would also pass on "Ptr: 0xdeadbeef".
        int* p = nullptr;
        fl::snprintf(buf, sizeof(buf), "Ptr: {}", p);
        FL_CHECK(fl::string(buf) == "Ptr: 0x0");
    }

    FL_SUBCASE("float") {
        fl::snprintf(buf, sizeof(buf), "Pi: {}", 3.14159f);
        fl::string result(buf);
        FL_CHECK(result.find("Pi: 3.14") == 0);
    }

    FL_SUBCASE("double") {
        fl::snprintf(buf, sizeof(buf), "Val: {}", 2.5);
        fl::string result(buf);
        FL_CHECK(result.find("Val: 2.5") == 0);
    }
}

FL_TEST_CASE("fl::snprintf generic {} matches the equivalent % specifier") {
    // "{}" is sugar over the existing format_arg machinery: for every supported
    // type it must produce byte-identical output to the specifier a caller
    // would have had to pick by hand. That is the whole ergonomic win.
    char generic[128];
    char explicit_spec[128];

    FL_SUBCASE("int matches %d") {
        fl::snprintf(generic, sizeof(generic), "{}", -12345);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%d", -12345);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("unsigned matches %u") {
        unsigned int value = 4000000000U;
        fl::snprintf(generic, sizeof(generic), "{}", value);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%u", value);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("long long matches %lld") {
        fl::i64 value = -9223372036854775807LL;
        fl::snprintf(generic, sizeof(generic), "{}", value);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%lld", value);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("char matches %c") {
        fl::snprintf(generic, sizeof(generic), "{}", 'Z');
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%c", 'Z');
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("C string matches %s") {
        fl::snprintf(generic, sizeof(generic), "{}", "hello");
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%s", "hello");
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("fl::string matches %s") {
        fl::string value("hello");
        fl::snprintf(generic, sizeof(generic), "{}", value);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%s", value);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("float matches %f") {
        fl::snprintf(generic, sizeof(generic), "{}", 3.14159f);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%f", 3.14159f);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }

    FL_SUBCASE("pointer matches %p") {
        int value = 5;
        int* ptr = &value;
        fl::snprintf(generic, sizeof(generic), "{}", ptr);
        fl::snprintf(explicit_spec, sizeof(explicit_spec), "%p", ptr);
        FL_CHECK(fl::strcmp(generic, explicit_spec) == 0);
    }
}

FL_TEST_CASE("fl::snprintf generic {} formats enums as their underlying integer") {
    // Regression guard. An unscoped enum fails is_integral but implicitly
    // converts to char, so without a dedicated is_enum overload, overload
    // resolution silently picks format_arg_generic(sstream&, char) and this
    // renders "B" (ASCII 66) instead of "66". Silent wrong output, and it
    // contradicts the documented "unsupported types are a compile error".
    char buf[128];

    FL_SUBCASE("unscoped enum with a printable-ASCII value") {
        enum Color { GREEN = 66 };
        fl::snprintf(buf, sizeof(buf), "c={}", GREEN);
        FL_CHECK(fl::string(buf) == "c=66");
    }

    // NOTE: scoped enums (enum class) are deliberately NOT covered. They do
    // not compile through {} -- nor through %d today -- because format_impl
    // compiles its '%' branch for every argument type and format_arg's body
    // needs an implicit conversion a scoped enum does not have. Unchanged by
    // this feature; cast explicitly at the call site.

    FL_SUBCASE("enum matches the %d spelling") {
        enum Big { VALUE = 4242 };
        char viaBrace[64];
        char viaPct[64];
        fl::snprintf(viaBrace, sizeof(viaBrace), "{}", VALUE);
        fl::snprintf(viaPct, sizeof(viaPct), "%d", static_cast<int>(VALUE));
        FL_CHECK(fl::string(viaBrace) == fl::string(viaPct));
        FL_CHECK(fl::string(viaBrace) == "4242");
    }
}

FL_TEST_CASE("fl::snprintf generic {} distinguishes char from small integers") {
    // char renders as a character; fl::u8 / signed char render numerically.
    // Pinning the asymmetry so it cannot drift silently.
    char buf[128];

    FL_SUBCASE("char renders as a character") {
        fl::snprintf(buf, sizeof(buf), "{}", static_cast<char>(66));
        FL_CHECK(fl::string(buf) == "B");
    }

    FL_SUBCASE("fl::u8 renders numerically") {
        fl::snprintf(buf, sizeof(buf), "{}", static_cast<fl::u8>(66));
        FL_CHECK(fl::string(buf) == "66");
    }

    FL_SUBCASE("signed char renders numerically") {
        fl::snprintf(buf, sizeof(buf), "{}", static_cast<signed char>(66));
        FL_CHECK(fl::string(buf) == "66");
    }
}

FL_TEST_CASE("fl::snprintf generic {} argument count behavior") {
    char buf[128];

    FL_SUBCASE("too few arguments emits <missing_arg>") {
        fl::snprintf(buf, sizeof(buf), "a={} b={}", 1);
        FL_CHECK(fl::string(buf) == "a=1 b=<missing_arg>");
    }

    FL_SUBCASE("no arguments at all emits <missing_arg>") {
        fl::snprintf(buf, sizeof(buf), "value: {}");
        FL_CHECK(fl::string(buf) == "value: <missing_arg>");
    }

    FL_SUBCASE("extra arguments are silently ignored") {
        fl::snprintf(buf, sizeof(buf), "a={}", 1, 2, 3);
        FL_CHECK(fl::string(buf) == "a=1");
    }

    FL_SUBCASE("multiple placeholders consume arguments in order") {
        fl::snprintf(buf, sizeof(buf), "{}-{}-{}", 1, "two", 'c');
        FL_CHECK(fl::string(buf) == "1-two-c");
    }
}

FL_TEST_CASE("fl::snprintf brace escaping and literal braces") {
    char buf[128];

    FL_SUBCASE("{{ is a literal {") {
        fl::snprintf(buf, sizeof(buf), "{{}");
        FL_CHECK(fl::string(buf) == "{}");
    }

    FL_SUBCASE("}} is a literal }") {
        fl::snprintf(buf, sizeof(buf), "a}}b");
        FL_CHECK(fl::string(buf) == "a}b");
    }

    FL_SUBCASE("escaped braces around a placeholder") {
        fl::snprintf(buf, sizeof(buf), "{{{}}}", 42);
        FL_CHECK(fl::string(buf) == "{42}");
    }

    FL_SUBCASE("lone { passes through literally") {
        fl::snprintf(buf, sizeof(buf), "a{b", 42);
        FL_CHECK(fl::string(buf) == "a{b");
    }

    FL_SUBCASE("lone } passes through literally") {
        fl::snprintf(buf, sizeof(buf), "a}b");
        FL_CHECK(fl::string(buf) == "a}b");
    }

    FL_SUBCASE("{ at end of string does not overrun") {
        fl::snprintf(buf, sizeof(buf), "value{");
        FL_CHECK(fl::string(buf) == "value{");
    }

    FL_SUBCASE("} at end of string does not overrun") {
        fl::snprintf(buf, sizeof(buf), "value}");
        FL_CHECK(fl::string(buf) == "value}");
    }

    FL_SUBCASE("unmatched { before a real placeholder") {
        fl::snprintf(buf, sizeof(buf), "{x {}", 7);
        FL_CHECK(fl::string(buf) == "{x 7");
    }
}

FL_TEST_CASE("fl::snprintf mixes {} and % specifiers") {
    char buf[128];

    FL_SUBCASE("{} then %d") {
        fl::snprintf(buf, sizeof(buf), "{} and %d", "text", 42);
        FL_CHECK(fl::string(buf) == "text and 42");
    }

    FL_SUBCASE("%d then {}") {
        fl::snprintf(buf, sizeof(buf), "%d and {}", 42, "text");
        FL_CHECK(fl::string(buf) == "42 and text");
    }

    FL_SUBCASE("%% literal alongside {}") {
        fl::snprintf(buf, sizeof(buf), "{}%% done", 50);
        FL_CHECK(fl::string(buf) == "50% done");
    }

    FL_SUBCASE("interleaved with braces and padding") {
        fl::snprintf(buf, sizeof(buf), "{{%05d}} {}", 42, true);
        FL_CHECK(fl::string(buf) == "{00042} true");
    }
}

FL_TEST_CASE("fl::printf generic {} placeholder print path") {
    fl::inject_print_handler(test_helper::capture_print);

    FL_SUBCASE("scalar and string through fl::printf") {
        test_helper::clear_capture();
        fl::printf("Name: {}, Age: {}", "Alice", 25);
        FL_CHECK(test_helper::get_capture() == fl::string("Name: Alice, Age: 25"));
    }

    FL_SUBCASE("fl::string through fl::printf") {
        test_helper::clear_capture();
        fl::string name("Bob");
        fl::printf("Name: {}", name);
        FL_CHECK(test_helper::get_capture() == fl::string("Name: Bob"));
    }

    FL_SUBCASE("missing arg through fl::printf") {
        test_helper::clear_capture();
        fl::printf("Name: {}");
        FL_CHECK(test_helper::get_capture() == fl::string("Name: <missing_arg>"));
    }

    FL_SUBCASE("escaped braces through fl::printf") {
        test_helper::clear_capture();
        fl::printf("{{{}}}", 1);
        FL_CHECK(test_helper::get_capture() == fl::string("{1}"));
    }

    fl::clear_io_handlers();
}

///////////////////////////////////////////////////////////////////////////////
// Documented behavior for mismatched / malformed % format strings.
//
// These sentinels are the CURRENT contract of fl::printf. They are asserted
// here so that any change to them is a deliberate, visible break:
//   <type_error>       argument type cannot satisfy the specifier
//   <string_not_hex>   a C string was passed to %x
//   <unknown_format>   unrecognized or absent specifier character
//   <missing_arg>      no argument remains for the placeholder
//   (null)             a null C string passed to %s
// The overriding guarantee is bounded, deterministic output: no crash and no
// read through an invalid pointer.
///////////////////////////////////////////////////////////////////////////////

FL_TEST_CASE("fl::snprintf documented mismatch behavior") {
    char buf[128];

    FL_SUBCASE("%d with a string argument") {
        fl::snprintf(buf, sizeof(buf), "number: %d", "number");
        FL_CHECK(fl::string(buf) == "number: <type_error>");
    }

    FL_SUBCASE("%i with a string argument") {
        fl::snprintf(buf, sizeof(buf), "number: %i", "number");
        FL_CHECK(fl::string(buf) == "number: <type_error>");
    }

    FL_SUBCASE("%u with a string argument") {
        fl::snprintf(buf, sizeof(buf), "number: %u", "number");
        FL_CHECK(fl::string(buf) == "number: <type_error>");
    }

    FL_SUBCASE("%f with a string argument") {
        fl::snprintf(buf, sizeof(buf), "number: %f", "number");
        FL_CHECK(fl::string(buf) == "number: <type_error>");
    }

    FL_SUBCASE("%x with a string argument") {
        fl::snprintf(buf, sizeof(buf), "hex: %x", "number");
        FL_CHECK(fl::string(buf) == "hex: <string_not_hex>");
    }

    FL_SUBCASE("%s with an integer argument stringifies the integer") {
        // %s routes through sstream, so an int degrades to its decimal text
        // rather than being treated as a pointer to dereference.
        fl::snprintf(buf, sizeof(buf), "text: %s", 42);
        FL_CHECK(fl::string(buf) == "text: 42");
    }

    FL_SUBCASE("%s with a null C string") {
        const char* s = nullptr;
        fl::snprintf(buf, sizeof(buf), "text: %s", s);
        FL_CHECK(fl::string(buf) == "text: (null)");
    }

    FL_SUBCASE("%f with an integer argument") {
        fl::snprintf(buf, sizeof(buf), "float: %f", 42);
        FL_CHECK(fl::string(buf) == "float: <type_error>");
    }

    FL_SUBCASE("%p with a non-pointer argument") {
        fl::snprintf(buf, sizeof(buf), "ptr: %p", 42);
        FL_CHECK(fl::string(buf) == "ptr: <unknown_format>");
    }

    FL_SUBCASE("%p with a null pointer") {
        int* p = nullptr;
        fl::snprintf(buf, sizeof(buf), "ptr: %p", p);
        fl::string result(buf);
        FL_CHECK(result.find("ptr: 0x") == 0);
        FL_CHECK(result.find("<") == fl::string::npos);
    }

    FL_SUBCASE("%d with a non-pointer, non-integer argument") {
        fl::snprintf(buf, sizeof(buf), "value: %d", 1.5f);
        FL_CHECK(fl::string(buf) == "value: <type_error>");
    }
}

FL_TEST_CASE("fl::snprintf documented malformed format behavior") {
    char buf[128];

    FL_SUBCASE("trailing % with no argument") {
        fl::snprintf(buf, sizeof(buf), "done %");
        FL_CHECK(fl::string(buf) == "done <missing_arg>");
    }

    FL_SUBCASE("trailing % with an argument") {
        fl::snprintf(buf, sizeof(buf), "done %", 42);
        FL_CHECK(fl::string(buf) == "done <unknown_format>");
    }

    FL_SUBCASE("truncated specifier at end of string does not read past the NUL") {
        // Regression guard for the parse_format_spec out-of-bounds fix.
        // Before it, parse_format_spec advanced past the type character
        // unconditionally; for a format string that ends mid-specifier the
        // type char IS the NUL terminator, so the pointer stepped past the
        // end of the buffer and the caller's `while (*format)` read heap
        // garbage. These truncated forms walked further past the terminator
        // than a bare trailing '%' does, so they are the stronger guard.
        // Run under --debug (ASAN) for this to be meaningful.
        const char* truncated[] = {"%-", "%5", "%.", "%l", "%ll", "%h",
                                   "%#0", "%.3", "%05"};
        for (const char* fmt : truncated) {
            // Exact-size heap copy so ASAN has a real redzone immediately
            // after the NUL, rather than the slack of a string literal.
            // fl::vector heap-allocates (unlike fl::string, which has inline
            // storage for short strings and would hide the overrun).
            const fl::size len = fl::string(fmt).size();
            fl::vector<char> exact;
            exact.resize(len + 1);
            for (fl::size i = 0; i < len; ++i) {
                exact[i] = fmt[i];
            }
            exact[len] = ' ';

            fl::snprintf(buf, sizeof(buf), exact.data());
            // Bounded, NUL-terminated output is all that is guaranteed; the
            // exact sentinel is not the point of this test.
            FL_CHECK(fl::string(buf).size() < sizeof(buf));

            fl::snprintf(buf, sizeof(buf), exact.data(), 42);
            FL_CHECK(fl::string(buf).size() < sizeof(buf));
        }
    }

    FL_SUBCASE("unknown specifier %q") {
        fl::snprintf(buf, sizeof(buf), "value: %q", 42);
        FL_CHECK(fl::string(buf) == "value: <unknown_format>");
    }

    FL_SUBCASE("unknown specifier %q with no argument") {
        fl::snprintf(buf, sizeof(buf), "value: %q");
        FL_CHECK(fl::string(buf) == "value: <missing_arg>");
    }

    FL_SUBCASE("incomplete width form") {
        fl::snprintf(buf, sizeof(buf), "value: %5");
        FL_CHECK(fl::string(buf) == "value: <missing_arg>");
    }

    FL_SUBCASE("incomplete precision form") {
        fl::snprintf(buf, sizeof(buf), "value: %.2", 1.5f);
        FL_CHECK(fl::string(buf) == "value: <unknown_format>");
    }

    FL_SUBCASE("literal %%") {
        fl::snprintf(buf, sizeof(buf), "100%% sure");
        FL_CHECK(fl::string(buf) == "100% sure");
    }

    FL_SUBCASE("literal %% with no arguments and trailing text") {
        fl::snprintf(buf, sizeof(buf), "%%%%");
        FL_CHECK(fl::string(buf) == "%%");
    }
}

FL_TEST_CASE("fl::snprintf {} respects buffer boundaries") {
    FL_SUBCASE("truncated {} output stays NUL terminated") {
        char buf[8];
        int written = fl::snprintf(buf, sizeof(buf), "value={}", 1234567890);
        FL_CHECK(written == 7);
        FL_CHECK(fl::strlen(buf) == 7);
        FL_CHECK(buf[7] == '\0');
        FL_CHECK(fl::string(buf) == "value=1");
    }

    FL_SUBCASE("truncated string {} output stays NUL terminated") {
        char buf[6];
        fl::string value("a very long string that will not fit");
        int written = fl::snprintf(buf, sizeof(buf), "{}", value);
        FL_CHECK(written == 5);
        FL_CHECK(fl::strlen(buf) == 5);
        FL_CHECK(buf[5] == '\0');
        FL_CHECK(fl::string(buf) == "a ver");
    }

    FL_SUBCASE("size 1 buffer only gets a NUL") {
        char buf[1];
        int written = fl::snprintf(buf, sizeof(buf), "{}", 42);
        FL_CHECK(written == 0);
        FL_CHECK(buf[0] == '\0');
    }

    FL_SUBCASE("null buffer is a no-op") {
        int written = fl::snprintf(static_cast<char*>(nullptr), 16, "{}", 42);
        FL_CHECK(written == 0);
    }

    FL_SUBCASE("fl::sprintf deduces size and truncates safely") {
        char buf[10];
        fl::sprintf(buf, "abc={}", 123456789);
        FL_CHECK(fl::strlen(buf) == 9);
        FL_CHECK(buf[9] == '\0');
        FL_CHECK(fl::string(buf) == "abc=12345");
    }
}

} // FL_TEST_FILE
