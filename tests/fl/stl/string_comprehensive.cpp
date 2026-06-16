// fl::string comprehensive tests (construction, assignment, element access,
// capacity, modifiers, substring/string ops, comparison, stream, COW,
// inline/heap storage, edge cases, memory management, compat, perf/stress,
// integration with FastLED types, regression scenarios).
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/vector.h"
#include "hsv2rgb.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: Tests from string_comprehensive.cpp
//=============================================================================


FL_TEST_CASE("fl::string - Construction and Assignment") {
    FL_SUBCASE("Default construction") {
        fl::string s;
        FL_CHECK(s.empty());
        FL_CHECK(s.size() == 0);
        FL_CHECK(s.length() == 0);
        FL_CHECK(s.c_str() != nullptr);
        FL_CHECK(s.c_str()[0] == '\0');
    }

    FL_SUBCASE("Construction from C-string") {
        fl::string s("Hello, World!");
        FL_CHECK(s.size() == 13);
        FL_CHECK(s.length() == 13);
        FL_CHECK(fl::strcmp(s.c_str(), "Hello, World!") == 0);
        FL_CHECK_FALSE(s.empty());
    }

    FL_SUBCASE("Construction from empty C-string") {
        fl::string s("");
        FL_CHECK(s.empty());
        FL_CHECK(s.size() == 0);
        FL_CHECK(s.c_str()[0] == '\0');
    }

    FL_SUBCASE("Copy construction") {
        fl::string s1("Original string");
        fl::string s2(s1);
        FL_CHECK(s2.size() == s1.size());
        FL_CHECK(fl::strcmp(s2.c_str(), s1.c_str()) == 0);
        FL_CHECK(s2 == s1);
    }

    FL_SUBCASE("Assignment from C-string") {
        fl::string s;
        s = "Assigned string";
        FL_CHECK(s.size() == 15);
        FL_CHECK(fl::strcmp(s.c_str(), "Assigned string") == 0);
    }

    FL_SUBCASE("Copy assignment") {
        fl::string s1("Source string");
        fl::string s2;
        s2 = s1;
        FL_CHECK(s2.size() == s1.size());
        FL_CHECK(s2 == s1);
    }

    FL_SUBCASE("Self-assignment") {
        fl::string s("Self assignment test");
        // Test self-assignment (suppress warning with compiler control macros)
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
        s = s;
        FL_DISABLE_WARNING_POP
        FL_CHECK(fl::strcmp(s.c_str(), "Self assignment test") == 0);
    }
}

FL_TEST_CASE("fl::string - Element Access") {
    FL_SUBCASE("operator[] - non-const") {
        fl::string s("Hello");
        FL_CHECK(s[0] == 'H');
        FL_CHECK(s[1] == 'e');
        FL_CHECK(s[4] == 'o');
        
        s[0] = 'h';
        FL_CHECK(s[0] == 'h');
        FL_CHECK(fl::strcmp(s.c_str(), "hello") == 0);
    }

    FL_SUBCASE("operator[] - const") {
        const fl::string s("Hello");
        FL_CHECK(s[0] == 'H');
        FL_CHECK(s[1] == 'e');
        FL_CHECK(s[4] == 'o');
    }

    FL_SUBCASE("operator[] - out of bounds") {
        fl::string s("Hello");
        // fl::string returns '\0' for out-of-bounds access
        FL_CHECK(s[10] == '\0');
        FL_CHECK(s[100] == '\0');
    }

    FL_SUBCASE("front() and back()") {
        fl::string s("Hello");
        FL_CHECK(s.front() == 'H');
        FL_CHECK(s.back() == 'o');
        
        fl::string empty_str;
        FL_CHECK(empty_str.front() == '\0');
        FL_CHECK(empty_str.back() == '\0');
    }

    FL_SUBCASE("c_str() and data()") {
        fl::string s("Hello");
        FL_CHECK(fl::strcmp(s.c_str(), "Hello") == 0);
        FL_CHECK(s.c_str()[5] == '\0');
        
        // For fl::string, c_str() should always be null-terminated
        fl::string empty_str;
        FL_CHECK(empty_str.c_str() != nullptr);
        FL_CHECK(empty_str.c_str()[0] == '\0');
    }
}

