// Consolidated test suite for fl::string
// Combines tests from: str.cpp, string_comprehensive.cpp, string_memory_bugs.cpp, string_optimization.cpp
// This tests the fl::string implementation in fl/stl/string.h and fl/stl/string.cpp

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/iterator.h"
#include "fl/stl/span.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/thread.h"
#include "fl/stl/unordered_set.h"
#include "fl/stl/vector.h"
#include "hsv2rgb.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;


//=============================================================================
// SECTION: Tests from str.cpp
//=============================================================================

FL_TEST_CASE("Str basic operations") {
    FL_SUBCASE("Construction and assignment") {
        fl::string s1;
        FL_CHECK(s1.size() == 0);
        FL_CHECK(s1.c_str()[0] == '\0');

        fl::string s2("hello");
        FL_CHECK(s2.size() == 5);
        FL_CHECK(fl::strcmp(s2.c_str(), "hello") == 0);

        fl::string s3 = s2;
        FL_CHECK(s3.size() == 5);
        FL_CHECK(fl::strcmp(s3.c_str(), "hello") == 0);

        s1 = "world";
        FL_CHECK(s1.size() == 5);
        FL_CHECK(fl::strcmp(s1.c_str(), "world") == 0);
    }

    FL_SUBCASE("Comparison operators") {
        fl::string s1("hello");
        fl::string s2("hello");
        fl::string s3("world");

        FL_CHECK(s1 == s2);
        FL_CHECK(s1 != s3);
    }

    FL_SUBCASE("Indexing") {
        fl::string s("hello");
        FL_CHECK(s[0] == 'h');
        FL_CHECK(s[4] == 'o');
        FL_CHECK(s[5] == '\0');  // Null terminator
    }

    FL_SUBCASE("Append") {
        fl::string s("hello");
        s.append(" world");
        FL_CHECK(s.size() == 11);
        FL_CHECK(fl::strcmp(s.c_str(), "hello world") == 0);
    }

    FL_SUBCASE("CRGB to Str") {
        CRGB c(255, 0, 0);
        fl::string s = c.toString();
        FL_CHECK_EQ(s, "CRGB(255,0,0)");
    }

    FL_SUBCASE("Copy-on-write behavior") {
        fl::string s1("hello");
        fl::string s2 = s1;
        s2.append(" world");
        FL_CHECK(fl::strcmp(s1.c_str(), "hello") == 0);
        FL_CHECK(fl::strcmp(s2.c_str(), "hello world") == 0);
    }
}


FL_TEST_CASE("Str::reserve") {
    fl::string s;
    s.reserve(10);
    FL_CHECK(s.size() == 0);
    FL_CHECK(s.capacity() >= 10);

    s.reserve(5);
    FL_CHECK(s.size() == 0);
    FL_CHECK(s.capacity() >= 10);

    s.reserve(500);
    FL_CHECK(s.size() == 0);
    FL_CHECK(s.capacity() >= 500);
    // s << "hello";
    s.append("hello");
    FL_CHECK(s.size() == 5);
    FL_CHECK_EQ(s, "hello");
}

FL_TEST_CASE("Str with fl::FixedVector") {
    fl::FixedVector<fl::string, 10> vec;
    vec.push_back(fl::string("hello"));
    vec.push_back(fl::string("world"));

    FL_CHECK(vec.size() == 2);
    FL_CHECK(fl::strcmp(vec[0].c_str(), "hello") == 0);
    FL_CHECK(fl::strcmp(vec[1].c_str(), "world") == 0);
}

FL_TEST_CASE("Str with long strings") {
    const char* long_string = "This is a very long string that exceeds the inline buffer size and should be allocated on the heap";
    fl::string s(long_string);
    FL_CHECK(s.size() == fl::strlen(long_string));
    FL_CHECK(fl::strcmp(s.c_str(), long_string) == 0);

    fl::string s2 = s;
    FL_CHECK(s2.size() == fl::strlen(long_string));
    FL_CHECK(fl::strcmp(s2.c_str(), long_string) == 0);

    s2.append(" with some additional text");
    FL_CHECK(fl::strcmp(s.c_str(), long_string) == 0);  // Original string should remain unchanged
}

FL_TEST_CASE("Str overflowing inline data") {
    FL_SUBCASE("Construction with long string") {
        fl::string long_string(FASTLED_STR_INLINED_SIZE + 10, 'a');  // Create a string longer than the inline buffer
        fl::string s(long_string.c_str());
        FL_CHECK(s.size() == long_string.length());
        FL_CHECK(fl::strcmp(s.c_str(), long_string.c_str()) == 0);
    }

    FL_SUBCASE("Appending to overflow") {
        fl::string s("Short string");
        fl::string append_string(FASTLED_STR_INLINED_SIZE, 'b');  // String to append that will cause overflow
        s.append(append_string.c_str());
        FL_CHECK(s.size() == fl::strlen("Short string") + append_string.length());
        FL_CHECK(s[0] == 'S');
        FL_CHECK(s[s.size() - 1] == 'b');
    }

    FL_SUBCASE("Copy on write with long string") {
        fl::string long_string(FASTLED_STR_INLINED_SIZE + 20, 'c');
        fl::string s1(long_string.c_str());
        fl::string s2 = s1;
        FL_CHECK(s1.size() == s2.size());
        FL_CHECK(fl::strcmp(s1.c_str(), s2.c_str()) == 0);

        s2.append("extra");
        FL_CHECK(s1.size() == long_string.length());
        FL_CHECK(s2.size() == long_string.length() + 5);
        FL_CHECK(fl::strcmp(s1.c_str(), long_string.c_str()) == 0);
        FL_CHECK(s2[s2.size() - 1] == 'a');
    }
}

FL_TEST_CASE("String concatenation operators") {
    FL_SUBCASE("String literal + fl::to_string") {
        // Test the specific case mentioned in the user query
        fl::string val = "string" + fl::to_string(5);
        FL_CHECK(fl::strcmp(val.c_str(), "string5") == 0);
    }

    FL_SUBCASE("fl::to_string + string literal") {
        fl::string val = fl::to_string(10) + " is a number";
        FL_CHECK(fl::strcmp(val.c_str(), "10 is a number") == 0);
    }

    FL_SUBCASE("String literal + fl::string") {
        fl::string str = "world";
        fl::string result = "Hello " + str;
        FL_CHECK(fl::strcmp(result.c_str(), "Hello world") == 0);
    }

    FL_SUBCASE("fl::string + string literal") {
        fl::string str = "Hello";
        fl::string result = str + " world";
        FL_CHECK(fl::strcmp(result.c_str(), "Hello world") == 0);
    }

    FL_SUBCASE("fl::string + fl::string") {
        fl::string str1 = "Hello";
        fl::string str2 = "World";
        fl::string result = str1 + " " + str2;
        FL_CHECK(fl::strcmp(result.c_str(), "Hello World") == 0);
    }

    FL_SUBCASE("Complex concatenation") {
        fl::string result = "Value: " + fl::to_string(42) + " and " + fl::to_string(3.14f);
        // Check that it contains the expected parts rather than exact match
        FL_CHECK(result.find("Value: ") != fl::string::npos);
        FL_CHECK(result.find("42") != fl::string::npos);
        FL_CHECK(result.find("and") != fl::string::npos);
        FL_CHECK(result.find("3.14") != fl::string::npos);
    }

    FL_SUBCASE("Number + string literal") {
        fl::string result = fl::to_string(100) + " percent";
        FL_CHECK(fl::strcmp(result.c_str(), "100 percent") == 0);
    }

    FL_SUBCASE("String literal + number") {
        fl::string result = "Count: " + fl::to_string(7);
        FL_CHECK(fl::strcmp(result.c_str(), "Count: 7") == 0);
    }
}