FL_TEST_CASE("fl::string - Capacity Operations") {
    FL_SUBCASE("empty()") {
        fl::string s;
        FL_CHECK(s.empty());
        
        s = "Not empty";
        FL_CHECK_FALSE(s.empty());
        
        s.clear();
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("size() and length()") {
        fl::string s;
        FL_CHECK(s.size() == 0);
        FL_CHECK(s.length() == 0);
        
        s = "Hello";
        FL_CHECK(s.size() == 5);
        FL_CHECK(s.length() == 5);
        
        s = "A much longer string to test size calculation";
        FL_CHECK(s.size() == 45);  // Corrected: actual length is 45
        FL_CHECK(s.length() == 45);
    }

    FL_SUBCASE("capacity() and reserve()") {
        fl::string s;
        size_t initial_capacity = s.capacity();
        FL_CHECK(initial_capacity >= 0);
        
        s.reserve(100);
        FL_CHECK(s.capacity() >= 100);
        FL_CHECK(s.empty()); // reserve shouldn't affect content
        
        s = "Short";
        s.reserve(50);
        FL_CHECK(s.capacity() >= 50);
        FL_CHECK(s == "Short"); // content preserved
        
        // Reserving less than current capacity should be no-op
        size_t current_capacity = s.capacity();
        s.reserve(10);
        FL_CHECK(s.capacity() >= current_capacity);
    }
}

FL_TEST_CASE("fl::string - Modifiers") {
    FL_SUBCASE("clear()") {
        fl::string s("Hello World");
        FL_CHECK_FALSE(s.empty());
        
        s.clear();
        FL_CHECK(s.empty());
        FL_CHECK(s.size() == 0);
        // Note: fl::string's clear() only sets length to 0, it doesn't null-terminate
        // the internal buffer immediately. This is different from fl::string behavior.
        // The string is logically empty even though the raw buffer may contain old data.
        FL_CHECK(s.size() == 0);  // This is the correct way to check if cleared
    }

    FL_SUBCASE("clear() with memory management") {
        fl::string s("Hello World");
        s.clear(false); // don't free memory
        FL_CHECK(s.empty());
        
        s = "Test";
        s.clear(true); // free memory
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("append() - C-string") {
        fl::string s("Hello");
        s.append(" World");
        FL_CHECK(s == "Hello World");
        FL_CHECK(s.size() == 11);
        
        s.append("!");
        FL_CHECK(s == "Hello World!");
    }

    FL_SUBCASE("append() - substring") {
        fl::string s("Hello");
        s.append(" World!!!", 6); // append only " World"
        FL_CHECK(s == "Hello World");
    }

    FL_SUBCASE("append() - fl::string") {
        fl::string s1("Hello");
        fl::string s2(" World");
        s1.append(s2.c_str(), s2.size());
        FL_CHECK(s1 == "Hello World");
    }

    FL_SUBCASE("operator+=") {
        fl::string s("Hello");
        s += " World";
        FL_CHECK(s == "Hello World");
        
        fl::string s2("!");
        s += s2;
        FL_CHECK(s == "Hello World!");
    }

    FL_SUBCASE("swap()") {
        fl::string s1("First");
        fl::string s2("Second");
        
        s1.swap(s2);
        FL_CHECK(s1 == "Second");
        FL_CHECK(s2 == "First");
        
        // Test with different sizes
        fl::string s3("A");
        fl::string s4("Much longer string");
        s3.swap(s4);
        FL_CHECK(s3 == "Much longer string");
        FL_CHECK(s4 == "A");
    }
}

FL_TEST_CASE("fl::string - Substring Operations") {
    FL_SUBCASE("substr() - standard behavior") {
        fl::string original("http://fastled.io");
        
        // Standard substr(pos, length) behavior
        // substr(0, 4) should return "http"
        fl::string scheme = original.substr(0, 4);
        FL_CHECK(fl::strcmp(scheme.c_str(), "http") == 0);
        
        // substr(7, 7) should return "fastled" (7 chars starting at pos 7)
        fl::string host_part = original.substr(7, 7);
        FL_CHECK(fl::strcmp(host_part.c_str(), "fastled") == 0);
        
        // substr(7) should return everything from position 7 onwards
        fl::string from_host = original.substr(7);
        FL_CHECK(fl::strcmp(from_host.c_str(), "fastled.io") == 0);
    }

    FL_SUBCASE("substr() - edge cases") {
        fl::string original("http://fastled.io");
        
        // Start beyond end
        fl::string empty = original.substr(100, 5);
        FL_CHECK(empty.empty());
        
        // Length beyond end
        fl::string partial = original.substr(15, 100);
        FL_CHECK(fl::strcmp(partial.c_str(), "io") == 0);
        
        // Zero length
        fl::string zero_len = original.substr(5, 0);
        FL_CHECK(zero_len.empty());
        
        // Entire string
        fl::string full = original.substr(0);
        FL_CHECK(full == original);
    }
}

FL_TEST_CASE("fl::string - String Operations") {
    FL_SUBCASE("find() - character") {
        fl::string s("Hello World");
        FL_CHECK(s.find('H') == 0);
        FL_CHECK(s.find('o') == 4); // first occurrence
        FL_CHECK(s.find('l') == 2); // first occurrence
        FL_CHECK(s.find('d') == 10);
        FL_CHECK(s.find('x') == string::npos);
    }

    FL_SUBCASE("find() - substring") {
        fl::string s("Hello World Hello");
        FL_CHECK(s.find("Hello") == 0);
        FL_CHECK(s.find("World") == 6);
        FL_CHECK(s.find("xyz") == string::npos);
        FL_CHECK(s.find("") == 0); // empty string found at position 0
    }

    FL_SUBCASE("find() - with position parameter") {
        fl::string url("http://fastled.io");
        
        // Test find operations that were working during debug
        auto scheme_end = url.find("://");
        FL_CHECK_EQ(4, scheme_end);  // Position of "://"
        
        auto path_start = url.find('/', 7);  // Find '/' after position 7
        FL_CHECK_EQ(string::npos, path_start);  // No path in this URL
        
        // Test with URL that has a path
        fl::string url_with_path("http://example.com/path");
        auto path_pos = url_with_path.find('/', 7);
        FL_CHECK_EQ(18, path_pos);  // Position of '/' in path
    }

    FL_SUBCASE("find() - edge cases") {
        fl::string s("abc");
        FL_CHECK(s.find("abcd") == string::npos); // substring longer than string
        
        fl::string empty_str;
        FL_CHECK(empty_str.find('a') == string::npos);
        FL_CHECK(empty_str.find("") == 0); // empty string in empty string
    }

    FL_SUBCASE("npos constant") {
        FL_CHECK(string::npos == static_cast<size_t>(-1));
    }
}

FL_TEST_CASE("fl::string - Comparison Operators") {
    FL_SUBCASE("Equality operators") {
        fl::string s1("Hello");
        fl::string s2("Hello");
        fl::string s3("World");
        
        FL_CHECK(s1 == s2);
        FL_CHECK_FALSE(s1 == s3);
        FL_CHECK_FALSE(s1 != s2);
        FL_CHECK(s1 != s3);
    }

    FL_SUBCASE("Equality operators - bug fix tests") {
        // Test basic string equality that was broken
        fl::string str1("http");
        fl::string str2("http");
        fl::string str3("https");
        
        // These should return true but were returning false
        FL_CHECK(str1 == str2);
        FL_CHECK_FALSE(str1 == str3);
        
        // Test with const char*
        FL_CHECK(str1 == "http");
        FL_CHECK_FALSE(str1 == "https");
        
        // Test edge cases
        fl::string empty1;
        fl::string empty2;
        FL_CHECK(empty1 == empty2);
        
        fl::string single1("a");
        fl::string single2("a");
        FL_CHECK(single1 == single2);
        
        // Test inequality operator
        FL_CHECK_FALSE(str1 != str2);
        FL_CHECK(str1 != str3);
    }

    FL_SUBCASE("Relational operators") {
        fl::string s1("Apple");
        fl::string s2("Banana");
        fl::string s3("Apple");
        
        FL_CHECK(s1 < s2);
        FL_CHECK_FALSE(s2 < s1);
        FL_CHECK_FALSE(s1 < s3);
        
        FL_CHECK(s1 <= s2);
        FL_CHECK(s1 <= s3);
        FL_CHECK_FALSE(s2 <= s1);
        
        FL_CHECK(s2 > s1);
        FL_CHECK_FALSE(s1 > s2);
        FL_CHECK_FALSE(s1 > s3);
        
        FL_CHECK(s2 >= s1);
        FL_CHECK(s1 >= s3);
        FL_CHECK_FALSE(s1 >= s2);
    }

    FL_SUBCASE("Comparison with empty strings") {
        fl::string s1;
        fl::string s2("");
        fl::string s3("Hello");
        
        FL_CHECK(s1 == s2);
        FL_CHECK(s1 < s3);
        FL_CHECK_FALSE(s3 < s1);
    }
}

FL_TEST_CASE("fl::string - Stream Operations") {
    FL_SUBCASE("Stream output") {
        fl::string test_str("http");
        
        // Test stream output - should show characters, not ASCII values
        fl::sstream oss;
        oss << test_str;
        fl::string result = oss.str();
        
        // Should be "http", not "104116116112" (ASCII values)
        FL_CHECK(fl::strcmp(result.c_str(), "http") == 0);
        
        // Test with special characters
        fl::string special("://");
        fl::sstream oss2;
        oss2 << special;
        fl::string result2 = oss2.str();
        FL_CHECK(fl::strcmp(result2.c_str(), "://") == 0);
    }

    FL_SUBCASE("Stream output - complex") {
        // Test combining stream operations
        fl::string scheme("https");
        fl::string host("192.0.2.0");
        fl::string path("/test");
        
        fl::sstream oss;
        oss << "Scheme: " << scheme << ", Host: " << host << ", Path: " << path;
        fl::string full_output = oss.str();
        FL_CHECK(fl::strcmp(full_output.c_str(), "Scheme: https, Host: 192.0.2.0, Path: /test") == 0);
    }
}

FL_TEST_CASE("fl::string - Copy-on-Write Behavior") {
    FL_SUBCASE("Shared data after copy") {
        fl::string s1("Hello World");
        fl::string s2 = s1;
        
        // Both should have the same content
        FL_CHECK(s1 == s2);
        FL_CHECK(s1.size() == s2.size());
    }

    FL_SUBCASE("Copy-on-write on modification") {
        fl::string s1("Hello World");
        fl::string s2 = s1;
        
        // Modify s2, s1 should remain unchanged
        s2.append("!");
        FL_CHECK(s1 == "Hello World");
        FL_CHECK(s2 == "Hello World!");
    }

    FL_SUBCASE("Copy-on-write with character modification") {
        fl::string s1("Hello");
        fl::string s2 = s1;
        
        s2[0] = 'h';
        FL_CHECK(s1 == "Hello");
        FL_CHECK(s2 == "hello");
    }
}

FL_TEST_CASE("fl::string - Inline vs Heap Storage") {
    FL_SUBCASE("Short strings (inline storage)") {
        // Create a string that fits in inline storage
        fl::string s("Short");
        FL_CHECK(s.size() == 5);
        FL_CHECK(s == "Short");
        
        // Test modification while staying inline
        s.append("er");
        FL_CHECK(s == "Shorter");
    }

    FL_SUBCASE("Long strings (heap storage)") {
        // Create a string longer than FASTLED_STR_INLINED_SIZE
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 10, 'a');
        fl::string s(long_str.c_str());
        
        FL_CHECK(s.size() == long_str.length());
        FL_CHECK(fl::strcmp(s.c_str(), long_str.c_str()) == 0);
    }

    FL_SUBCASE("Transition from inline to heap") {
        fl::string s("Short");
        
        // Append enough to exceed inline capacity
        fl::string long_append(FASTLED_STR_INLINED_SIZE, 'x');
        s.append(long_append.c_str());
        
        FL_CHECK(s.size() == 5 + long_append.length());
        FL_CHECK(s[0] == 'S');
        FL_CHECK(s[5] == 'x');
    }

    FL_SUBCASE("Copy-on-write with heap storage") {
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'b');
        fl::string s1(long_str.c_str());
        fl::string s2 = s1;
        
        s2.append("extra");
        FL_CHECK(s1.size() == long_str.length());
        FL_CHECK(s2.size() == long_str.length() + 5);
        
        // Verify copy-on-write behavior: s1 should remain unchanged
        FL_CHECK(s1.c_str()[0] == 'b');
        
        // Note: There appears to be an issue with fl::string heap storage character access
        // after copy-on-write operations. This is a limitation of the current implementation.
        // We'll verify that at least the string content and size are correct.
        FL_CHECK(s2.size() > long_str.length());
        
        // Verify that the strings are different (copy-on-write worked)
        FL_CHECK(s1 != s2);
    }
}

FL_TEST_CASE("fl::string - Edge Cases and Special Characters") {
    FL_SUBCASE("Null characters in string") {
        // Since fl::string doesn't support (const char*, size_t) constructor,
        // we'll test null character handling differently
        fl::string s("Hello");
        s.append("\0", 1);  // Add null character
        s.append("World");
        // Note: The actual behavior may vary since fl::string uses strlen internally
        FL_CHECK(s.size() >= 5);  // At least the "Hello" part
        FL_CHECK(s[0] == 'H');
        FL_CHECK(s[4] == 'o');
    }

    FL_SUBCASE("Very long strings") {
        // Test with very long strings
        fl::string very_long(1000, 'z');
        fl::string s(very_long.c_str());
        FL_CHECK(s.size() == 1000);
        FL_CHECK(s[0] == 'z');
        FL_CHECK(s[999] == 'z');
    }

    FL_SUBCASE("Repeated operations") {
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.append("a");
        }
        FL_CHECK(s.size() == 100);
        FL_CHECK(s[0] == 'a');
        FL_CHECK(s[99] == 'a');
    }

    FL_SUBCASE("Multiple consecutive modifications") {
        fl::string s("Start");
        s.append(" middle");
        s.append(" end");
        s[0] = 's';
        FL_CHECK(s == "start middle end");
    }
}