FL_TEST_CASE("String insert operations") {
    FL_SUBCASE("Insert character at beginning") {
        fl::string s = "world";
        s.insert(0, 1, 'H');
        FL_CHECK_EQ(s, "Hworld");
        FL_CHECK(s.size() == 6);
    }

    FL_SUBCASE("Insert character in middle") {
        fl::string s = "helo";
        s.insert(2, 1, 'l');
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Insert character at end") {
        fl::string s = "hello";
        s.insert(5, 1, '!');
        FL_CHECK_EQ(s, "hello!");
        FL_CHECK(s.size() == 6);
    }

    FL_SUBCASE("Insert multiple characters") {
        fl::string s = "hello";
        s.insert(5, 3, '!');
        FL_CHECK_EQ(s, "hello!!!");
        FL_CHECK(s.size() == 8);
    }

    FL_SUBCASE("Insert c-string") {
        fl::string s = "hello";
        s.insert(5, " world");
        FL_CHECK_EQ(s, "hello world");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Insert c-string at beginning") {
        fl::string s = "world";
        s.insert(0, "hello ");
        FL_CHECK_EQ(s, "hello world");
    }

    FL_SUBCASE("Insert partial c-string") {
        fl::string s = "hello";
        s.insert(5, " wonderful world", 10);
        FL_CHECK_EQ(s, "hello wonderful");
    }

    FL_SUBCASE("Insert fl::string") {
        fl::string s = "hello";
        fl::string insert_str = " world";
        s.insert(5, insert_str);
        FL_CHECK_EQ(s, "hello world");
    }

    FL_SUBCASE("Insert substring of fl::string") {
        fl::string s = "hello";
        fl::string insert_str = "the world";
        s.insert(5, insert_str, 3, 6);  // Insert " world"
        FL_CHECK_EQ(s, "hello world");
    }

    FL_SUBCASE("Insert substring with npos") {
        fl::string s = "hello";
        fl::string insert_str = "the world";
        s.insert(5, insert_str, 3);  // Insert " world" (to end)
        FL_CHECK_EQ(s, "hello world");
    }

    FL_SUBCASE("Insert causing inline to heap transition") {
        fl::string s = "short";
        // Create a long string that will cause heap allocation
        fl::string long_insert(FASTLED_STR_INLINED_SIZE, 'x');
        s.insert(5, long_insert);
        FL_CHECK(s.size() == 5 + FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s[0] == 's');
        FL_CHECK(s[5] == 'x');
    }

    FL_SUBCASE("Insert on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 10, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Insert into s2 should trigger COW
        s2.insert(5, "XXX");

        // s1 should remain unchanged
        FL_CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 10);
        for (fl::size i = 0; i < s1.size(); ++i) {
            FL_CHECK(s1[i] == 'a');
        }

        // s2 should have the insertion
        FL_CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 13);
        FL_CHECK(s2[5] == 'X');
        FL_CHECK(s2[6] == 'X');
        FL_CHECK(s2[7] == 'X');
    }

    FL_SUBCASE("Insert with invalid position clamped") {
        fl::string s = "hello";
        s.insert(100, " world");  // Position beyond end
        FL_CHECK_EQ(s, "hello world");  // Should append at end
    }

    FL_SUBCASE("Insert zero characters") {
        fl::string s = "hello";
        s.insert(2, 0, 'x');
        FL_CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    FL_SUBCASE("Insert empty string") {
        fl::string s = "hello";
        s.insert(2, "");
        FL_CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    // Note: Iterator-based insert tests disabled due to ambiguity issues
    // They can be re-enabled once better type disambiguation is implemented
}

FL_TEST_CASE("String erase operations") {
    FL_SUBCASE("Erase from beginning") {
        fl::string s = "hello world";
        s.erase(0, 6);
        FL_CHECK_EQ(s, "world");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Erase from middle") {
        fl::string s = "hello world";
        s.erase(5, 1);  // Remove the space
        FL_CHECK_EQ(s, "helloworld");
        FL_CHECK(s.size() == 10);
    }

    FL_SUBCASE("Erase to end with npos") {
        fl::string s = "hello world";
        s.erase(5);  // Erase from position 5 to end (default count=npos)
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Erase to end explicit") {
        fl::string s = "hello world";
        s.erase(5, fl::string::npos);
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Erase entire string") {
        fl::string s = "hello";
        s.erase(0);
        FL_CHECK_EQ(s, "");
        FL_CHECK(s.size() == 0);
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("Erase with count larger than remaining") {
        fl::string s = "hello world";
        s.erase(5, 100);  // Count exceeds string length
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Erase zero characters") {
        fl::string s = "hello";
        s.erase(2, 0);
        FL_CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    FL_SUBCASE("Erase with invalid position") {
        fl::string s = "hello";
        s.erase(100, 5);  // Position beyond end
        FL_CHECK_EQ(s, "hello");  // Should remain unchanged (no-op)
    }

    FL_SUBCASE("Erase on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Erase from s2 should trigger COW
        s2.erase(5, 10);

        // s1 should remain unchanged
        FL_CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 20);
        for (fl::size i = 0; i < s1.size(); ++i) {
            FL_CHECK(s1[i] == 'a');
        }

        // s2 should have the erasure
        FL_CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 10);
        for (fl::size i = 0; i < s2.size(); ++i) {
            FL_CHECK(s2[i] == 'a');
        }
    }

    FL_SUBCASE("Erase single character at position") {
        fl::string s = "hello";
        s.erase(1, 1);  // Remove 'e'
        FL_CHECK_EQ(s, "hllo");
        FL_CHECK(s.size() == 4);
    }

    FL_SUBCASE("Iterator-based erase single character") {
        fl::string s = "hello";
        auto it = s.begin() + 1;  // Point to 'e'
        auto result = s.erase(static_cast<char*>(it));
        FL_CHECK_EQ(s, "hllo");
        FL_CHECK(s.size() == 4);
        // Result should point to 'l' (the character after erased 'e')
        FL_CHECK(*result == 'l');
    }

    FL_SUBCASE("Iterator-based erase range") {
        fl::string s = "hello world";
        auto first = s.begin() + 5;  // Point to space
        auto last = s.begin() + 11;  // Point to end
        auto result = s.erase(static_cast<char*>(first), static_cast<char*>(last));
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
        // Result should point to end
        FL_CHECK(result == s.end());
    }

    FL_SUBCASE("Iterator-based erase middle range") {
        fl::string s = "hello world";
        auto first = s.begin() + 2;  // Point to first 'l'
        auto last = s.begin() + 9;   // Point to second 'l' (end of range is exclusive)
        s.erase(static_cast<char*>(first), static_cast<char*>(last));
        FL_CHECK_EQ(s, "held");
        FL_CHECK(s.size() == 4);
    }

    FL_SUBCASE("Iterator-based erase at beginning") {
        fl::string s = "hello";
        auto it = s.begin();
        s.erase(static_cast<char*>(it));
        FL_CHECK_EQ(s, "ello");
        FL_CHECK(s.size() == 4);
    }

    FL_SUBCASE("Iterator-based erase at end-1") {
        fl::string s = "hello";
        auto it = s.end() - 1;  // Point to 'o'
        s.erase(static_cast<char*>(it));
        FL_CHECK_EQ(s, "hell");
        FL_CHECK(s.size() == 4);
    }

    FL_SUBCASE("Erase and verify null termination") {
        fl::string s = "hello world";
        s.erase(5);
        FL_CHECK(s.c_str()[5] == '\0');
        FL_CHECK(fl::strlen(s.c_str()) == s.size());
    }

    FL_SUBCASE("Multiple consecutive erases") {
        fl::string s = "abcdefgh";
        s.erase(2, 2);  // Remove "cd" -> "abefgh"
        FL_CHECK_EQ(s, "abefgh");
        s.erase(2, 2);  // Remove "ef" -> "abgh"
        FL_CHECK_EQ(s, "abgh");
        s.erase(2, 2);  // Remove "gh" -> "ab"
        FL_CHECK_EQ(s, "ab");
        FL_CHECK(s.size() == 2);
    }
}

FL_TEST_CASE("String replace operations") {
    FL_SUBCASE("Replace with shorter string") {
        fl::string s = "hello world";
        s.replace(6, 5, "C++");  // Replace "world" with "C++"
        FL_CHECK_EQ(s, "hello C++");
        FL_CHECK(s.size() == 9);
    }

    FL_SUBCASE("Replace with longer string") {
        fl::string s = "hello";
        s.replace(0, 5, "goodbye");  // Replace "hello" with "goodbye"
        FL_CHECK_EQ(s, "goodbye");
        FL_CHECK(s.size() == 7);
    }

    FL_SUBCASE("Replace with equal length string") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");  // Replace "world" with "there"
        FL_CHECK_EQ(s, "hello there");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Replace at beginning") {
        fl::string s = "hello world";
        s.replace(0, 5, "hi");  // Replace "hello" with "hi"
        FL_CHECK_EQ(s, "hi world");
        FL_CHECK(s.size() == 8);
    }

    FL_SUBCASE("Replace in middle") {
        fl::string s = "hello world";
        s.replace(5, 1, "---");  // Replace space with "---"
        FL_CHECK_EQ(s, "hello---world");
        FL_CHECK(s.size() == 13);
    }

    FL_SUBCASE("Replace to end with npos") {
        fl::string s = "hello world";
        s.replace(6, fl::string::npos, "everyone");  // Replace "world" to end
        FL_CHECK_EQ(s, "hello everyone");
        FL_CHECK(s.size() == 14);
    }

    FL_SUBCASE("Replace entire string") {
        fl::string s = "hello";
        s.replace(0, 5, "goodbye world");
        FL_CHECK_EQ(s, "goodbye world");
        FL_CHECK(s.size() == 13);
    }

    FL_SUBCASE("Replace with empty string (delete)") {
        fl::string s = "hello world";
        s.replace(5, 6, "");  // Remove " world"
        FL_CHECK_EQ(s, "hello");
        FL_CHECK(s.size() == 5);
    }

    FL_SUBCASE("Replace with c-string") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");
        FL_CHECK_EQ(s, "hello there");
    }

    FL_SUBCASE("Replace with partial c-string") {
        fl::string s = "hello world";
        s.replace(6, 5, "wonderful place", 9);  // Use first 9 chars
        FL_CHECK_EQ(s, "hello wonderful");
        FL_CHECK(s.size() == 15);
    }

    FL_SUBCASE("Replace with fl::string") {
        fl::string s = "hello world";
        fl::string replacement = "everyone";
        s.replace(6, 5, replacement);
        FL_CHECK_EQ(s, "hello everyone");
    }

    FL_SUBCASE("Replace with substring of fl::string") {
        fl::string s = "hello world";
        fl::string source = "the wonderful place";
        s.replace(6, 5, source, 4, 9);  // Use "wonderful"
        FL_CHECK_EQ(s, "hello wonderful");
    }

    FL_SUBCASE("Replace with substring using npos") {
        fl::string s = "hello world";
        fl::string source = "the wonderful";
        s.replace(6, 5, source, 4);  // Use "wonderful" to end
        FL_CHECK_EQ(s, "hello wonderful");
    }

    FL_SUBCASE("Replace with repeated character") {
        fl::string s = "hello world";
        s.replace(6, 5, 3, '!');  // Replace "world" with "!!!"
        FL_CHECK_EQ(s, "hello !!!");
        FL_CHECK(s.size() == 9);
    }

    FL_SUBCASE("Replace with zero characters") {
        fl::string s = "hello world";
        s.replace(6, 5, 0, 'x');  // Replace "world" with nothing
        FL_CHECK_EQ(s, "hello ");
        FL_CHECK(s.size() == 6);
    }

    FL_SUBCASE("Replace with count larger than string") {
        fl::string s = "hello world";
        s.replace(6, 100, "everyone");  // Count exceeds string length
        FL_CHECK_EQ(s, "hello everyone");
    }

    FL_SUBCASE("Replace causing heap growth") {
        fl::string s = "hello";
        // Create a long replacement string
        fl::string long_replacement(FASTLED_STR_INLINED_SIZE, 'x');
        s.replace(0, 5, long_replacement);
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s[0] == 'x');
        FL_CHECK(s[FASTLED_STR_INLINED_SIZE - 1] == 'x');
    }

    FL_SUBCASE("Replace on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Replace in s2 should trigger COW
        s2.replace(5, 10, "XXX");

        // s1 should remain unchanged
        FL_CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 20);
        for (fl::size i = 0; i < s1.size(); ++i) {
            FL_CHECK(s1[i] == 'a');
        }

        // s2 should have the replacement
        FL_CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 13);  // -10 + 3
        FL_CHECK(s2[5] == 'X');
        FL_CHECK(s2[6] == 'X');
        FL_CHECK(s2[7] == 'X');
        FL_CHECK(s2[8] == 'a');
    }

    FL_SUBCASE("Replace with invalid position") {
        fl::string s = "hello world";
        s.replace(100, 5, "test");  // Position beyond end
        FL_CHECK_EQ(s, "hello world");  // Should remain unchanged (no-op)
    }

    FL_SUBCASE("Replace zero count at position") {
        fl::string s = "hello world";
        s.replace(5, 0, "XXX");  // Replace nothing, effectively insert
        FL_CHECK_EQ(s, "helloXXX world");
        FL_CHECK(s.size() == 14);
    }

    FL_SUBCASE("Replace and verify null termination") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");
        FL_CHECK(s.c_str()[11] == '\0');
        FL_CHECK(fl::strlen(s.c_str()) == s.size());
    }

    FL_SUBCASE("Multiple consecutive replaces") {
        fl::string s = "hello world";
        s.replace(0, 5, "hi");      // "hi world"
        FL_CHECK_EQ(s, "hi world");
        s.replace(3, 5, "there");   // "hi there"
        FL_CHECK_EQ(s, "hi there");
        s.replace(0, 2, "hello");   // "hello there"
        FL_CHECK_EQ(s, "hello there");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Replace shrinking inline string") {
        fl::string s = "hello world";
        s.replace(6, 5, "!");  // Replace "world" with "!"
        FL_CHECK_EQ(s, "hello !");
        FL_CHECK(s.size() == 7);
    }

    FL_SUBCASE("Replace growing inline string") {
        fl::string s = "hi";
        s.replace(0, 2, "hello world");
        FL_CHECK_EQ(s, "hello world");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Replace with same content") {
        fl::string s = "hello world";
        s.replace(0, 5, "hello");
        FL_CHECK_EQ(s, "hello world");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Replace at end position") {
        fl::string s = "hello";
        s.replace(5, 0, " world");  // Insert at end
        FL_CHECK_EQ(s, "hello world");
        FL_CHECK(s.size() == 11);
    }

    FL_SUBCASE("Replace with null pointer (should erase)") {
        fl::string s = "hello world";
        s.replace(6, 5, static_cast<const char*>(nullptr));
        FL_CHECK_EQ(s, "hello ");
        FL_CHECK(s.size() == 6);
    }

    // Note: Iterator-based replace tests disabled due to ambiguity issues
    // (same as insert/erase iterator variants)
}