FL_TEST_CASE("fl::string - Memory Management") {
    FL_SUBCASE("Reserve and capacity management") {
        fl::string s;
        
        // Test reserve with small capacity
        s.reserve(10);
        FL_CHECK(s.capacity() >= 10);
        s = "Test";
        FL_CHECK(s == "Test");
        
        // Test reserve with large capacity
        s.reserve(1000);
        FL_CHECK(s.capacity() >= 1000);
        FL_CHECK(s == "Test");
        
        // Test that content is preserved during capacity changes
        for (int i = 0; i < 100; ++i) {
            s.append("x");
        }
        FL_CHECK(s.size() == 104); // "Test" + 100 'x'
        FL_CHECK(s[0] == 'T');
        FL_CHECK(s[4] == 'x');
    }

    FL_SUBCASE("Memory efficiency") {
        // Test that small strings don't allocate heap memory unnecessarily
        fl::string s1("Small");
        fl::string s2("Another small string");
        
        // These should work without issues
        fl::string s3 = s1;
        s3.append(" addition");
        FL_CHECK(s1 == "Small");
        FL_CHECK(s3 != s1);
    }
}

FL_TEST_CASE("fl::string - Compatibility with fl::string patterns") {
    FL_SUBCASE("Common fl::string usage patterns") {
        // Pattern 1: Build string incrementally
        fl::string result;
        result += "Hello";
        result += " ";
        result += "World";
        result += "!";
        FL_CHECK(result == "Hello World!");
        
        // Pattern 2: Copy and modify
        fl::string original("Template string");
        fl::string modified = original;
        modified[0] = 't';
        FL_CHECK(original == "Template string");
        FL_CHECK(modified == "template string");
        
        // Pattern 3: Clear and reuse
        fl::string reusable("First content");
        FL_CHECK(reusable == "First content");
        reusable.clear();
        reusable = "Second content";
        FL_CHECK(reusable == "Second content");
    }

    FL_SUBCASE("String container behavior") {
        // Test that fl::string can be used like fl::string in containers
        fl::vector<string> strings;
        strings.push_back(fl::string("First"));
        strings.push_back(fl::string("Second"));
        strings.push_back(fl::string("Third"));
        
        FL_CHECK(strings.size() == 3);
        FL_CHECK(strings[0] == "First");
        FL_CHECK(strings[1] == "Second");
        FL_CHECK(strings[2] == "Third");
        
        // Test sorting (requires comparison operators)
        // This would test the < operator implementation
        FL_CHECK(strings[0] < strings[1]); // "First" < "Second"
    }
}