FL_TEST_CASE("String rfind operations") {
    FL_SUBCASE("rfind character in string") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind('o') == 7);  // Last 'o' in "world"
        FL_CHECK(s.rfind('l') == 9);  // Last 'l' in "world"
        FL_CHECK(s.rfind('h') == 0);  // First and only 'h'
        FL_CHECK(s.rfind('x') == fl::string::npos);  // Not found
    }

    FL_SUBCASE("rfind character from specific position") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind('o', 10) == 7);  // Search from pos 10, find 'o' at 7
        FL_CHECK(s.rfind('o', 7) == 7);   // Search from pos 7, find 'o' at 7
        FL_CHECK(s.rfind('o', 6) == 4);   // Search from pos 6, find 'o' at 4 in "hello"
        FL_CHECK(s.rfind('l', 3) == 3);   // Find 'l' at position 3
        FL_CHECK(s.rfind('l', 2) == 2);   // Find 'l' at position 2
        FL_CHECK(s.rfind('h', 0) == 0);   // Find 'h' at position 0
    }

    FL_SUBCASE("rfind character with pos beyond string length") {
        fl::string s = "hello";
        FL_CHECK(s.rfind('o', 100) == 4);  // Should search from end
        FL_CHECK(s.rfind('h', 1000) == 0); // Should find 'h' at start
    }

    FL_SUBCASE("rfind character in empty string") {
        fl::string s = "";
        FL_CHECK(s.rfind('x') == fl::string::npos);
        FL_CHECK(s.rfind('x', 0) == fl::string::npos);
    }

    FL_SUBCASE("rfind substring") {
        fl::string s = "hello world hello";
        FL_CHECK(s.rfind("hello") == 12);  // Last occurrence
        FL_CHECK(s.rfind("world") == 6);   // Only occurrence
        FL_CHECK(s.rfind("o w") == 4);     // Substring spanning words
        FL_CHECK(s.rfind("xyz") == fl::string::npos);  // Not found
    }

    FL_SUBCASE("rfind substring with position") {
        fl::string s = "hello world hello";
        FL_CHECK(s.rfind("hello", 15) == 12);  // Find last "hello"
        FL_CHECK(s.rfind("hello", 11) == 0);   // Find first "hello" (search before last one)
        FL_CHECK(s.rfind("world", 10) == 6);   // Find "world"
        FL_CHECK(s.rfind("world", 5) == fl::string::npos);  // Can't find before position 6
    }

    FL_SUBCASE("rfind with c-string and count") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind("world", fl::string::npos, 5) == 6);  // Full match
        FL_CHECK(s.rfind("world", fl::string::npos, 3) == 6);  // Match "wor"
        FL_CHECK(s.rfind("world", 10, 3) == 6);  // Match "wor" from position 10
        FL_CHECK(s.rfind("hello", 10, 3) == 0);  // Match "hel"
    }

    FL_SUBCASE("rfind empty string") {
        fl::string s = "hello";
        FL_CHECK(s.rfind("") == 5);  // Empty string matches at end
        FL_CHECK(s.rfind("", 2) == 2);  // Empty string matches at position
        FL_CHECK(s.rfind("", 10) == 5);  // Position beyond end returns length
        FL_CHECK(s.rfind("", fl::string::npos, 0) == 5);  // Empty with count=0
    }

    FL_SUBCASE("rfind fl::string") {
        fl::string s = "hello world hello";
        fl::string pattern1 = "hello";
        fl::string pattern2 = "world";
        fl::string pattern3 = "xyz";

        FL_CHECK(s.rfind(pattern1) == 12);  // Last "hello"
        FL_CHECK(s.rfind(pattern2) == 6);   // "world"
        FL_CHECK(s.rfind(pattern3) == fl::string::npos);  // Not found
    }

    FL_SUBCASE("rfind fl::string with position") {
        fl::string s = "hello world hello";
        fl::string pattern = "hello";

        FL_CHECK(s.rfind(pattern, 15) == 12);  // Last occurrence
        FL_CHECK(s.rfind(pattern, 11) == 0);   // First occurrence
        FL_CHECK(s.rfind(pattern, 5) == 0);    // Before second occurrence
    }

    FL_SUBCASE("rfind at beginning of string") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind("hello") == 0);
        FL_CHECK(s.rfind('h') == 0);
    }

    FL_SUBCASE("rfind at end of string") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind('d') == 10);
        FL_CHECK(s.rfind("world") == 6);
        FL_CHECK(s.rfind("ld") == 9);
    }

    FL_SUBCASE("rfind single character string") {
        fl::string s = "hello";
        FL_CHECK(s.rfind("o") == 4);
        FL_CHECK(s.rfind("h") == 0);
    }

    FL_SUBCASE("rfind with repeated pattern") {
        fl::string s = "aaaaaaa";
        FL_CHECK(s.rfind('a') == 6);  // Last 'a'
        FL_CHECK(s.rfind('a', 3) == 3);  // 'a' at position 3
        FL_CHECK(s.rfind("aa") == 5);  // Last occurrence of "aa"
        FL_CHECK(s.rfind("aaa") == 4);  // Last occurrence of "aaa"
    }

    FL_SUBCASE("rfind substring longer than string") {
        fl::string s = "hi";
        FL_CHECK(s.rfind("hello") == fl::string::npos);
        FL_CHECK(s.rfind("hello world") == fl::string::npos);
    }

    FL_SUBCASE("rfind on inline string") {
        fl::string s = "short";
        FL_CHECK(s.rfind('o') == 2);
        FL_CHECK(s.rfind("ort") == 2);
        FL_CHECK(s.rfind('s') == 0);
    }

    FL_SUBCASE("rfind on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'a');
        s.replace(5, 1, "b");  // Put a 'b' at position 5
        s.replace(s.size() - 5, 1, "b");  // Put a 'b' near the end

        FL_CHECK(s.rfind('b') == s.size() - 5);  // Find last 'b'
        FL_CHECK(s.rfind('b', s.size() - 6) == 5);  // Find first 'b'
        FL_CHECK(s.rfind('a') == s.size() - 1);  // Last 'a'
    }

    FL_SUBCASE("rfind with overlapping matches") {
        fl::string s = "aaaa";
        FL_CHECK(s.rfind("aa") == 2);  // Last possible match at position 2
        FL_CHECK(s.rfind("aa", 1) == 1);  // Match at position 1
        FL_CHECK(s.rfind("aa", 0) == 0);  // Match at position 0
    }

    FL_SUBCASE("rfind case sensitive") {
        fl::string s = "Hello World";
        FL_CHECK(s.rfind('h') == fl::string::npos);  // Lowercase 'h' not found
        FL_CHECK(s.rfind('H') == 0);  // Uppercase 'H' found
        FL_CHECK(s.rfind("hello") == fl::string::npos);  // Case mismatch
        FL_CHECK(s.rfind("Hello") == 0);  // Exact match
    }

    FL_SUBCASE("rfind with null terminator in count") {
        fl::string s = "hello\0world";  // Contains embedded null
        // Note: string is actually "hello" due to constructor behavior
        FL_CHECK(s.size() == 5);  // Only "hello" is stored
        FL_CHECK(s.rfind("hello") == 0);
    }

    FL_SUBCASE("rfind comparison with find") {
        fl::string s = "test";
        // For strings with unique characters, rfind should equal find
        FL_CHECK(s.rfind('t') == 3);  // Last 't'
        FL_CHECK(s.find('t') == 0);   // First 't'

        fl::string s2 = "unique";
        FL_CHECK(s2.rfind('u') == 4);  // Last 'u'
        FL_CHECK(s2.find('u') == 0);   // First 'u'
    }

    FL_SUBCASE("rfind with position 0") {
        fl::string s = "hello world";
        FL_CHECK(s.rfind('h', 0) == 0);  // Can find at position 0
        FL_CHECK(s.rfind("hello", 0) == 0);  // Can find at position 0
        FL_CHECK(s.rfind('e', 0) == fl::string::npos);  // 'e' is after position 0
        FL_CHECK(s.rfind("world", 0) == fl::string::npos);  // "world" is after position 0
    }

    FL_SUBCASE("rfind performance - multiple occurrences") {
        fl::string s = "the quick brown fox jumps over the lazy dog";
        FL_CHECK(s.rfind("the") == 31);  // Last occurrence of "the" in "the lazy"
        FL_CHECK(s.rfind("the", 30) == 0);  // First occurrence of "the" (before position 31)
        FL_CHECK(s.rfind(' ') == 39);  // Last space (before "dog")
        FL_CHECK(s.rfind('o') == 41);  // Last 'o' in "dog"
    }
}

FL_TEST_CASE("String find_first_of operations") {
    FL_SUBCASE("find_first_of with character set") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of("aeiou") == 1);  // 'e' at position 1
        FL_CHECK(s.find_first_of("xyz") == fl::string::npos);  // None found
        FL_CHECK(s.find_first_of("wo") == 4);  // 'o' in "hello" at position 4
    }

    FL_SUBCASE("find_first_of single character") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of('o') == 4);  // First 'o'
        FL_CHECK(s.find_first_of('h') == 0);  // At beginning
        FL_CHECK(s.find_first_of('d') == 10);  // At end
        FL_CHECK(s.find_first_of('x') == fl::string::npos);  // Not found
    }

    FL_SUBCASE("find_first_of with position offset") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of("aeiou", 0) == 1);  // 'e' from start
        FL_CHECK(s.find_first_of("aeiou", 2) == 4);  // 'o' at position 4
        FL_CHECK(s.find_first_of("aeiou", 5) == 7);  // 'o' in "world" at position 7
        FL_CHECK(s.find_first_of("aeiou", 8) == fl::string::npos);  // No vowels after 'o'
    }

    FL_SUBCASE("find_first_of beyond string length") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_of("aeiou", 100) == fl::string::npos);
        FL_CHECK(s.find_first_of('o', 100) == fl::string::npos);
    }

    FL_SUBCASE("find_first_of in empty string") {
        fl::string s = "";
        FL_CHECK(s.find_first_of("abc") == fl::string::npos);
        FL_CHECK(s.find_first_of('x') == fl::string::npos);
        FL_CHECK(s.find_first_of("") == fl::string::npos);
    }

    FL_SUBCASE("find_first_of with empty set") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_of("") == fl::string::npos);
        FL_CHECK(s.find_first_of("", 0, 0) == fl::string::npos);
    }

    FL_SUBCASE("find_first_of with null pointer") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_of(static_cast<const char*>(nullptr)) == fl::string::npos);
    }

    FL_SUBCASE("find_first_of with counted string") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of("aeiou", 0, 3) == 1);  // Search for "aei", find 'e'
        FL_CHECK(s.find_first_of("xyz", 0, 2) == fl::string::npos);  // Search for "xy"
        FL_CHECK(s.find_first_of("world", 0, 1) == 6);  // Search for "w", found at position 6
    }

    FL_SUBCASE("find_first_of with fl::string") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";
        fl::string consonants = "bcdfghjklmnpqrstvwxyz";
        fl::string digits = "0123456789";

        FL_CHECK(s.find_first_of(vowels) == 1);  // 'e' at position 1
        FL_CHECK(s.find_first_of(consonants) == 0);  // 'h' at position 0
        FL_CHECK(s.find_first_of(digits) == fl::string::npos);  // No digits
    }

    FL_SUBCASE("find_first_of with fl::string and position") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";

        FL_CHECK(s.find_first_of(vowels, 0) == 1);  // 'e' from start
        FL_CHECK(s.find_first_of(vowels, 2) == 4);  // 'o' at position 4
        FL_CHECK(s.find_first_of(vowels, 5) == 7);  // 'o' in "world"
    }

    FL_SUBCASE("find_first_of whitespace") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of(" \t\n") == 5);  // Space at position 5

        fl::string s2 = "no-spaces-here";
        FL_CHECK(s2.find_first_of(" \t\n") == fl::string::npos);
    }

    FL_SUBCASE("find_first_of digits in mixed string") {
        fl::string s = "abc123def456";
        FL_CHECK(s.find_first_of("0123456789") == 3);  // '1' at position 3
        FL_CHECK(s.find_first_of("0123456789", 4) == 4);  // '2' at position 4
        FL_CHECK(s.find_first_of("0123456789", 6) == 9);  // '4' at position 9
    }

    FL_SUBCASE("find_first_of punctuation") {
        fl::string s = "hello, world!";
        FL_CHECK(s.find_first_of(",.;:!?") == 5);  // ',' at position 5
        FL_CHECK(s.find_first_of(",.;:!?", 6) == 12);  // '!' at position 12
    }

    FL_SUBCASE("find_first_of case sensitive") {
        fl::string s = "Hello World";
        FL_CHECK(s.find_first_of("h") == fl::string::npos);  // Lowercase 'h' not found
        FL_CHECK(s.find_first_of("H") == 0);  // Uppercase 'H' found
        FL_CHECK(s.find_first_of("hH") == 0);  // Either case, finds 'H'
    }

    FL_SUBCASE("find_first_of with repeated characters in set") {
        fl::string s = "hello world";
        FL_CHECK(s.find_first_of("ooo") == 4);  // Duplicates in set don't matter
        FL_CHECK(s.find_first_of("llllll") == 2);  // First 'l' at position 2
    }

    FL_SUBCASE("find_first_of all characters match") {
        fl::string s = "aaaa";
        FL_CHECK(s.find_first_of("a") == 0);  // First match at start
        FL_CHECK(s.find_first_of("a", 1) == 1);  // From position 1
        FL_CHECK(s.find_first_of("a", 3) == 3);  // From position 3
    }

    FL_SUBCASE("find_first_of no characters match") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_of("xyz") == fl::string::npos);
        FL_CHECK(s.find_first_of("123") == fl::string::npos);
        FL_CHECK(s.find_first_of("XYZ") == fl::string::npos);
    }

    FL_SUBCASE("find_first_of at string boundaries") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_of("h") == 0);  // First character
        FL_CHECK(s.find_first_of("o") == 4);  // Last character
        FL_CHECK(s.find_first_of("ho") == 0);  // Either boundary
    }

    FL_SUBCASE("find_first_of with special characters") {
        fl::string s = "path/to/file.txt";
        FL_CHECK(s.find_first_of("/\\") == 4);  // First '/' or '\'
        FL_CHECK(s.find_first_of(".") == 12);  // First '.'
        FL_CHECK(s.find_first_of("/.", 5) == 7);  // Next '/' or '.' after position 5
    }

    FL_SUBCASE("find_first_of for tokenization") {
        fl::string s = "word1,word2;word3:word4";
        FL_CHECK(s.find_first_of(",;:") == 5);  // First delimiter ','
        FL_CHECK(s.find_first_of(",;:", 6) == 11);  // Second delimiter ';'
        FL_CHECK(s.find_first_of(",;:", 12) == 17);  // Third delimiter ':'
    }

    FL_SUBCASE("find_first_of on inline string") {
        fl::string s = "short";
        FL_CHECK(s.find_first_of("aeiou") == 2);  // 'o' at position 2
        FL_CHECK(s.find_first_of("xyz") == fl::string::npos);
    }

    FL_SUBCASE("find_first_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "a");  // Put an 'a' at position 10
        s.replace(50, 1, "b");  // Put a 'b' at position 50

        FL_CHECK(s.find_first_of("ab") == 10);  // Find 'a' at position 10
        FL_CHECK(s.find_first_of("ab", 11) == 50);  // Find 'b' at position 50
        FL_CHECK(s.find_first_of("ab", 51) == fl::string::npos);  // No more matches
    }

    FL_SUBCASE("find_first_of comparison with find") {
        fl::string s = "hello world";
        // For single character, find_first_of should equal find
        FL_CHECK(s.find_first_of('o') == s.find('o'));
        FL_CHECK(s.find_first_of('h') == s.find('h'));
        FL_CHECK(s.find_first_of('x') == s.find('x'));
    }

    FL_SUBCASE("find_first_of from each position") {
        fl::string s = "abcdef";
        FL_CHECK(s.find_first_of("cf", 0) == 2);  // 'c' at position 2
        FL_CHECK(s.find_first_of("cf", 1) == 2);  // 'c' at position 2
        FL_CHECK(s.find_first_of("cf", 2) == 2);  // 'c' at position 2
        FL_CHECK(s.find_first_of("cf", 3) == 5);  // 'f' at position 5
        FL_CHECK(s.find_first_of("cf", 4) == 5);  // 'f' at position 5
        FL_CHECK(s.find_first_of("cf", 5) == 5);  // 'f' at position 5
        FL_CHECK(s.find_first_of("cf", 6) == fl::string::npos);  // Past end
    }

    FL_SUBCASE("find_first_of with entire alphabet") {
        fl::string s = "123 hello";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        FL_CHECK(s.find_first_of(alphabet) == 4);  // 'h' at position 4
    }

    FL_SUBCASE("find_first_of realistic use case - trimming") {
        fl::string s = "   hello";
        FL_CHECK(s.find_first_of("abcdefghijklmnopqrstuvwxyz") == 3);  // First letter at 3

        fl::string s2 = "\t\n  text";
        FL_CHECK(s2.find_first_of("abcdefghijklmnopqrstuvwxyz") == 4);  // First letter at 4
    }
}

FL_TEST_CASE("String find_last_of operations") {
    FL_SUBCASE("find_last_of with character set") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of("aeiou") == 7);  // Last vowel 'o' in "world" at position 7
        FL_CHECK(s.find_last_of("xyz") == fl::string::npos);  // None found
        FL_CHECK(s.find_last_of("hl") == 9);  // Last 'l' at position 9
    }

    FL_SUBCASE("find_last_of single character") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of('o') == 7);  // Last 'o' in "world"
        FL_CHECK(s.find_last_of('h') == 0);  // Only 'h' at beginning
        FL_CHECK(s.find_last_of('d') == 10);  // 'd' at end
        FL_CHECK(s.find_last_of('x') == fl::string::npos);  // Not found
    }

    FL_SUBCASE("find_last_of with position limit") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of("aeiou") == 7);  // Last 'o' from end
        FL_CHECK(s.find_last_of("aeiou", 6) == 4);  // Last 'o' in "hello" at position 4
        FL_CHECK(s.find_last_of("aeiou", 3) == 1);  // 'e' at position 1
        FL_CHECK(s.find_last_of("aeiou", 0) == fl::string::npos);  // No vowels at position 0
    }

    FL_SUBCASE("find_last_of with pos beyond string length") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of("aeiou", 100) == 4);  // Should search from end, find 'o'
        FL_CHECK(s.find_last_of('o', 1000) == 4);  // Should find 'o' at position 4
    }

    FL_SUBCASE("find_last_of with pos = npos") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of("aeiou", fl::string::npos) == 7);  // Search from end
        FL_CHECK(s.find_last_of('l', fl::string::npos) == 9);  // Last 'l'
    }

    FL_SUBCASE("find_last_of in empty string") {
        fl::string s = "";
        FL_CHECK(s.find_last_of("abc") == fl::string::npos);
        FL_CHECK(s.find_last_of('x') == fl::string::npos);
        FL_CHECK(s.find_last_of("") == fl::string::npos);
    }

    FL_SUBCASE("find_last_of with empty set") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of("") == fl::string::npos);
        FL_CHECK(s.find_last_of("", fl::string::npos, 0) == fl::string::npos);
    }

    FL_SUBCASE("find_last_of with null pointer") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of(static_cast<const char*>(nullptr)) == fl::string::npos);
    }

    FL_SUBCASE("find_last_of with counted string") {
        fl::string s = "hello world";
        // With "aeiou" and count=3, search for "aei" (first 3 chars)
        // In "hello world", 'e' at position 1 is the last occurrence of any char from "aei"
        FL_CHECK(s.find_last_of("aeiou", fl::string::npos, 3) == 1);  // Search for "aei", last is 'e' at position 1
        FL_CHECK(s.find_last_of("world", fl::string::npos, 1) == 6);  // Search for "w", found at position 6
    }

    FL_SUBCASE("find_last_of with fl::string") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";
        fl::string consonants = "bcdfghjklmnpqrstvwxyz";
        fl::string digits = "0123456789";

        FL_CHECK(s.find_last_of(vowels) == 7);  // Last 'o' at position 7
        FL_CHECK(s.find_last_of(consonants) == 10);  // Last 'd' at position 10
        FL_CHECK(s.find_last_of(digits) == fl::string::npos);  // No digits
    }

    FL_SUBCASE("find_last_of with fl::string and position") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";

        FL_CHECK(s.find_last_of(vowels) == 7);  // Last 'o' from end
        FL_CHECK(s.find_last_of(vowels, 6) == 4);  // Last vowel at or before position 6 is 'o' at 4
        FL_CHECK(s.find_last_of(vowels, 3) == 1);  // Last vowel at or before position 3 is 'e' at 1
    }

    FL_SUBCASE("find_last_of whitespace") {
        fl::string s = "hello world test";
        FL_CHECK(s.find_last_of(" \t\n") == 11);  // Last space at position 11

        fl::string s2 = "no-spaces-here";
        FL_CHECK(s2.find_last_of(" \t\n") == fl::string::npos);
    }

    FL_SUBCASE("find_last_of digits in mixed string") {
        fl::string s = "abc123def456";
        FL_CHECK(s.find_last_of("0123456789") == 11);  // Last digit '6' at position 11
        FL_CHECK(s.find_last_of("0123456789", 8) == 5);  // Last digit at or before 8 is '3' at position 5
        FL_CHECK(s.find_last_of("0123456789", 2) == fl::string::npos);  // No digits before position 3
    }

    FL_SUBCASE("find_last_of punctuation") {
        fl::string s = "hello, world!";
        FL_CHECK(s.find_last_of(",.;:!?") == 12);  // Last '!' at position 12
        FL_CHECK(s.find_last_of(",.;:!?", 11) == 5);  // ',' at position 5
    }

    FL_SUBCASE("find_last_of case sensitive") {
        fl::string s = "Hello World";
        FL_CHECK(s.find_last_of("h") == fl::string::npos);  // Lowercase 'h' not found
        FL_CHECK(s.find_last_of("H") == 0);  // Uppercase 'H' found
        FL_CHECK(s.find_last_of("hH") == 0);  // Either case, finds 'H'
    }

    FL_SUBCASE("find_last_of with repeated characters in set") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of("ooo") == 7);  // Duplicates in set don't matter
        FL_CHECK(s.find_last_of("llllll") == 9);  // Last 'l' at position 9
    }

    FL_SUBCASE("find_last_of all characters match") {
        fl::string s = "aaaa";
        FL_CHECK(s.find_last_of("a") == 3);  // Last match at end
        FL_CHECK(s.find_last_of("a", 2) == 2);  // From position 2
        FL_CHECK(s.find_last_of("a", 0) == 0);  // From position 0
    }

    FL_SUBCASE("find_last_of no characters match") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of("xyz") == fl::string::npos);
        FL_CHECK(s.find_last_of("123") == fl::string::npos);
        FL_CHECK(s.find_last_of("XYZ") == fl::string::npos);
    }

    FL_SUBCASE("find_last_of at string boundaries") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of("h") == 0);  // First character (also last occurrence)
        FL_CHECK(s.find_last_of("o") == 4);  // Last character
        FL_CHECK(s.find_last_of("ho") == 4);  // Last occurrence is 'o'
    }

    FL_SUBCASE("find_last_of with special characters") {
        fl::string s = "path/to/file.txt";
        FL_CHECK(s.find_last_of("/\\") == 7);  // Last '/' at position 7
        FL_CHECK(s.find_last_of(".") == 12);  // Last '.' at position 12
        FL_CHECK(s.find_last_of("/.") == 12);  // Last '/' or '.' is '.' at position 12
    }

    FL_SUBCASE("find_last_of for reverse tokenization") {
        fl::string s = "word1,word2;word3:word4";
        FL_CHECK(s.find_last_of(",;:") == 17);  // Last delimiter ':' at position 17
        FL_CHECK(s.find_last_of(",;:", 16) == 11);  // Previous delimiter ';' at position 11
        FL_CHECK(s.find_last_of(",;:", 10) == 5);  // First delimiter ',' at position 5
    }

    FL_SUBCASE("find_last_of on inline string") {
        fl::string s = "short";
        FL_CHECK(s.find_last_of("aeiou") == 2);  // Last (and only) vowel 'o' at position 2
        FL_CHECK(s.find_last_of("xyz") == fl::string::npos);
    }

    FL_SUBCASE("find_last_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "a");  // Put an 'a' at position 10
        s.replace(50, 1, "b");  // Put a 'b' at position 50

        FL_CHECK(s.find_last_of("ab") == 50);  // Last match is 'b' at position 50
        FL_CHECK(s.find_last_of("ab", 49) == 10);  // Before position 50, 'a' at position 10
        FL_CHECK(s.find_last_of("ab", 9) == fl::string::npos);  // No matches before position 10
    }

    FL_SUBCASE("find_last_of comparison with rfind") {
        fl::string s = "hello world";
        // For single character, find_last_of should equal rfind
        FL_CHECK(s.find_last_of('o') == s.rfind('o'));
        FL_CHECK(s.find_last_of('h') == s.rfind('h'));
        FL_CHECK(s.find_last_of('l') == s.rfind('l'));
        FL_CHECK(s.find_last_of('x') == s.rfind('x'));
    }

    FL_SUBCASE("find_last_of from each position") {
        fl::string s = "abcdef";
        FL_CHECK(s.find_last_of("cf", 5) == 5);  // 'f' at position 5
        FL_CHECK(s.find_last_of("cf", 4) == 2);  // 'c' at position 2
        FL_CHECK(s.find_last_of("cf", 3) == 2);  // 'c' at position 2
        FL_CHECK(s.find_last_of("cf", 2) == 2);  // 'c' at position 2
        FL_CHECK(s.find_last_of("cf", 1) == fl::string::npos);  // No match at or before position 1
        FL_CHECK(s.find_last_of("cf", 0) == fl::string::npos);  // No match at position 0
    }

    FL_SUBCASE("find_last_of with entire alphabet") {
        fl::string s = "123 hello 456";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        FL_CHECK(s.find_last_of(alphabet) == 8);  // Last letter 'o' at position 8
    }

    FL_SUBCASE("find_last_of realistic use case - trailing whitespace") {
        fl::string s = "hello   ";
        FL_CHECK(s.find_last_of("abcdefghijklmnopqrstuvwxyz") == 4);  // Last letter 'o' at position 4

        fl::string s2 = "text\t\n  ";
        FL_CHECK(s2.find_last_of("abcdefghijklmnopqrstuvwxyz") == 3);  // Last letter 't' at position 3
    }

    FL_SUBCASE("find_last_of with overlapping character sets") {
        fl::string s = "hello123world456";
        FL_CHECK(s.find_last_of("0123456789") == 15);  // Last digit '6'
        FL_CHECK(s.find_last_of("abcdefghijklmnopqrstuvwxyz") == 12);  // Last letter 'd'
        FL_CHECK(s.find_last_of("0123456789abcdefghijklmnopqrstuvwxyz") == 15);  // Last alphanumeric
    }

    FL_SUBCASE("find_last_of at position 0") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_of('h', 0) == 0);  // Can find at position 0
        FL_CHECK(s.find_last_of("h", 0) == 0);  // Can find at position 0
        FL_CHECK(s.find_last_of('e', 0) == fl::string::npos);  // 'e' is after position 0
        FL_CHECK(s.find_last_of("world", 0) == fl::string::npos);  // No characters from "world" at position 0
    }

    FL_SUBCASE("find_last_of with multiple occurrences") {
        fl::string s = "the quick brown fox jumps over the lazy dog";
        // Positions: "the quick brown fox jumps over the lazy dog"
        //             0123456789...
        // Last 'o' in "dog" is at position 41
        // Last space before "dog" is at position 39
        // Last occurrence of 't', 'h', or 'e' - 'e' in "the" at position 33
        FL_CHECK(s.find_last_of("aeiou") == 41);  // Last vowel 'o' in "dog"
        FL_CHECK(s.find_last_of(" ") == 39);  // Last space (before "dog")
        FL_CHECK(s.find_last_of("the") == 33);  // Last 'e' in "the lazy" at position 33
    }

    FL_SUBCASE("find_last_of single character string") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_of("o") == 4);
        FL_CHECK(s.find_last_of("h") == 0);
        FL_CHECK(s.find_last_of("l") == 3);
    }

    FL_SUBCASE("find_last_of with repeated pattern") {
        fl::string s = "aaaaaaa";
        FL_CHECK(s.find_last_of('a') == 6);  // Last 'a'
        FL_CHECK(s.find_last_of('a', 3) == 3);  // 'a' at position 3
        FL_CHECK(s.find_last_of('a', 0) == 0);  // 'a' at position 0
    }

    FL_SUBCASE("find_last_of for file extension detection") {
        fl::string s = "file.backup.txt";
        FL_CHECK(s.find_last_of(".") == 11);  // Last '.' before extension
        fl::size ext_pos = s.find_last_of(".");
        FL_CHECK(s.substr(ext_pos + 1) == "txt");  // Extract extension
    }

    FL_SUBCASE("find_last_of for path separator") {
        fl::string s = "C:\\path\\to\\file.txt";
        // String is: "C:\path\to\file.txt" - backslashes are interpreted
        // Actually the backslash escapes: C:\p\t\f.txt with special chars
        // Better test: just use forward slashes or raw positions
        // Positions: C(0) :(1) \(2) p(3) a(4) t(5) h(6) \(7) t(8) o(9) \(10)...
        FL_CHECK(s.find_last_of("\\/") == 10);  // Last separator at position 10
    }

    FL_SUBCASE("find_last_of comparison find_first_of") {
        fl::string s = "test string";
        fl::string charset = "st";
        // find_first_of finds first occurrence of any character from set
        // find_last_of finds last occurrence of any character from set
        // "test string" = positions: t(0)e(1)s(2)t(3) (4)s(5)t(6)r(7)i(8)n(9)g(10)
        FL_CHECK(s.find_first_of(charset) == 0);  // First 't' at position 0
        FL_CHECK(s.find_last_of(charset) == 6);  // Last 't' at position 6
    }
}