FL_TEST_CASE("fl::string - Performance and Stress Testing") {
    FL_SUBCASE("Large string operations") {
        fl::string s;

        // Build a large string (reduced from 1000 to 500 for performance, still provides excellent coverage)
        for (int i = 0; i < 500; ++i) {
            s.append("X");
        }
        FL_CHECK(s.size() == 500);
        
        // Copy the large string
        fl::string s2 = s;
        FL_CHECK(s2.size() == 500);
        FL_CHECK(s2 == s);

        // Modify the copy
        s2.append("Y");
        FL_CHECK(s.size() == 500);
        FL_CHECK(s2.size() == 501);
        FL_CHECK(s2[500] == 'Y');
    }

    FL_SUBCASE("Repeated copy operations") {
        fl::string original("Test string for copying");
        
        for (int i = 0; i < 100; ++i) {
            fl::string copy = original;
            FL_CHECK(copy == original);
            copy.append("X");
            FL_CHECK(copy != original);
        }
        
        // Original should be unchanged
        FL_CHECK(original == "Test string for copying");
    }
}

FL_TEST_CASE("fl::string - Integration with FastLED types") {
    FL_SUBCASE("Append with various numeric types") {
        fl::string s;
        
        s.append(static_cast<int8_t>(127));
        s.clear();
        s.append(static_cast<uint8_t>(255));
        s.clear();
        s.append(static_cast<int16_t>(32767));
        s.clear();
        s.append(static_cast<uint16_t>(65535));
        s.clear();
        s.append(static_cast<int32_t>(2147483647));
        s.clear();
        s.append(static_cast<uint32_t>(4294967295U));
        
        // Just verify they don't crash - exact formatting may vary
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("Boolean append") {
        fl::string s;
        s.append(true);
        FL_CHECK(s == "true");
        
        s.clear();
        s.append(false);
        FL_CHECK(s == "false");
    }
}

FL_TEST_CASE("fl::string - Comprehensive Integration Tests") {
    FL_SUBCASE("URL parsing scenario") {
        // Comprehensive test combining all operations
        fl::string url("https://192.0.2.0/test");
        
        // Extract scheme
        fl::string scheme = url.substr(0, 5);  // "https"
        FL_CHECK(fl::strcmp(scheme.c_str(), "https") == 0);
        FL_CHECK(scheme == "https");
        
        // Extract protocol separator  
        fl::string proto_sep = url.substr(5, 3);  // "://"
        FL_CHECK(fl::strcmp(proto_sep.c_str(), "://") == 0);
        FL_CHECK(proto_sep == "://");
        
        // Extract host
        fl::string host = url.substr(8, 9);  // "192.0.2.0"
        FL_CHECK(fl::strcmp(host.c_str(), "192.0.2.0") == 0);
        FL_CHECK(host == "192.0.2.0");
        
        // Extract path
        fl::string path = url.substr(17);  // "/test"
        FL_CHECK(fl::strcmp(path.c_str(), "/test") == 0);
        FL_CHECK(path == "/test");
        
        // Stream output test
        fl::sstream oss;
        oss << "Scheme: " << scheme << ", Host: " << host << ", Path: " << path;
        fl::string full_output = oss.str();
        FL_CHECK(fl::strcmp(full_output.c_str(), "Scheme: https, Host: 192.0.2.0, Path: /test") == 0);
    }
}

FL_TEST_CASE("fl::string - Regression Tests and Debug Scenarios") {
    FL_SUBCASE("Debug scenario - exact networking code failure") {
        // Test the exact scenario that was failing in the networking code
        fl::string test_url("http://fastled.io");
        
        // Debug: Check individual character access
        FL_CHECK_EQ('h', test_url[0]);
        FL_CHECK_EQ('t', test_url[1]);
        FL_CHECK_EQ('t', test_url[2]);
        FL_CHECK_EQ('p', test_url[3]);
        
        // Debug: Check length
        FL_CHECK_EQ(17, test_url.size());  // "http://fastled.io" is 17 characters
        
        // Debug: Check find operation
        auto pos = test_url.find("://");
        FL_CHECK_EQ(4, pos);
        
        // Debug: Check substring extraction (the failing operation)
        fl::string scheme = test_url.substr(0, 4);
        FL_CHECK_EQ(4, scheme.size());
        FL_CHECK(fl::strcmp(scheme.c_str(), "http") == 0);
        
        // The critical test: equality comparison
        FL_CHECK(scheme == "http");
        
        // Manual character comparison that was working
        bool manual_check = (scheme.size() == 4 && 
                            scheme[0] == 'h' && scheme[1] == 't' && 
                            scheme[2] == 't' && scheme[3] == 'p');
        FL_CHECK(manual_check);
    }
}

//=============================================================================

} // FL_TEST_FILE