FL_TEST_CASE("String find_first_not_of operations") {
    FL_SUBCASE("find_first_not_of single character") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_first_not_of('a') == 3);  // First 'b' at position 3
        FL_CHECK(s.find_first_not_of('b') == 0);  // First 'a' at position 0
        FL_CHECK(s.find_first_not_of('x') == 0);  // First char (no match with 'x')
    }

    FL_SUBCASE("find_first_not_of with character set") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_first_not_of("ab") == 6);  // First 'c' at position 6
        FL_CHECK(s.find_first_not_of("abc") == fl::string::npos);  // All chars are in set
        FL_CHECK(s.find_first_not_of("xyz") == 0);  // First char not in set
    }

    FL_SUBCASE("find_first_not_of for trimming whitespace") {
        fl::string s = "   hello world";
        FL_CHECK(s.find_first_not_of(" ") == 3);  // First non-space at position 3
        FL_CHECK(s.find_first_not_of(" \t\n\r") == 3);  // First non-whitespace

        fl::string s2 = "\t\n  text";
        FL_CHECK(s2.find_first_not_of(" \t\n\r") == 4);  // First non-whitespace at position 4
    }

    FL_SUBCASE("find_first_not_of with position offset") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_first_not_of("a", 0) == 3);  // From start, first non-'a'
        FL_CHECK(s.find_first_not_of("a", 3) == 3);  // From position 3, first non-'a' is at 3
        FL_CHECK(s.find_first_not_of("b", 3) == 6);  // From position 3, first non-'b' is 'c' at 6
        FL_CHECK(s.find_first_not_of("c", 6) == fl::string::npos);  // From position 6, all are 'c'
    }

    FL_SUBCASE("find_first_not_of beyond string length") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_not_of("xyz", 100) == fl::string::npos);
        FL_CHECK(s.find_first_not_of('x', 100) == fl::string::npos);
    }

    FL_SUBCASE("find_first_not_of in empty string") {
        fl::string s = "";
        FL_CHECK(s.find_first_not_of("abc") == fl::string::npos);
        FL_CHECK(s.find_first_not_of('x') == fl::string::npos);
        FL_CHECK(s.find_first_not_of("") == fl::string::npos);
    }

    FL_SUBCASE("find_first_not_of with empty set") {
        fl::string s = "hello";
        // Empty set means every character is "not in the set"
        FL_CHECK(s.find_first_not_of("") == 0);  // First char
        FL_CHECK(s.find_first_not_of("", 0, 0) == 0);  // First char
        FL_CHECK(s.find_first_not_of("", 2) == 2);  // From position 2
    }

    FL_SUBCASE("find_first_not_of with null pointer") {
        fl::string s = "hello";
        // Null pointer means every character is "not in the set"
        FL_CHECK(s.find_first_not_of(static_cast<const char*>(nullptr)) == 0);
        FL_CHECK(s.find_first_not_of(static_cast<const char*>(nullptr), 2) == 2);
    }

    FL_SUBCASE("find_first_not_of with counted string") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_first_not_of("abc", 0, 2) == 6);  // Search for NOT "ab", find 'c' at position 6
        FL_CHECK(s.find_first_not_of("abc", 0, 1) == 3);  // Search for NOT "a", find 'b' at position 3
        FL_CHECK(s.find_first_not_of("xyz", 0, 2) == 0);  // Search for NOT "xy", find 'a' at position 0
    }

    FL_SUBCASE("find_first_not_of with fl::string") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";
        fl::string letters = "abcdefghijklmnopqrstuvwxyz";
        fl::string punct = ",.;:!?";

        FL_CHECK(s.find_first_not_of(digits) == 3);  // First letter 'a' at position 3
        FL_CHECK(s.find_first_not_of(letters) == 0);  // First digit '1' at position 0
        FL_CHECK(s.find_first_not_of(punct) == 0);  // First char '1' not punctuation
    }

    FL_SUBCASE("find_first_not_of with fl::string and position") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";

        FL_CHECK(s.find_first_not_of(digits, 0) == 3);  // First non-digit from start
        FL_CHECK(s.find_first_not_of(digits, 3) == 3);  // First non-digit from position 3
        FL_CHECK(s.find_first_not_of(digits, 4) == 4);  // 'b' at position 4
        FL_CHECK(s.find_first_not_of(digits, 6) == fl::string::npos);  // All digits from position 6
    }

    FL_SUBCASE("find_first_not_of for parsing digits") {
        fl::string s = "123abc";
        FL_CHECK(s.find_first_not_of("0123456789") == 3);  // First non-digit 'a'

        fl::string s2 = "999";
        FL_CHECK(s2.find_first_not_of("0123456789") == fl::string::npos);  // All digits
    }

    FL_SUBCASE("find_first_not_of for alphanumeric detection") {
        fl::string s = "hello_world";
        FL_CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == 5);  // '_' at position 5

        fl::string s2 = "abc123";
        FL_CHECK(s2.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    FL_SUBCASE("find_first_not_of case sensitive") {
        fl::string s = "Hello World";
        FL_CHECK(s.find_first_not_of("hello") == 0);  // 'H' not in lowercase set
        FL_CHECK(s.find_first_not_of("HELLO") == 1);  // 'e' not in uppercase set
        FL_CHECK(s.find_first_not_of("HELOelo") == 5);  // Space at position 5
    }

    FL_SUBCASE("find_first_not_of with repeated characters in set") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_first_not_of("aaa") == 3);  // Duplicates don't matter, first non-'a'
        FL_CHECK(s.find_first_not_of("ababab") == 6);  // First non-'a' or 'b' is 'c'
    }

    FL_SUBCASE("find_first_not_of all characters match") {
        fl::string s = "aaaa";
        FL_CHECK(s.find_first_not_of("a") == fl::string::npos);  // All are 'a'
        FL_CHECK(s.find_first_not_of("a", 0) == fl::string::npos);
        FL_CHECK(s.find_first_not_of("a", 2) == fl::string::npos);
    }

    FL_SUBCASE("find_first_not_of no characters match") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_not_of("xyz") == 0);  // First char not in set
        FL_CHECK(s.find_first_not_of("123") == 0);
        FL_CHECK(s.find_first_not_of("XYZ") == 0);
    }

    FL_SUBCASE("find_first_not_of at string boundaries") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_not_of("h") == 1);  // First non-'h' is 'e'
        FL_CHECK(s.find_first_not_of("hel") == 4);  // First not 'h','e','l' is 'o' at position 4
        FL_CHECK(s.find_first_not_of("helo") == fl::string::npos);  // All chars are in set
    }

    FL_SUBCASE("find_first_not_of with special characters") {
        fl::string s = "///path/to/file";
        FL_CHECK(s.find_first_not_of("/") == 3);  // First non-'/' is 'p' at position 3

        fl::string s2 = "...file.txt";
        FL_CHECK(s2.find_first_not_of(".") == 3);  // First non-'.' is 'f' at position 3
    }

    FL_SUBCASE("find_first_not_of for tokenization") {
        fl::string s = "   word1   word2";
        fl::size first_non_space = s.find_first_not_of(" ");
        FL_CHECK(first_non_space == 3);  // 'w' at position 3

        fl::size next_space = s.find_first_of(" ", first_non_space);
        FL_CHECK(next_space == 8);  // Space after "word1"

        fl::size next_word = s.find_first_not_of(" ", next_space);
        FL_CHECK(next_word == 11);  // 'w' of "word2"
    }

    FL_SUBCASE("find_first_not_of on inline string") {
        fl::string s = "   text";
        FL_CHECK(s.find_first_not_of(" ") == 3);
        FL_CHECK(s.find_first_not_of(" \t") == 3);
    }

    FL_SUBCASE("find_first_not_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "y");  // Put a 'y' at position 10
        s.replace(50, 1, "z");  // Put a 'z' at position 50

        FL_CHECK(s.find_first_not_of("x") == 10);  // First non-'x' is 'y' at position 10
        FL_CHECK(s.find_first_not_of("x", 11) == 50);  // Next non-'x' is 'z' at position 50
        FL_CHECK(s.find_first_not_of("xyz") == fl::string::npos);  // All are x, y, or z
    }

    FL_SUBCASE("find_first_not_of from each position") {
        fl::string s = "aaabbb";
        FL_CHECK(s.find_first_not_of("a", 0) == 3);  // First non-'a' from start
        FL_CHECK(s.find_first_not_of("a", 1) == 3);  // Still position 3
        FL_CHECK(s.find_first_not_of("a", 2) == 3);  // Still position 3
        FL_CHECK(s.find_first_not_of("a", 3) == 3);  // 'b' at position 3
        FL_CHECK(s.find_first_not_of("a", 4) == 4);  // 'b' at position 4
        FL_CHECK(s.find_first_not_of("b", 3) == fl::string::npos);  // All 'b' from position 3
    }

    FL_SUBCASE("find_first_not_of realistic use case - leading whitespace") {
        fl::string s1 = "   hello";
        FL_CHECK(s1.find_first_not_of(" \t\n\r") == 3);

        fl::string s2 = "\t\n  hello";
        FL_CHECK(s2.find_first_not_of(" \t\n\r") == 4);

        fl::string s3 = "hello";
        FL_CHECK(s3.find_first_not_of(" \t\n\r") == 0);  // No leading whitespace

        fl::string s4 = "    ";
        FL_CHECK(s4.find_first_not_of(" \t\n\r") == fl::string::npos);  // All whitespace
    }

    FL_SUBCASE("find_first_not_of realistic use case - parsing numbers") {
        fl::string s = "0000123";
        FL_CHECK(s.find_first_not_of("0") == 4);  // First non-zero digit at position 4

        fl::string s2 = "00000";
        FL_CHECK(s2.find_first_not_of("0") == fl::string::npos);  // All zeros
    }

    FL_SUBCASE("find_first_not_of realistic use case - validation") {
        fl::string s1 = "12345";
        FL_CHECK(s1.find_first_not_of("0123456789") == fl::string::npos);  // All digits (valid)

        fl::string s2 = "123a5";
        FL_CHECK(s2.find_first_not_of("0123456789") == 3);  // Invalid char 'a' at position 3
    }

    FL_SUBCASE("find_first_not_of with entire alphabet") {
        fl::string s = "123abc";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        FL_CHECK(s.find_first_not_of(alphabet) == 0);  // First non-letter '1' at position 0
        FL_CHECK(s.find_first_not_of(alphabet, 3) == fl::string::npos);  // All letters from position 3
    }

    FL_SUBCASE("find_first_not_of with position at string end") {
        fl::string s = "hello";
        FL_CHECK(s.find_first_not_of("xyz", 5) == fl::string::npos);  // Position at end
        FL_CHECK(s.find_first_not_of("xyz", 4) == 4);  // 'o' not in "xyz"
    }

    FL_SUBCASE("find_first_not_of comparison with find_first_of") {
        fl::string s = "aaabbbccc";
        // find_first_of finds first char that IS in set
        // find_first_not_of finds first char that is NOT in set
        FL_CHECK(s.find_first_of("bc") == 3);  // First 'b' at position 3
        FL_CHECK(s.find_first_not_of("ab") == 6);  // First non-'a' or 'b' is 'c' at position 6
    }

    FL_SUBCASE("find_first_not_of single character repeated") {
        fl::string s = "aaaaaaa";
        FL_CHECK(s.find_first_not_of('a') == fl::string::npos);  // All 'a'
        FL_CHECK(s.find_first_not_of('b') == 0);  // First char not 'b'
    }

    FL_SUBCASE("find_first_not_of mixed alphanumeric") {
        fl::string s = "abc123def456";
        FL_CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == 3);  // First digit '1'
        FL_CHECK(s.find_first_not_of("0123456789") == 0);  // First letter 'a'
        FL_CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    FL_SUBCASE("find_first_not_of for prefix detection") {
        fl::string s = "0x1A2B";  // Hex number with prefix
        FL_CHECK(s.find_first_not_of("0") == 1);  // 'x' at position 1
        FL_CHECK(s.find_first_not_of("0x", 0) == 2);  // First non-prefix char '1' at position 2
    }

    FL_SUBCASE("find_first_not_of multiple character types") {
        fl::string s = "!!!hello";
        FL_CHECK(s.find_first_not_of("!") == 3);  // First letter at position 3

        fl::string s2 = "$$$100";
        FL_CHECK(s2.find_first_not_of("$") == 3);  // First digit at position 3
    }

    FL_SUBCASE("find_first_not_of with zero count") {
        fl::string s = "hello";
        // Count 0 means empty set, so every character is "not in the set"
        FL_CHECK(s.find_first_not_of("xyz", 0, 0) == 0);  // First char
        FL_CHECK(s.find_first_not_of("xyz", 2, 0) == 2);  // From position 2
    }

    FL_SUBCASE("find_first_not_of for comment detection") {
        fl::string s = "### This is a comment";
        // String positions: #(0) #(1) #(2) space(3) T(4)...
        FL_CHECK(s.find_first_not_of("#") == 3);  // First non-'#' is space at position 3
        FL_CHECK(s.find_first_not_of("# ") == 4);  // First non-'#' or space is 'T' at position 4
    }

    FL_SUBCASE("find_first_not_of comprehensive trim test") {
        fl::string s1 = "   \t\n  hello world  \t\n   ";
        fl::size start = s1.find_first_not_of(" \t\n\r");
        // String: space(0) space(1) space(2) tab(3) newline(4) space(5) space(6) h(7)
        FL_CHECK(start == 7);  // 'h' at position 7

        fl::string s2 = "hello";
        FL_CHECK(s2.find_first_not_of(" \t\n\r") == 0);  // No trimming needed
    }

    FL_SUBCASE("find_first_not_of versus operator==") {
        fl::string s = "aaa";
        // All characters are 'a', so first not 'a' is npos
        FL_CHECK(s.find_first_not_of("a") == fl::string::npos);
        // Confirms all characters match the set

        fl::string s2 = "aab";
        FL_CHECK(s2.find_first_not_of("a") == 2);  // 'b' at position 2
    }
}

FL_TEST_CASE("String find_last_not_of operations") {
    FL_SUBCASE("find_last_not_of single character") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of('c') == 5);  // Last 'b' at position 5
        FL_CHECK(s.find_last_not_of('a') == 8);  // Last 'c' at position 8
        FL_CHECK(s.find_last_not_of('x') == 8);  // Last char (no match with 'x')
    }

    FL_SUBCASE("find_last_not_of with character set") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of("bc") == 2);  // Last 'a' at position 2
        FL_CHECK(s.find_last_not_of("abc") == fl::string::npos);  // All chars are in set
        FL_CHECK(s.find_last_not_of("xyz") == 8);  // Last char not in set
    }

    FL_SUBCASE("find_last_not_of for trimming trailing whitespace") {
        fl::string s = "hello world   ";
        FL_CHECK(s.find_last_not_of(" ") == 10);  // Last non-space 'd' at position 10
        FL_CHECK(s.find_last_not_of(" \t\n\r") == 10);  // Last non-whitespace

        fl::string s2 = "text\t\n  ";
        FL_CHECK(s2.find_last_not_of(" \t\n\r") == 3);  // Last non-whitespace 't' at position 3
    }

    FL_SUBCASE("find_last_not_of with position limit") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of("c") == 5);  // From end, last non-'c'
        FL_CHECK(s.find_last_not_of("c", 5) == 5);  // From position 5, last non-'c' is at 5
        FL_CHECK(s.find_last_not_of("c", 4) == 4);  // From position 4, last non-'c' is at 4
        FL_CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // From position 2, all are 'a'
    }

    FL_SUBCASE("find_last_not_of with pos beyond string length") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_not_of("xyz", 100) == 4);  // Should search from end, find 'o'
        FL_CHECK(s.find_last_not_of('x', 1000) == 4);  // Should find 'o' at position 4
    }

    FL_SUBCASE("find_last_not_of with pos = npos") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_not_of(" ", fl::string::npos) == 10);  // Search from end, last non-space 'd'
        FL_CHECK(s.find_last_not_of('d', fl::string::npos) == 9);  // Last non-'d' is 'l'
    }

    FL_SUBCASE("find_last_not_of in empty string") {
        fl::string s = "";
        FL_CHECK(s.find_last_not_of("abc") == fl::string::npos);
        FL_CHECK(s.find_last_not_of('x') == fl::string::npos);
        FL_CHECK(s.find_last_not_of("") == fl::string::npos);
    }

    FL_SUBCASE("find_last_not_of with empty set") {
        fl::string s = "hello";
        // Empty set means every character is "not in the set"
        FL_CHECK(s.find_last_not_of("") == 4);  // Last char
        FL_CHECK(s.find_last_not_of("", fl::string::npos, 0) == 4);  // Last char
        FL_CHECK(s.find_last_not_of("", 2) == 2);  // From position 2
    }

    FL_SUBCASE("find_last_not_of with null pointer") {
        fl::string s = "hello";
        // Null pointer means every character is "not in the set"
        FL_CHECK(s.find_last_not_of(static_cast<const char*>(nullptr)) == 4);
        FL_CHECK(s.find_last_not_of(static_cast<const char*>(nullptr), 2) == 2);
    }

    FL_SUBCASE("find_last_not_of with counted string") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of("abc", fl::string::npos, 2) == 8);  // Search for NOT "ab", find 'c' at position 8
        FL_CHECK(s.find_last_not_of("abc", fl::string::npos, 1) == 8);  // Search for NOT "a", find 'c' at position 8
        FL_CHECK(s.find_last_not_of("xyz", fl::string::npos, 2) == 8);  // Search for NOT "xy", find last char
    }

    FL_SUBCASE("find_last_not_of with fl::string") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";
        fl::string letters = "abcdefghijklmnopqrstuvwxyz";
        fl::string punct = ",.;:!?";

        FL_CHECK(s.find_last_not_of(digits) == 5);  // Last letter 'c' at position 5
        FL_CHECK(s.find_last_not_of(letters) == 8);  // Last digit '6' at position 8
        FL_CHECK(s.find_last_not_of(punct) == 8);  // Last char not punctuation
    }

    FL_SUBCASE("find_last_not_of with fl::string and position") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";

        FL_CHECK(s.find_last_not_of(digits) == 5);  // Last non-digit from end
        FL_CHECK(s.find_last_not_of(digits, 5) == 5);  // Last non-digit at or before position 5
        FL_CHECK(s.find_last_not_of(digits, 4) == 4);  // 'b' at position 4
        FL_CHECK(s.find_last_not_of(digits, 2) == fl::string::npos);  // All digits before and at position 2
    }

    FL_SUBCASE("find_last_not_of for trailing zeros") {
        fl::string s = "1230000";
        FL_CHECK(s.find_last_not_of("0") == 2);  // Last non-zero digit '3' at position 2

        fl::string s2 = "00000";
        FL_CHECK(s2.find_last_not_of("0") == fl::string::npos);  // All zeros
    }

    FL_SUBCASE("find_last_not_of for validation") {
        fl::string s1 = "12345";
        FL_CHECK(s1.find_last_not_of("0123456789") == fl::string::npos);  // All digits (valid)

        fl::string s2 = "123a5";
        FL_CHECK(s2.find_last_not_of("0123456789") == 3);  // Invalid char 'a' at position 3 is last non-digit
    }

    FL_SUBCASE("find_last_not_of case sensitive") {
        fl::string s = "Hello World";
        // String: H(0) e(1) l(2) l(3) o(4) space(5) W(6) o(7) r(8) l(9) d(10)
        FL_CHECK(s.find_last_not_of("world") == 6);  // 'W' not in lowercase set (case sensitive)
        FL_CHECK(s.find_last_not_of("WORLD") == 10);  // 'd' not in uppercase set (case sensitive)
        FL_CHECK(s.find_last_not_of("WORLDorld") == 5);  // Space at position 5
    }

    FL_SUBCASE("find_last_not_of with repeated characters in set") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of("ccc") == 5);  // Duplicates don't matter, last non-'c'
        FL_CHECK(s.find_last_not_of("bcbcbc") == 2);  // Last non-'b' or 'c' is 'a'
    }

    FL_SUBCASE("find_last_not_of all characters match") {
        fl::string s = "aaaa";
        FL_CHECK(s.find_last_not_of("a") == fl::string::npos);  // All are 'a'
        FL_CHECK(s.find_last_not_of("a", 3) == fl::string::npos);
        FL_CHECK(s.find_last_not_of("a", 1) == fl::string::npos);
    }

    FL_SUBCASE("find_last_not_of no characters match") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_not_of("xyz") == 4);  // Last char not in set
        FL_CHECK(s.find_last_not_of("123") == 4);
        FL_CHECK(s.find_last_not_of("XYZ") == 4);
    }

    FL_SUBCASE("find_last_not_of at string boundaries") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_not_of("o") == 3);  // Last non-'o' is 'l'
        FL_CHECK(s.find_last_not_of("elo") == 0);  // Last not 'e','l','o' is 'h' at position 0
        FL_CHECK(s.find_last_not_of("helo") == fl::string::npos);  // All chars are in set
    }

    FL_SUBCASE("find_last_not_of with special characters") {
        fl::string s = "path/to/file///";
        FL_CHECK(s.find_last_not_of("/") == 11);  // Last non-'/' is 'e' at position 11

        fl::string s2 = "file.txt...";
        FL_CHECK(s2.find_last_not_of(".") == 7);  // Last non-'.' is 't' at position 7
    }

    FL_SUBCASE("find_last_not_of for reverse tokenization") {
        fl::string s = "word1   word2   word3";
        fl::size last_non_space = s.find_last_not_of(" ");
        FL_CHECK(last_non_space == 20);  // '3' at position 20

        fl::size prev_space = s.find_last_of(" ", last_non_space - 1);
        FL_CHECK(prev_space == 15);  // Space before "word3"

        fl::size prev_word_end = s.find_last_not_of(" ", prev_space);
        FL_CHECK(prev_word_end == 12);  // '2' at position 12
    }

    FL_SUBCASE("find_last_not_of on inline string") {
        fl::string s = "text   ";
        FL_CHECK(s.find_last_not_of(" ") == 3);
        FL_CHECK(s.find_last_not_of(" \t") == 3);
    }

    FL_SUBCASE("find_last_not_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "y");  // Put a 'y' at position 10
        s.replace(50, 1, "z");  // Put a 'z' at position 50

        FL_CHECK(s.find_last_not_of("x") == 50);  // Last non-'x' is 'z' at position 50
        FL_CHECK(s.find_last_not_of("x", 49) == 10);  // Previous non-'x' is 'y' at position 10
        FL_CHECK(s.find_last_not_of("xyz") == fl::string::npos);  // All are x, y, or z
    }

    FL_SUBCASE("find_last_not_of from each position") {
        fl::string s = "aaabbb";
        FL_CHECK(s.find_last_not_of("b", 5) == 2);  // Last non-'b' from position 5 is 'a' at 2
        FL_CHECK(s.find_last_not_of("b", 4) == 2);  // Still position 2
        FL_CHECK(s.find_last_not_of("b", 3) == 2);  // Still position 2
        FL_CHECK(s.find_last_not_of("b", 2) == 2);  // 'a' at position 2
        FL_CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // All 'a' from position 2
    }

    FL_SUBCASE("find_last_not_of realistic use case - trailing whitespace") {
        fl::string s1 = "hello   ";
        FL_CHECK(s1.find_last_not_of(" \t\n\r") == 4);

        fl::string s2 = "hello\t\n  ";
        FL_CHECK(s2.find_last_not_of(" \t\n\r") == 4);

        fl::string s3 = "hello";
        FL_CHECK(s3.find_last_not_of(" \t\n\r") == 4);  // No trailing whitespace

        fl::string s4 = "    ";
        FL_CHECK(s4.find_last_not_of(" \t\n\r") == fl::string::npos);  // All whitespace
    }

    FL_SUBCASE("find_last_not_of realistic use case - trailing zeros") {
        fl::string s = "1230000";
        FL_CHECK(s.find_last_not_of("0") == 2);  // Last non-zero digit at position 2

        fl::string s2 = "00000";
        FL_CHECK(s2.find_last_not_of("0") == fl::string::npos);  // All zeros
    }

    FL_SUBCASE("find_last_not_of realistic use case - file extension") {
        fl::string s = "file.txt   ";
        fl::size end = s.find_last_not_of(" ");
        FL_CHECK(end == 7);  // Last non-space 't' at position 7
    }

    FL_SUBCASE("find_last_not_of with entire alphabet") {
        fl::string s = "abc123";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        FL_CHECK(s.find_last_not_of(alphabet) == 5);  // Last non-letter '3' at position 5
        FL_CHECK(s.find_last_not_of(alphabet, 2) == fl::string::npos);  // All letters up to position 2
    }

    FL_SUBCASE("find_last_not_of with position at string end") {
        fl::string s = "hello";
        FL_CHECK(s.find_last_not_of("xyz", 4) == 4);  // 'o' not in "xyz"
        FL_CHECK(s.find_last_not_of("o", 4) == 3);  // 'l' at position 3
    }

    FL_SUBCASE("find_last_not_of comparison with find_first_not_of") {
        fl::string s = "aaabbbccc";
        // find_first_not_of finds first char that is NOT in set
        // find_last_not_of finds last char that is NOT in set
        FL_CHECK(s.find_first_not_of("a") == 3);  // First non-'a' at position 3
        FL_CHECK(s.find_last_not_of("c") == 5);  // Last non-'c' at position 5
    }

    FL_SUBCASE("find_last_not_of single character repeated") {
        fl::string s = "aaaaaaa";
        FL_CHECK(s.find_last_not_of('a') == fl::string::npos);  // All 'a'
        FL_CHECK(s.find_last_not_of('b') == 6);  // Last char not 'b'
    }

    FL_SUBCASE("find_last_not_of mixed alphanumeric") {
        fl::string s = "abc123def456";
        FL_CHECK(s.find_last_not_of("0123456789") == 8);  // Last letter 'f' at position 8
        FL_CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz") == 11);  // Last digit '6' at position 11
        FL_CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    FL_SUBCASE("find_last_not_of for suffix detection") {
        fl::string s = "hello!!!";
        FL_CHECK(s.find_last_not_of("!") == 4);  // Last letter 'o' at position 4

        fl::string s2 = "value$$$";
        FL_CHECK(s2.find_last_not_of("$") == 4);  // Last letter 'e' at position 4
    }

    FL_SUBCASE("find_last_not_of multiple character types") {
        fl::string s = "hello!!!";
        FL_CHECK(s.find_last_not_of("!") == 4);  // Last non-'!' at position 4

        fl::string s2 = "100$$$";
        FL_CHECK(s2.find_last_not_of("$") == 2);  // Last digit '0' at position 2
    }

    FL_SUBCASE("find_last_not_of with zero count") {
        fl::string s = "hello";
        // Count 0 means empty set, so every character is "not in the set"
        FL_CHECK(s.find_last_not_of("xyz", fl::string::npos, 0) == 4);  // Last char
        FL_CHECK(s.find_last_not_of("xyz", 2, 0) == 2);  // From position 2
    }

    FL_SUBCASE("find_last_not_of for comment trailing spaces") {
        fl::string s = "This is a comment   ";
        FL_CHECK(s.find_last_not_of(" ") == 16);  // Last non-space 't' at position 16
        FL_CHECK(s.find_last_not_of(" \t") == 16);  // Last non-whitespace
    }

    FL_SUBCASE("find_last_not_of comprehensive trim test") {
        fl::string s1 = "   \t\n  hello world  \t\n   ";
        fl::size end = s1.find_last_not_of(" \t\n\r");
        FL_CHECK(end == 17);  // 'd' at position 17

        fl::string s2 = "hello";
        FL_CHECK(s2.find_last_not_of(" \t\n\r") == 4);  // No trimming needed
    }

    FL_SUBCASE("find_last_not_of versus operator==") {
        fl::string s = "aaa";
        // All characters are 'a', so last not 'a' is npos
        FL_CHECK(s.find_last_not_of("a") == fl::string::npos);
        // Confirms all characters match the set

        fl::string s2 = "baa";
        FL_CHECK(s2.find_last_not_of("a") == 0);  // 'b' at position 0
    }

    FL_SUBCASE("find_last_not_of at position 0") {
        fl::string s = "hello world";
        FL_CHECK(s.find_last_not_of('h', 0) == fl::string::npos);  // Can't find non-'h' at position 0
        FL_CHECK(s.find_last_not_of("world", 0) == 0);  // 'h' not in "world"
        FL_CHECK(s.find_last_not_of('e', 0) == 0);  // 'h' at position 0 is not 'e'
    }

    FL_SUBCASE("find_last_not_of with overlapping character sets") {
        fl::string s = "hello123world456";
        FL_CHECK(s.find_last_not_of("0123456789") == 12);  // Last letter 'd'
        FL_CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz") == 15);  // Last digit '6'
        FL_CHECK(s.find_last_not_of("0123456789abcdefghijklmnopqrstuvwxyz") == fl::string::npos);  // All alphanumeric
    }

    FL_SUBCASE("find_last_not_of for line ending detection") {
        fl::string s = "line of text\n\r\n";
        FL_CHECK(s.find_last_not_of("\n\r") == 11);  // Last non-line-ending 't' at position 11
    }

    FL_SUBCASE("find_last_not_of path trailing separators") {
        fl::string s = "path/to/dir///";
        FL_CHECK(s.find_last_not_of("/") == 10);  // Last non-'/' is 'r' at position 10
    }

    FL_SUBCASE("find_last_not_of comparison with rfind") {
        fl::string s = "hello world";
        // For strings without the target character, behavior differs:
        // rfind('x') returns npos (not found)
        // find_last_not_of('x') returns last position (all chars are not 'x')
        FL_CHECK(s.rfind('x') == fl::string::npos);  // 'x' not found
        FL_CHECK(s.find_last_not_of('x') == 10);  // Last char not 'x' is 'd' at position 10
    }

    FL_SUBCASE("find_last_not_of with position exactly at boundary") {
        fl::string s = "aaabbbccc";
        FL_CHECK(s.find_last_not_of("c", 5) == 5);  // Position 5 is 'b', which is not 'c'
        FL_CHECK(s.find_last_not_of("b", 5) == 2);  // From position 5, last non-'b' is 'a' at 2
        FL_CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // Positions 0-2 are all 'a'
    }

    FL_SUBCASE("find_last_not_of for data validation - trailing invalid chars") {
        fl::string s = "12345xyz";
        FL_CHECK(s.find_last_not_of("0123456789") == 7);  // Last non-digit 'z' at position 7

        fl::string s2 = "12345";
        FL_CHECK(s2.find_last_not_of("0123456789") == fl::string::npos);  // All digits (valid)
    }

    FL_SUBCASE("find_last_not_of empty string with various sets") {
        fl::string s = "";
        FL_CHECK(s.find_last_not_of("abc") == fl::string::npos);
        FL_CHECK(s.find_last_not_of("") == fl::string::npos);
        FL_CHECK(s.find_last_not_of("xyz", 0) == fl::string::npos);
        FL_CHECK(s.find_last_not_of('a') == fl::string::npos);
    }

    FL_SUBCASE("find_last_not_of single character string") {
        fl::string s = "x";
        FL_CHECK(s.find_last_not_of('x') == fl::string::npos);  // Only char is 'x'
        FL_CHECK(s.find_last_not_of('y') == 0);  // Only char 'x' is not 'y'
        FL_CHECK(s.find_last_not_of("xy") == fl::string::npos);  // 'x' is in set
        FL_CHECK(s.find_last_not_of("yz") == 0);  // 'x' not in set
    }

    FL_SUBCASE("find_last_not_of realistic trim implementation") {
        fl::string s = "   hello world   ";
        fl::size start = s.find_first_not_of(" \t\n\r");
        fl::size end = s.find_last_not_of(" \t\n\r");

        FL_CHECK(start == 3);  // 'h' at position 3
        FL_CHECK(end == 13);  // 'd' at position 13

        if (start != fl::string::npos && end != fl::string::npos) {
            fl::string trimmed = s.substr(start, end - start + 1);
            FL_CHECK(trimmed == "hello world");
        }
    }

    // at() tests - bounds-checked element access (fl::string compatibility)
    FL_SUBCASE("at() basic access") {
        fl::string s = "Hello";
        FL_CHECK(s.at(0) == 'H');
        FL_CHECK(s.at(1) == 'e');
        FL_CHECK(s.at(2) == 'l');
        FL_CHECK(s.at(3) == 'l');
        FL_CHECK(s.at(4) == 'o');
    }

    FL_SUBCASE("at() const access") {
        const fl::string s = "World";
        FL_CHECK(s.at(0) == 'W');
        FL_CHECK(s.at(1) == 'o');
        FL_CHECK(s.at(2) == 'r');
        FL_CHECK(s.at(3) == 'l');
        FL_CHECK(s.at(4) == 'd');
    }

    FL_SUBCASE("at() modification") {
        fl::string s = "Hello";
        s.at(0) = 'h';
        s.at(4) = '!';
        FL_CHECK(s == "hell!");
    }

    FL_SUBCASE("at() out of bounds") {
        fl::string s = "test";
        // Out of bounds access returns dummy '\0'
        // (unlike fl::string which would throw exception)
        FL_CHECK(s.at(4) == '\0');  // pos == length
        FL_CHECK(s.at(5) == '\0');  // pos > length
        FL_CHECK(s.at(100) == '\0');  // far out of bounds
    }

    FL_SUBCASE("at() const out of bounds") {
        const fl::string s = "test";
        FL_CHECK(s.at(4) == '\0');
        FL_CHECK(s.at(5) == '\0');
        FL_CHECK(s.at(100) == '\0');
    }

    FL_SUBCASE("at() empty string") {
        fl::string s;
        FL_CHECK(s.at(0) == '\0');
        FL_CHECK(s.at(1) == '\0');
    }

    FL_SUBCASE("at() single character") {
        fl::string s = "A";
        FL_CHECK(s.at(0) == 'A');
        FL_CHECK(s.at(1) == '\0');  // out of bounds
    }

    FL_SUBCASE("at() first and last") {
        fl::string s = "ABCDEF";
        FL_CHECK(s.at(0) == 'A');  // first
        FL_CHECK(s.at(5) == 'F');  // last
        FL_CHECK(s.at(6) == '\0');  // past end
    }

    FL_SUBCASE("at() vs operator[]") {
        fl::string s = "compare";
        // Both should behave the same for fl::string
        for (fl::size i = 0; i < s.size(); ++i) {
            FL_CHECK(s.at(i) == s[i]);
        }
        // Out of bounds should also match
        FL_CHECK(s.at(s.size()) == s[s.size()]);
    }

    FL_SUBCASE("at() modification at boundaries") {
        fl::string s = "test";
        s.at(0) = 'T';  // first
        s.at(3) = 'T';  // last
        FL_CHECK(s == "TesT");
    }

    FL_SUBCASE("at() with inline string") {
        fl::string s = "short";  // inline buffer
        FL_CHECK(s.at(0) == 's');
        FL_CHECK(s.at(4) == 't');
        s.at(2) = 'x';
        FL_CHECK(s == "shxrt");
    }

    FL_SUBCASE("at() with heap string") {
        // Create a string that will use heap storage
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.push_back('A' + (i % 26));
        }
        FL_CHECK(s.at(0) == 'A');
        FL_CHECK(s.at(50) == 'A' + (50 % 26));
        FL_CHECK(s.at(99) == 'A' + (99 % 26));
        s.at(50) = 'X';
        FL_CHECK(s.at(50) == 'X');
    }

    FL_SUBCASE("at() sequential access") {
        fl::string s = "0123456789";
        for (fl::size i = 0; i < 10; ++i) {
            FL_CHECK(s.at(i) == static_cast<char>('0' + i));
        }
    }

    FL_SUBCASE("at() modify all characters") {
        fl::string s = "aaaaa";
        for (fl::size i = 0; i < s.size(); ++i) {
            s.at(i) = 'a' + i;
        }
        FL_CHECK(s == "abcde");
    }

    FL_SUBCASE("at() with special characters") {
        fl::string s = "!@#$%";
        FL_CHECK(s.at(0) == '!');
        FL_CHECK(s.at(1) == '@');
        FL_CHECK(s.at(2) == '#');
        FL_CHECK(s.at(3) == '$');
        FL_CHECK(s.at(4) == '%');
    }

    FL_SUBCASE("at() with numbers") {
        fl::string s = "0123456789";
        for (fl::size i = 0; i < 10; ++i) {
            FL_CHECK(s.at(i) == static_cast<char>('0' + i));
        }
    }

    FL_SUBCASE("at() case sensitivity") {
        fl::string s = "AaBbCc";
        FL_CHECK(s.at(0) == 'A');
        FL_CHECK(s.at(1) == 'a');
        FL_CHECK(s.at(2) == 'B');
        FL_CHECK(s.at(3) == 'b');
        FL_CHECK(s.at(4) == 'C');
        FL_CHECK(s.at(5) == 'c');
    }

    FL_SUBCASE("at() with spaces") {
        fl::string s = "a b c";
        FL_CHECK(s.at(0) == 'a');
        FL_CHECK(s.at(1) == ' ');
        FL_CHECK(s.at(2) == 'b');
        FL_CHECK(s.at(3) == ' ');
        FL_CHECK(s.at(4) == 'c');
    }

    FL_SUBCASE("at() with newlines and tabs") {
        fl::string s = "a\nb\tc";
        FL_CHECK(s.at(0) == 'a');
        FL_CHECK(s.at(1) == '\n');
        FL_CHECK(s.at(2) == 'b');
        FL_CHECK(s.at(3) == '\t');
        FL_CHECK(s.at(4) == 'c');
    }

    FL_SUBCASE("at() after clear") {
        fl::string s = "test";
        s.clear();
        FL_CHECK(s.at(0) == '\0');
    }

    FL_SUBCASE("at() after erase") {
        fl::string s = "testing";
        s.erase(3, 4);  // "tes"
        FL_CHECK(s.at(0) == 't');
        FL_CHECK(s.at(1) == 'e');
        FL_CHECK(s.at(2) == 's');
        FL_CHECK(s.at(3) == '\0');  // now out of bounds
    }

    FL_SUBCASE("at() after insert") {
        fl::string s = "test";
        s.insert(2, "XX");  // "teXXst"
        FL_CHECK(s.at(0) == 't');
        FL_CHECK(s.at(1) == 'e');
        FL_CHECK(s.at(2) == 'X');
        FL_CHECK(s.at(3) == 'X');
        FL_CHECK(s.at(4) == 's');
        FL_CHECK(s.at(5) == 't');
    }

    FL_SUBCASE("at() after replace") {
        fl::string s = "Hello";
        s.replace(1, 3, "i");  // "Hio"
        FL_CHECK(s.at(0) == 'H');
        FL_CHECK(s.at(1) == 'i');
        FL_CHECK(s.at(2) == 'o');
        FL_CHECK(s.at(3) == '\0');
    }

    FL_SUBCASE("at() with repeated characters") {
        fl::string s = "aaaaaaaaaa";
        for (fl::size i = 0; i < s.size(); ++i) {
            FL_CHECK(s.at(i) == 'a');
        }
    }

    FL_SUBCASE("at() boundary at length - 1") {
        fl::string s = "test";
        FL_CHECK(s.at(s.size() - 1) == 't');  // last valid character
        FL_CHECK(s.at(s.size()) == '\0');  // first invalid position
    }

    FL_SUBCASE("at() return reference test") {
        fl::string s = "test";
        char& ref = s.at(0);
        ref = 'T';
        FL_CHECK(s == "Test");
        FL_CHECK(s.at(0) == 'T');
    }

    FL_SUBCASE("at() const reference test") {
        const fl::string s = "test";
        const char& ref = s.at(0);
        FL_CHECK(ref == 't');
        FL_CHECK(&ref == &s.at(0));  // same memory location
    }

    FL_SUBCASE("at() with zero position") {
        fl::string s = "test";
        FL_CHECK(s.at(0) == 't');
        s.at(0) = 'T';
        FL_CHECK(s.at(0) == 'T');
    }

    FL_SUBCASE("at() comparison with front/back") {
        fl::string s = "test";
        FL_CHECK(s.at(0) == s.front());
        FL_CHECK(s.at(s.size() - 1) == s.back());
    }

    FL_SUBCASE("at() with substring result") {
        fl::string s = "Hello World";
        fl::string sub = s.substr(6, 5);  // "World"
        FL_CHECK(sub.at(0) == 'W');
        FL_CHECK(sub.at(4) == 'd');
    }

    FL_SUBCASE("at() access pattern") {
        fl::string s = "pattern";
        // Access in different order
        FL_CHECK(s.at(3) == 't');
        FL_CHECK(s.at(0) == 'p');
        FL_CHECK(s.at(6) == 'n');
        FL_CHECK(s.at(2) == 't');
        FL_CHECK(s.at(5) == 'r');
    }

    FL_SUBCASE("at() large index out of bounds") {
        fl::string s = "small";
        FL_CHECK(s.at(1000) == '\0');
        FL_CHECK(s.at(fl::size(-1) / 2) == '\0');  // very large index
    }
}


} // FL_TEST_FILE
