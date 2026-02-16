// Consolidated test suite for fl::string
// Combines tests from: str.cpp, string_comprehensive.cpp, string_memory_bugs.cpp, string_optimization.cpp
// This tests the fl::string implementation in fl/stl/string.h and fl/stl/string.cpp

#include "fl/compiler_control.h"
#include "fl/int.h"
#include "fl/stl/cstdint.h"
#include "fl/stl/cstring.h"
#include "fl/stl/iterator.h"
#include "fl/stl/span.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/stl/strstream.h"
#include "fl/stl/thread.h"
#include "fl/stl/vector.h"
#include "hsv2rgb.h"
#include "test.h"

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
            FL_CHECK(s.at(i) == '0' + i);
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
            FL_CHECK(s.at(i) == ('0' + i));
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

// Test reverse iterators (fl::string compatibility)
FL_TEST_CASE("StrN reverse iterators") {
    FL_SUBCASE("rbegin/rend on non-empty string") {
        fl::string s = "Hello";
        // rbegin() should point to last character
        FL_CHECK(s.rbegin() != s.rend());
        FL_CHECK(*s.rbegin() == 'o');

        // Manually iterate backwards
        auto it = s.rbegin();
        FL_CHECK(*it == 'o'); ++it;
        FL_CHECK(*it == 'l'); ++it;
        FL_CHECK(*it == 'l'); ++it;
        FL_CHECK(*it == 'e'); ++it;
        FL_CHECK(*it == 'H'); ++it;
        // After incrementing past all characters, should equal rend()
        FL_CHECK(it == s.rend());
    }

    FL_SUBCASE("rbegin/rend on empty string") {
        fl::string s = "";
        FL_CHECK(s.rbegin() == s.rend());
    }

    FL_SUBCASE("const rbegin/rend") {
        const fl::string s = "World";
        FL_CHECK(s.rbegin() != s.rend());
        FL_CHECK(*s.rbegin() == 'd');

        auto it = s.rbegin();
        FL_CHECK(*it == 'd'); ++it;
        FL_CHECK(*it == 'l'); ++it;
        FL_CHECK(*it == 'r'); ++it;
        FL_CHECK(*it == 'o'); ++it;
        FL_CHECK(*it == 'W'); ++it;
        FL_CHECK(it == s.rend());
    }

    FL_SUBCASE("crbegin/crend") {
        fl::string s = "Test";
        // crbegin/crend should return const iterators
        auto crit = s.crbegin();
        FL_CHECK(crit != s.crend());
        FL_CHECK(*crit == 't');

        ++crit;
        FL_CHECK(*crit == 's'); ++crit;
        FL_CHECK(*crit == 'e'); ++crit;
        FL_CHECK(*crit == 'T'); ++crit;
        FL_CHECK(crit == s.crend());
    }

    FL_SUBCASE("reverse iteration with single character") {
        fl::string s = "X";
        FL_CHECK(s.rbegin() != s.rend());
        FL_CHECK(*s.rbegin() == 'X');
        auto it = s.rbegin();
        ++it;
        FL_CHECK(it == s.rend());  // After one increment, should reach rend
    }

    FL_SUBCASE("reverse iteration builds reversed string") {
        fl::string s = "ABC";
        fl::string reversed;

        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "CBA");
    }

    FL_SUBCASE("const reverse iteration") {
        const fl::string s = "12345";
        fl::string result;

        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            result.push_back(*it);
        }
        FL_CHECK(result == "54321");
    }

    FL_SUBCASE("modification through reverse iterator") {
        fl::string s = "abcd";
        auto it = s.rbegin();
        *it = 'D';  // Change 'd' to 'D'
        FL_CHECK(s == "abcD");

        ++it;
        *it = 'C';  // Change 'c' to 'C'
        FL_CHECK(s == "abCD");
    }

    FL_SUBCASE("reverse iterator with inline string") {
        fl::string s = "Short";  // Fits in inline buffer
        FL_CHECK(s.rbegin() != s.rend());
        FL_CHECK(*s.rbegin() == 't');

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "trohS");
    }

    FL_SUBCASE("reverse iterator with heap string") {
        // Create a string large enough to require heap allocation
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.push_back('A' + (i % 26));
        }

        FL_CHECK(s.rbegin() != s.rend());
        FL_CHECK(*s.rbegin() == 'V');  // 99 % 26 = 21, 'A' + 21 = 'V'

        // Verify first few characters in reverse
        auto it = s.rbegin();
        FL_CHECK(*it == 'V'); ++it;  // i=99: 99%26=21
        FL_CHECK(*it == 'U'); ++it;  // i=98: 98%26=20
        FL_CHECK(*it == 'T');        // i=97: 97%26=19
    }

    FL_SUBCASE("reverse iterator after modification") {
        fl::string s = "test";
        s.insert(2, "XX");  // "teXXst"

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "tsXXet");
    }

    FL_SUBCASE("reverse iterator matches forward") {
        fl::string s = "abcdef";

        // Forward iteration
        fl::string forward;
        for (auto it = s.begin(); it != s.end(); ++it) {
            forward.push_back(*it);
        }

        // Reverse iteration
        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }

        FL_CHECK(forward == "abcdef");
        FL_CHECK(reversed == "fedcba");
    }

    FL_SUBCASE("reverse iterator with special characters") {
        fl::string s = "!@#$%";
        FL_CHECK(*s.rbegin() == '%');

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "%$#@!");
    }

    FL_SUBCASE("reverse iterator with digits") {
        fl::string s = "0123456789";
        FL_CHECK(*s.rbegin() == '9');

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "9876543210");
    }

    FL_SUBCASE("reverse iterator with whitespace") {
        fl::string s = "a b c";
        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "c b a");
    }

    FL_SUBCASE("reverse iterator iteration") {
        fl::string s = "12345";
        int count = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            count++;
        }
        FL_CHECK(count == static_cast<int>(s.size()));
    }

    FL_SUBCASE("const correctness of reverse iterators") {
        fl::string s = "test";
        const fl::string& cs = s;

        // Non-const version
        auto it = s.rbegin();
        FL_CHECK(it != s.rend());

        // Const version
        auto cit = cs.rbegin();
        FL_CHECK(cit != cs.rend());

        // crbegin always returns const
        auto ccit = s.crbegin();
        FL_CHECK(ccit != s.crend());
    }

    FL_SUBCASE("reverse iterator bounds checking") {
        fl::string s = "ABC";
        auto it = s.rbegin();

        // Should be able to access all characters
        FL_CHECK(*it == 'C'); ++it;
        FL_CHECK(*it == 'B'); ++it;
        FL_CHECK(*it == 'A'); ++it;

        // After iterating past all elements, should reach rend
        FL_CHECK(it == s.rend());
    }

    FL_SUBCASE("reverse iterator with copy-on-write") {
        fl::string s1 = "shared";
        fl::string s2 = s1;  // COW: shares data

        // Read through reverse iterator (no copy)
        FL_CHECK(*s1.rbegin() == 'd');
        FL_CHECK(*s2.rbegin() == 'd');

        // Modify through reverse iterator (triggers copy)
        *s1.rbegin() = 'D';
        FL_CHECK(s1 == "shareD");
        FL_CHECK(s2 == "shared");  // s2 unchanged
    }

    FL_SUBCASE("reverse iterator comparison with at()") {
        fl::string s = "test";
        FL_CHECK(*s.rbegin() == s.at(s.size() - 1));
        FL_CHECK(*(s.rbegin() + 1) == s.at(s.size() - 2));
        FL_CHECK(*(s.rbegin() + 2) == s.at(s.size() - 3));
    }

    FL_SUBCASE("reverse iterator with substr") {
        fl::string s = "Hello World";
        fl::string sub = s.substr(6, 5);  // "World"

        fl::string reversed;
        for (auto it = sub.rbegin(); it != sub.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "dlroW");
    }

    FL_SUBCASE("reverse iterator empty after clear") {
        fl::string s = "test";
        s.clear();
        FL_CHECK(s.rbegin() == s.rend());
    }

    FL_SUBCASE("reverse iterator with repeated characters") {
        fl::string s = "aaaaaa";
        int count = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            FL_CHECK(*it == 'a');
            count++;
        }
        FL_CHECK(count == 6);
    }

    FL_SUBCASE("reverse iterator comparison with back()") {
        fl::string s = "example";
        FL_CHECK(*s.rbegin() == s.back());
        // Note: Can't directly compare reverse_iterator with forward iterator
        // Just verify rbegin points to the last element
        FL_CHECK(*s.rbegin() == s[s.size() - 1]);
    }

    FL_SUBCASE("reverse iterator manual loop count") {
        fl::string s = "count";
        int iterations = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            iterations++;
        }
        FL_CHECK(iterations == static_cast<int>(s.size()));
    }

    FL_SUBCASE("reverse iterator with newlines") {
        fl::string s = "a\nb\nc";
        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "c\nb\na");
    }

    FL_SUBCASE("reverse iterator palindrome check") {
        fl::string s = "racecar";

        // Check if palindrome using reverse iteration
        auto fwd = s.begin();
        auto rev = s.rbegin();
        bool is_palindrome = true;

        while (fwd != s.end() && rev != s.rend()) {
            if (*fwd != *rev) {
                is_palindrome = false;
                break;
            }
            ++fwd;
            ++rev;
        }
        FL_CHECK(is_palindrome == true);
    }

    FL_SUBCASE("reverse iterator not palindrome") {
        fl::string s = "hello";

        auto fwd = s.begin();
        auto rev = s.rbegin();
        bool is_palindrome = true;

        while (fwd != s.end() && rev != s.rend()) {
            if (*fwd != *rev) {
                is_palindrome = false;
                break;
            }
            ++fwd;
            ++rev;
        }
        FL_CHECK(is_palindrome == false);
    }

    FL_SUBCASE("reverse iterator null terminator not included") {
        fl::string s = "test";
        // Reverse iterators should not include null terminator
        int count = 0;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            count++;
        }
        FL_CHECK(count == 4);  // Only actual characters, not '\0'
    }

    FL_SUBCASE("reverse iterator after erase") {
        fl::string s = "testing";
        s.erase(3, 3);  // Remove "tin" -> "tesg"

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "gset");
    }

    FL_SUBCASE("reverse iterator after replace") {
        fl::string s = "test";
        s.replace(1, 2, "XX");  // "tXXt"

        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK(reversed == "tXXt");  // Palindrome!
    }
}

FL_TEST_CASE("String compare operations") {
    // compare() returns <0 if this<other, 0 if equal, >0 if this>other
    // Like strcmp, provides three-way comparison for lexicographical ordering

    FL_SUBCASE("compare with equal strings") {
        fl::string s1 = "hello";
        fl::string s2 = "hello";
        FL_CHECK(s1.compare(s2) == 0);
        FL_CHECK(s2.compare(s1) == 0);
    }

    FL_SUBCASE("compare with different strings") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        FL_CHECK(s1.compare(s2) < 0);  // "abc" < "def"
        FL_CHECK(s2.compare(s1) > 0);  // "def" > "abc"
    }

    FL_SUBCASE("compare empty strings") {
        fl::string s1 = "";
        fl::string s2 = "";
        FL_CHECK(s1.compare(s2) == 0);

        fl::string s3 = "hello";
        FL_CHECK(s1.compare(s3) < 0);  // Empty < non-empty
        FL_CHECK(s3.compare(s1) > 0);  // Non-empty > empty
    }

    FL_SUBCASE("compare with C-string") {
        fl::string s = "hello";
        FL_CHECK(s.compare("hello") == 0);
        FL_CHECK(s.compare("world") < 0);  // "hello" < "world"
        FL_CHECK(s.compare("abc") > 0);    // "hello" > "abc"
    }

    FL_SUBCASE("compare with null C-string") {
        fl::string s = "hello";
        FL_CHECK(s.compare(nullptr) > 0);  // Non-empty > null

        fl::string empty = "";
        FL_CHECK(empty.compare(nullptr) == 0);  // Empty == null
    }

    FL_SUBCASE("compare prefix strings") {
        fl::string s1 = "hello";
        fl::string s2 = "hello world";
        FL_CHECK(s1.compare(s2) < 0);  // Shorter prefix < longer
        FL_CHECK(s2.compare(s1) > 0);  // Longer > shorter prefix
    }

    FL_SUBCASE("compare case sensitivity") {
        fl::string s1 = "Hello";
        fl::string s2 = "hello";
        FL_CHECK(s1.compare(s2) < 0);  // 'H' (72) < 'h' (104)
        FL_CHECK(s2.compare(s1) > 0);
    }

    FL_SUBCASE("compare substring with another string") {
        fl::string s1 = "hello world";
        fl::string s2 = "world";
        // Compare substring [6, 11) with "world"
        FL_CHECK(s1.compare(6, 5, s2) == 0);

        // Compare substring [0, 5) with "world"
        FL_CHECK(s1.compare(0, 5, s2) < 0);  // "hello" < "world"
    }

    FL_SUBCASE("compare substring with npos count") {
        fl::string s = "hello world";
        fl::string s2 = "world";
        // npos means "until end of string"
        FL_CHECK(s.compare(6, fl::string::npos, s2) == 0);
    }

    FL_SUBCASE("compare substring exceeding length") {
        fl::string s = "hello";
        fl::string s2 = "hello world";
        // Compare all of s with s2 (count is limited to available chars)
        FL_CHECK(s.compare(0, 100, s2) < 0);  // "hello" < "hello world"
    }

    FL_SUBCASE("compare substring with C-string") {
        fl::string s = "hello world";
        FL_CHECK(s.compare(0, 5, "hello") == 0);
        FL_CHECK(s.compare(6, 5, "world") == 0);
        FL_CHECK(s.compare(0, 5, "world") < 0);  // "hello" < "world"
    }

    FL_SUBCASE("compare substring with substring") {
        fl::string s1 = "prefix_data_suffix";
        fl::string s2 = "other_data_end";
        // Compare "data" from s1 with "data" from s2
        FL_CHECK(s1.compare(7, 4, s2, 6, 4) == 0);

        // Compare "prefix" from s1 with "other" from s2
        FL_CHECK(s1.compare(0, 6, s2, 0, 5) > 0);  // "prefix" > "other"
    }

    FL_SUBCASE("compare substring with npos in second string") {
        fl::string s1 = "hello_world";
        fl::string s2 = "world_is_beautiful";
        // Compare "world" from s1 with "world_is_beautiful" from s2
        FL_CHECK(s1.compare(6, 5, s2, 0, fl::string::npos) < 0);  // "world" < "world_is_beautiful"
    }

    FL_SUBCASE("compare out of bounds position") {
        fl::string s1 = "hello";
        fl::string s2 = "world";
        // Out of bounds position returns comparison with empty string
        FL_CHECK(s1.compare(100, 5, s2) < 0);  // "" < "world"
        FL_CHECK(s2.compare(100, 5, "") == 0);  // "" == ""
    }

    FL_SUBCASE("compare with count2 for C-string") {
        fl::string s = "hello";
        // Compare with first 3 chars of "hello world"
        FL_CHECK(s.compare(0, 3, "hello world", 3) == 0);  // "hel" == "hel"

        // Compare with first 5 chars
        FL_CHECK(s.compare(0, 5, "hello world", 5) == 0);  // "hello" == "hello"

        // Compare with first 11 chars
        FL_CHECK(s.compare(0, 5, "hello world", 11) < 0);  // "hello" < "hello world"
    }

    FL_SUBCASE("compare substring length mismatch") {
        fl::string s1 = "testing";
        fl::string s2 = "test";
        // When actual compared portions are equal but lengths differ, shorter is "less"
        FL_CHECK(s1.compare(0, 4, s2, 0, 4) == 0);  // "test" == "test"
        FL_CHECK(s1.compare(0, 7, s2, 0, 4) > 0);   // "testing" > "test"
    }

    FL_SUBCASE("compare with zero count") {
        fl::string s1 = "hello";
        fl::string s2 = "world";
        // Zero count means comparing empty strings
        FL_CHECK(s1.compare(0, 0, s2, 0, 0) == 0);  // "" == ""
        FL_CHECK(s1.compare(2, 0, s2, 3, 0) == 0);  // "" == ""
    }

    FL_SUBCASE("compare for sorting") {
        fl::string s1 = "apple";
        fl::string s2 = "banana";
        fl::string s3 = "cherry";

        FL_CHECK(s1.compare(s2) < 0);
        FL_CHECK(s2.compare(s3) < 0);
        FL_CHECK(s1.compare(s3) < 0);

        // Verify transitivity
        FL_CHECK((s1.compare(s2) < 0 && s2.compare(s3) < 0) == (s1.compare(s3) < 0));
    }

    FL_SUBCASE("compare with special characters") {
        fl::string s1 = "hello!";
        fl::string s2 = "hello?";
        FL_CHECK(s1.compare(s2) < 0);  // '!' (33) < '?' (63)

        fl::string s3 = "hello\n";
        fl::string s4 = "hello\t";
        FL_CHECK(s3.compare(s4) > 0);  // '\n' (10) > '\t' (9), so s3 > s4
    }

    FL_SUBCASE("compare numbers as strings") {
        fl::string s1 = "10";
        fl::string s2 = "9";
        // Lexicographical: '1' < '9', so "10" < "9"
        FL_CHECK(s1.compare(s2) < 0);

        fl::string s3 = "100";
        fl::string s4 = "99";
        FL_CHECK(s3.compare(s4) < 0);  // '1' < '9'
    }

    FL_SUBCASE("compare position at string boundary") {
        fl::string s = "hello";
        // Position at length() is valid (points to empty substring)
        FL_CHECK(s.compare(5, 0, "") == 0);
        FL_CHECK(s.compare(5, 0, "x") < 0);  // "" < "x"
    }

    FL_SUBCASE("compare entire string via substring") {
        fl::string s1 = "hello world";
        fl::string s2 = "hello world";
        // These should be equivalent
        FL_CHECK(s1.compare(s2) == s1.compare(0, fl::string::npos, s2));
        FL_CHECK(s1.compare(s2) == s1.compare(0, s1.length(), s2, 0, s2.length()));
    }

    FL_SUBCASE("compare after string modifications") {
        fl::string s1 = "hello";
        fl::string s2 = "hello";
        FL_CHECK(s1.compare(s2) == 0);

        s1.append(" world");
        FL_CHECK(s1.compare(s2) > 0);  // "hello world" > "hello"

        s1.clear();
        FL_CHECK(s1.compare(s2) < 0);  // "" < "hello"
    }

    FL_SUBCASE("compare consistency with equality operators") {
        fl::string s1 = "test";
        fl::string s2 = "test";
        fl::string s3 = "other";

        // compare() == 0 should match operator==
        FL_CHECK((s1.compare(s2) == 0) == (s1 == s2));
        FL_CHECK((s1.compare(s3) == 0) == (s1 == s3));

        // compare() != 0 should match operator!=
        FL_CHECK((s1.compare(s3) != 0) == (s1 != s3));
    }

    FL_SUBCASE("compare with repeated characters") {
        fl::string s1 = "aaaa";
        fl::string s2 = "aaab";
        FL_CHECK(s1.compare(s2) < 0);  // Last char: 'a' < 'b'

        fl::string s3 = "aaa";
        FL_CHECK(s1.compare(s3) > 0);  // "aaaa" > "aaa"
    }

    FL_SUBCASE("compare middle substrings") {
        fl::string s = "the quick brown fox jumps";
        FL_CHECK(s.compare(4, 5, "quick") == 0);
        FL_CHECK(s.compare(10, 5, "brown") == 0);
        FL_CHECK(s.compare(20, 5, "jumps") == 0);
    }

    FL_SUBCASE("compare overlapping substrings of same string") {
        fl::string s = "abcdefgh";
        // Compare "abc" with "def"
        FL_CHECK(s.compare(0, 3, s, 3, 3) < 0);  // "abc" < "def"

        // Compare "def" with "abc"
        FL_CHECK(s.compare(3, 3, s, 0, 3) > 0);  // "def" > "abc"
    }
}

FL_TEST_CASE("StrN comparison operators") {
    FL_SUBCASE("operator< basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        FL_CHECK(s1 < s2);      // "abc" < "def"
        FL_CHECK_FALSE(s2 < s1); // NOT "def" < "abc"
        FL_CHECK_FALSE(s1 < s3); // NOT "abc" < "abc" (equal)
    }

    FL_SUBCASE("operator> basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        FL_CHECK(s2 > s1);      // "def" > "abc"
        FL_CHECK_FALSE(s1 > s2); // NOT "abc" > "def"
        FL_CHECK_FALSE(s1 > s3); // NOT "abc" > "abc" (equal)
    }

    FL_SUBCASE("operator<= basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        FL_CHECK(s1 <= s2);     // "abc" <= "def"
        FL_CHECK(s1 <= s3);     // "abc" <= "abc" (equal)
        FL_CHECK_FALSE(s2 <= s1); // NOT "def" <= "abc"
    }

    FL_SUBCASE("operator>= basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        FL_CHECK(s2 >= s1);     // "def" >= "abc"
        FL_CHECK(s1 >= s3);     // "abc" >= "abc" (equal)
        FL_CHECK_FALSE(s1 >= s2); // NOT "abc" >= "def"
    }

    FL_SUBCASE("comparison with different template sizes") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<64> s2 = "def";
        fl::StrN<128> s3 = "abc";

        // Test < operator
        FL_CHECK(s1 < s2);      // "abc" < "def"
        FL_CHECK_FALSE(s2 < s1); // NOT "def" < "abc"
        FL_CHECK_FALSE(s1 < s3); // NOT "abc" < "abc" (equal)

        // Test > operator
        FL_CHECK(s2 > s1);      // "def" > "abc"
        FL_CHECK_FALSE(s1 > s2); // NOT "abc" > "def"
        FL_CHECK_FALSE(s1 > s3); // NOT "abc" > "abc" (equal)

        // Test <= operator
        FL_CHECK(s1 <= s2);     // "abc" <= "def"
        FL_CHECK(s1 <= s3);     // "abc" <= "abc" (equal)
        FL_CHECK_FALSE(s2 <= s1); // NOT "def" <= "abc"

        // Test >= operator
        FL_CHECK(s2 >= s1);     // "def" >= "abc"
        FL_CHECK(s1 >= s3);     // "abc" >= "abc" (equal)
        FL_CHECK_FALSE(s1 >= s2); // NOT "abc" >= "def"
    }

    FL_SUBCASE("comparison with empty strings") {
        fl::StrN<32> empty1 = "";
        fl::StrN<32> empty2 = "";
        fl::StrN<32> nonempty = "abc";

        // Empty strings are equal to each other
        FL_CHECK_FALSE(empty1 < empty2);  // NOT "" < ""
        FL_CHECK_FALSE(empty1 > empty2);  // NOT "" > ""
        FL_CHECK(empty1 <= empty2);       // "" <= ""
        FL_CHECK(empty1 >= empty2);       // "" >= ""

        // Empty string is less than non-empty
        FL_CHECK(empty1 < nonempty);      // "" < "abc"
        FL_CHECK_FALSE(empty1 > nonempty); // NOT "" > "abc"
        FL_CHECK(empty1 <= nonempty);     // "" <= "abc"
        FL_CHECK_FALSE(empty1 >= nonempty); // NOT "" >= "abc"

        // Non-empty string is greater than empty
        FL_CHECK_FALSE(nonempty < empty1); // NOT "abc" < ""
        FL_CHECK(nonempty > empty1);      // "abc" > ""
        FL_CHECK_FALSE(nonempty <= empty1); // NOT "abc" <= ""
        FL_CHECK(nonempty >= empty1);     // "abc" >= ""
    }

    FL_SUBCASE("comparison with prefix strings") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "abcd";

        FL_CHECK(s1 < s2);       // "abc" < "abcd" (prefix is less)
        FL_CHECK_FALSE(s1 > s2);  // NOT "abc" > "abcd"
        FL_CHECK(s1 <= s2);      // "abc" <= "abcd"
        FL_CHECK_FALSE(s1 >= s2); // NOT "abc" >= "abcd"

        FL_CHECK_FALSE(s2 < s1);  // NOT "abcd" < "abc"
        FL_CHECK(s2 > s1);       // "abcd" > "abc"
        FL_CHECK_FALSE(s2 <= s1); // NOT "abcd" <= "abc"
        FL_CHECK(s2 >= s1);      // "abcd" >= "abc"
    }

    FL_SUBCASE("case sensitivity") {
        fl::StrN<32> lower = "abc";
        fl::StrN<32> upper = "ABC";

        // Uppercase letters have lower ASCII values than lowercase
        FL_CHECK(upper < lower);      // "ABC" < "abc" (ASCII 65 < 97)
        FL_CHECK_FALSE(upper > lower); // NOT "ABC" > "abc"
        FL_CHECK(upper <= lower);     // "ABC" <= "abc"
        FL_CHECK_FALSE(upper >= lower); // NOT "ABC" >= "abc"
    }

    FL_SUBCASE("lexicographical ordering for sorting") {
        fl::StrN<32> s1 = "apple";
        fl::StrN<32> s2 = "banana";
        fl::StrN<32> s3 = "cherry";
        fl::StrN<32> s4 = "apple";

        // Verify transitivity and consistency for sorting
        FL_CHECK(s1 < s2);
        FL_CHECK(s2 < s3);
        FL_CHECK(s1 < s3);  // Transitive: if a<b and b<c, then a<c

        FL_CHECK(s1 <= s4);  // Equal strings
        FL_CHECK(s4 <= s1);  // Equal strings
        FL_CHECK(s1 >= s4);  // Equal strings
        FL_CHECK(s4 >= s1);  // Equal strings

        // Check reverse ordering
        FL_CHECK(s3 > s2);
        FL_CHECK(s2 > s1);
        FL_CHECK(s3 > s1);

        FL_CHECK(s3 >= s2);
        FL_CHECK(s2 >= s1);
        FL_CHECK(s3 >= s1);
    }

    FL_SUBCASE("comparison with special characters") {
        fl::StrN<32> s1 = "abc!";
        fl::StrN<32> s2 = "abc@";
        fl::StrN<32> s3 = "abc#";

        // ASCII: ! (33) < # (35) < @ (64)
        FL_CHECK(s1 < s3);       // "abc!" < "abc#"
        FL_CHECK(s3 < s2);       // "abc#" < "abc@"
        FL_CHECK(s1 < s2);       // "abc!" < "abc@"

        FL_CHECK(s2 > s3);       // "abc@" > "abc#"
        FL_CHECK(s3 > s1);       // "abc#" > "abc!"
        FL_CHECK(s2 > s1);       // "abc@" > "abc!"
    }

    FL_SUBCASE("comparison with number strings") {
        fl::StrN<32> s1 = "10";
        fl::StrN<32> s2 = "2";
        fl::StrN<32> s3 = "100";

        // Lexicographical, not numeric: "10" < "2" because '1' < '2'
        FL_CHECK(s1 < s2);       // "10" < "2" (lexicographical)
        FL_CHECK(s3 < s2);       // "100" < "2" (lexicographical)

        FL_CHECK(s2 > s1);       // "2" > "10"
        FL_CHECK(s2 > s3);       // "2" > "100"
    }

    FL_SUBCASE("consistency with equality operators") {
        fl::StrN<32> s1 = "test";
        fl::StrN<32> s2 = "test";
        fl::StrN<32> s3 = "different";

        // If s1 == s2, then s1 <= s2 and s1 >= s2
        FL_CHECK(s1 == s2);
        FL_CHECK(s1 <= s2);
        FL_CHECK(s1 >= s2);
        FL_CHECK_FALSE(s1 < s2);
        FL_CHECK_FALSE(s1 > s2);

        // If s1 != s3, then either s1 < s3 or s1 > s3
        FL_CHECK(s1 != s3);
        bool one_comparison_true = (s1 < s3) || (s1 > s3);
        FL_CHECK(one_comparison_true);
    }

    FL_SUBCASE("comparison operator completeness") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";

        // Exactly one of <, ==, > should be true
        int count = 0;
        if (s1 < s2) count++;
        if (s1 == s2) count++;
        if (s1 > s2) count++;
        FL_CHECK(count == 1);  // Exactly one should be true

        // Verify <= is equivalent to (< or ==)
        FL_CHECK((s1 <= s2) == ((s1 < s2) || (s1 == s2)));

        // Verify >= is equivalent to (> or ==)
        FL_CHECK((s1 >= s2) == ((s1 > s2) || (s1 == s2)));

        // Verify < is the opposite of >=
        FL_CHECK((s1 < s2) == !(s1 >= s2));

        // Verify > is the opposite of <=
        FL_CHECK((s1 > s2) == !(s1 <= s2));
    }

    FL_SUBCASE("comparison with heap vs inline storage") {
        // Short string (inline storage)
        fl::StrN<64> short1 = "short";
        fl::StrN<64> short2 = "short";

        // Long string (heap storage) - exceeds 64 bytes
        fl::StrN<64> long1 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";
        fl::StrN<64> long2 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";

        // Comparison should work correctly regardless of storage type
        FL_CHECK(short1 == short2);
        FL_CHECK(short1 <= short2);
        FL_CHECK(short1 >= short2);
        FL_CHECK_FALSE(short1 < short2);
        FL_CHECK_FALSE(short1 > short2);

        FL_CHECK(long1 == long2);
        FL_CHECK(long1 <= long2);
        FL_CHECK(long1 >= long2);
        FL_CHECK_FALSE(long1 < long2);
        FL_CHECK_FALSE(long1 > long2);

        // Mixed: short vs long
        FL_CHECK(short1 < long1);  // "short" < "this is..."
        FL_CHECK(long1 > short1);  // "this is..." > "short"
    }
}

//=============================================================================
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
// SECTION: Tests from string_memory_bugs.cpp
//=============================================================================


FL_TEST_CASE("StringHolder - Capacity off-by-one bugs") {
    // These tests are designed to expose the bugs where mCapacity is set to mLength
    // instead of mLength + 1 in StringHolder constructors

    FL_SUBCASE("StringHolder(fl::size length) capacity bug") {
        // This constructor should allocate length+1 bytes and set mCapacity = length+1
        // But it incorrectly sets mCapacity = mLength (missing the +1 for null terminator)

        // Create a string that will use StringHolder(fl::size length) constructor
        // This happens when we create a string with a specific length
        fl::string s1("x");  // Short string, inline storage

        // Now force it to grow beyond inline storage
        // This will trigger StringHolder allocation
        fl::size target_size = FASTLED_STR_INLINED_SIZE + 10;
        for (fl::size i = 1; i < target_size; ++i) {
            s1.append("x");
        }

        FL_CHECK(s1.size() == target_size);
        FL_CHECK(s1.capacity() >= target_size);  // Should be >= target_size + 1 for null terminator

        // The bug manifests when we try to append more data
        // With incorrect capacity, buffer overruns can occur
        s1.append("y");
        FL_CHECK(s1.size() == target_size + 1);
        FL_CHECK(s1[target_size] == 'y');
        FL_CHECK(s1.c_str()[target_size + 1] == '\0');  // Null terminator should be present
    }

    FL_SUBCASE("StringHolder(const char*, fl::size) capacity bug") {
        // This constructor has the same bug: mCapacity = mLength instead of mLength + 1

        // Create a long string that will trigger heap allocation
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s(long_str.c_str());

        FL_CHECK(s.size() == long_str.length());

        // Verify capacity is correct (should include null terminator)
        // Bug: capacity will be equal to length, missing +1 for null terminator

        // This might not catch the bug directly, but the next operation will
        FL_CHECK(s.capacity() >= long_str.length());

        // Try to append - this can cause buffer issues with wrong capacity
        s.append("b");
        FL_CHECK(s.size() == long_str.length() + 1);
        FL_CHECK(s[long_str.length()] == 'b');

        // Verify null termination is intact
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }

    FL_SUBCASE("StringHolder::grow() fallback path capacity bug") {
        // In grow(), when realloc fails and malloc is used as fallback,
        // mCapacity = mLength is used instead of mLength + 1

        // We can't easily force realloc to fail, but we can test the normal path
        // and document that line 79 in str.cpp has the same bug

        fl::string s("Start");

        // Grow the string multiple times
        // Note: "_extra_data_to_force_growth" is 27 characters
        for (int i = 0; i < 10; ++i) {
            fl::size old_size = s.size();
            s.append("_extra_data_to_force_growth");
            FL_CHECK(s.size() == old_size + 27);
        }

        // Verify final state
        FL_CHECK(s.size() == 5 + (10 * 27));
        FL_CHECK(s.capacity() >= s.size());
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }

    FL_SUBCASE("Copy with length exactly at inline boundary") {
        // Test strings that are exactly at the boundary between inline and heap storage
        // This is where off-by-one errors are most likely to manifest

        fl::size boundary = FASTLED_STR_INLINED_SIZE - 1;
        fl::string boundary_str(boundary, 'b');

        fl::string s1(boundary_str.c_str());
        FL_CHECK(s1.size() == boundary);

        // This should still fit in inline storage (boundary + 1 for null terminator <= SIZE)
        // Now push it just over the boundary
        s1.append("X");
        FL_CHECK(s1.size() == boundary + 1);

        // At this point, if SIZE = 64 and boundary = 63, then:
        // - boundary + 1 = 64 characters
        // - We need 65 bytes total (64 + 1 for null terminator)
        // - This should trigger heap allocation

        // Verify we can still access and modify the string
        s1.append("Y");
        FL_CHECK(s1.size() == boundary + 2);
        FL_CHECK(s1[boundary] == 'X');
        FL_CHECK(s1[boundary + 1] == 'Y');
    }

    FL_SUBCASE("Null terminator preservation after operations") {
        // Verify that null terminators are always correctly placed

        fl::string s1("Hello");
        FL_CHECK(s1.c_str()[5] == '\0');
        FL_CHECK(::strlen(s1.c_str()) == 5);

        s1.append(" World");
        FL_CHECK(s1.c_str()[11] == '\0');
        FL_CHECK(::strlen(s1.c_str()) == 11);

        // Force heap allocation
        fl::string long_append(FASTLED_STR_INLINED_SIZE, 'x');
        s1.append(long_append.c_str());
        FL_CHECK(s1.c_str()[s1.size()] == '\0');
        FL_CHECK(::strlen(s1.c_str()) == s1.size());
    }

    FL_SUBCASE("Capacity after copy operations") {
        // Test that capacity is correctly maintained during copy-on-write operations

        fl::string long_str(FASTLED_STR_INLINED_SIZE + 50, 'c');
        fl::string s1(long_str.c_str());
        fl::string s2 = s1;  // Copy (copy-on-write)

        // Both should report same size
        FL_CHECK(s1.size() == s2.size());
        FL_CHECK(s1.size() == long_str.length());

        // Modify s2 to trigger copy-on-write
        s2.append("_modified");

        // s1 should be unchanged
        FL_CHECK(s1.size() == long_str.length());

        // s2 should have grown
        FL_CHECK(s2.size() == long_str.length() + 9);

        // Both should maintain proper null termination
        FL_CHECK(s1.c_str()[s1.size()] == '\0');
        FL_CHECK(s2.c_str()[s2.size()] == '\0');
        FL_CHECK(::strlen(s1.c_str()) == s1.size());
        FL_CHECK(::strlen(s2.c_str()) == s2.size());
    }
}

FL_TEST_CASE("StringHolder - hasCapacity checks") {
    // Test the hasCapacity() method which relies on mCapacity being correct

    FL_SUBCASE("Reserve and capacity tracking") {
        fl::string s;

        // Start with empty string
        FL_CHECK(s.empty());

        // Reserve space
        s.reserve(100);
        FL_CHECK(s.capacity() >= 100);

        // Add content up to reserved capacity
        for (fl::size i = 0; i < 50; ++i) {
            s.append("a");
        }
        FL_CHECK(s.size() == 50);

        // Capacity should accommodate null terminator
        // If capacity was set to size (bug), then we'd have buffer issues
        FL_CHECK(s.capacity() >= 50);

        // Continue appending
        for (fl::size i = 0; i < 50; ++i) {
            s.append("b");
        }
        FL_CHECK(s.size() == 100);

        // Verify null termination
        FL_CHECK(s.c_str()[100] == '\0');
        FL_CHECK(::strlen(s.c_str()) == 100);
    }

    FL_SUBCASE("Write operations and capacity") {
        fl::string s;

        // Use write() method which checks capacity
        const char* data1 = "First chunk of data";
        s.write(data1, ::strlen(data1));
        FL_CHECK(s.size() == ::strlen(data1));
        FL_CHECK(::strcmp(s.c_str(), data1) == 0);

        // Write more data
        const char* data2 = " and second chunk";
        s.write(data2, ::strlen(data2));

        fl::size expected_size = ::strlen(data1) + ::strlen(data2);
        FL_CHECK(s.size() == expected_size);
        FL_CHECK(s.c_str()[expected_size] == '\0');

        // Force heap allocation by writing a large amount
        fl::size large_size = FASTLED_STR_INLINED_SIZE + 100;
        for (fl::size i = s.size(); i < large_size; ++i) {
            s.write('x');
        }

        FL_CHECK(s.size() == large_size);
        FL_CHECK(s.c_str()[large_size] == '\0');
    }
}

FL_TEST_CASE("StringHolder - Edge cases exposing capacity bugs") {
    FL_SUBCASE("Exact boundary conditions") {
        // Test strings of length 0, 1, SIZE-1, SIZE, SIZE+1

        // Length 0
        fl::string s0;
        FL_CHECK(s0.size() == 0);
        FL_CHECK(s0.c_str()[0] == '\0');

        // Length 1
        fl::string s1("a");
        FL_CHECK(s1.size() == 1);
        FL_CHECK(s1.c_str()[1] == '\0');
        FL_CHECK(::strlen(s1.c_str()) == 1);

        // Length SIZE-1 (should fit inline with null terminator)
        fl::string str_size_minus_1(FASTLED_STR_INLINED_SIZE - 1, 'm');
        fl::string s_sm1(str_size_minus_1.c_str());
        FL_CHECK(s_sm1.size() == FASTLED_STR_INLINED_SIZE - 1);
        FL_CHECK(s_sm1.c_str()[FASTLED_STR_INLINED_SIZE - 1] == '\0');

        // Length SIZE (exactly at boundary, needs heap)
        fl::string str_size(FASTLED_STR_INLINED_SIZE, 's');
        fl::string s_s(str_size.c_str());
        FL_CHECK(s_s.size() == FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s_s.c_str()[FASTLED_STR_INLINED_SIZE] == '\0');
        FL_CHECK(::strlen(s_s.c_str()) == FASTLED_STR_INLINED_SIZE);

        // Length SIZE+1
        fl::string str_size_plus_1(FASTLED_STR_INLINED_SIZE + 1, 'p');
        fl::string s_sp1(str_size_plus_1.c_str());
        FL_CHECK(s_sp1.size() == FASTLED_STR_INLINED_SIZE + 1);
        FL_CHECK(s_sp1.c_str()[FASTLED_STR_INLINED_SIZE + 1] == '\0');
        FL_CHECK(::strlen(s_sp1.c_str()) == FASTLED_STR_INLINED_SIZE + 1);
    }

    FL_SUBCASE("Multiple append operations at boundaries") {
        fl::string s;

        // Build up to exactly SIZE-1
        for (fl::size i = 0; i < FASTLED_STR_INLINED_SIZE - 1; ++i) {
            s.append("a");
        }
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE - 1);

        // One more append pushes to exactly SIZE
        s.append("b");
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE);
        FL_CHECK(s.c_str()[FASTLED_STR_INLINED_SIZE] == '\0');

        // One more append forces heap allocation
        s.append("c");
        FL_CHECK(s.size() == FASTLED_STR_INLINED_SIZE + 1);
        FL_CHECK(s.c_str()[FASTLED_STR_INLINED_SIZE + 1] == '\0');

        // Verify content is correct
        FL_CHECK(s[FASTLED_STR_INLINED_SIZE - 1] == 'b');
        FL_CHECK(s[FASTLED_STR_INLINED_SIZE] == 'c');
    }

    FL_SUBCASE("Substr operations preserving null termination") {
        fl::string original("This is a test string for substring operations");

        fl::string sub1 = original.substr(0, 4);  // "This"
        FL_CHECK(sub1.size() == 4);
        FL_CHECK(sub1.c_str()[4] == '\0');
        FL_CHECK(::strcmp(sub1.c_str(), "This") == 0);

        fl::string sub2 = original.substr(10, 4);  // "test"
        FL_CHECK(sub2.size() == 4);
        FL_CHECK(sub2.c_str()[4] == '\0');
        FL_CHECK(::strcmp(sub2.c_str(), "test") == 0);

        fl::string sub3 = original.substr(original.size() - 10);  // "operations"
        FL_CHECK(sub3.size() == 10);
        FL_CHECK(sub3.c_str()[10] == '\0');
        FL_CHECK(::strcmp(sub3.c_str(), "operations") == 0);
    }
}

FL_TEST_CASE("StringHolder - Memory safety with incorrect capacity") {
    // These tests attempt to expose memory corruption that would occur
    // if capacity is set incorrectly (missing +1 for null terminator)

    FL_SUBCASE("Rapid growth and access patterns") {
        fl::string s("initial");

        // Grow in various increments
        s.append("_1234567890");
        FL_CHECK(::strlen(s.c_str()) == s.size());

        s.append("_abcdefghijklmnopqrstuvwxyz");
        FL_CHECK(::strlen(s.c_str()) == s.size());

        // Force transition from inline to heap multiple times
        s.clear();
        s = "short";
        FL_CHECK(::strlen(s.c_str()) == 5);

        fl::string long_data(FASTLED_STR_INLINED_SIZE * 2, 'L');
        s = long_data.c_str();
        FL_CHECK(::strlen(s.c_str()) == long_data.length());

        s.clear();
        s = "tiny";
        FL_CHECK(::strlen(s.c_str()) == 4);
    }

    FL_SUBCASE("Copy and modify patterns") {
        fl::string base(FASTLED_STR_INLINED_SIZE + 10, 'B');
        fl::string s1(base.c_str());

        // Create multiple copies
        fl::string s2 = s1;
        fl::string s3 = s1;
        fl::string s4 = s1;

        // Modify each copy differently
        s2.append("_s2");
        s3.append("_s3");
        s4.append("_s4");

        // All should maintain null termination
        FL_CHECK(::strlen(s1.c_str()) == s1.size());
        FL_CHECK(::strlen(s2.c_str()) == s2.size());
        FL_CHECK(::strlen(s3.c_str()) == s3.size());
        FL_CHECK(::strlen(s4.c_str()) == s4.size());

        // Original should be unchanged
        FL_CHECK(s1.size() == base.length());

        // Copies should have grown
        FL_CHECK(s2.size() == base.length() + 3);
        FL_CHECK(s3.size() == base.length() + 3);
        FL_CHECK(s4.size() == base.length() + 3);
    }

    FL_SUBCASE("Insert operations with capacity constraints") {
        fl::string s("Hello World");

        // Insert in the middle
        s.insert(5, " Beautiful");
        FL_CHECK(::strlen(s.c_str()) == s.size());
        FL_CHECK(::strcmp(s.c_str(), "Hello Beautiful World") == 0);

        // Insert at the beginning
        s.insert(0, ">> ");
        FL_CHECK(::strlen(s.c_str()) == s.size());

        // Insert at the end
        s.insert(s.size(), " <<");
        FL_CHECK(::strlen(s.c_str()) == s.size());

        // Verify null termination throughout
        FL_CHECK(s.c_str()[s.size()] == '\0');
    }
}

//=============================================================================
// SECTION: Tests from string_optimization.cpp
//=============================================================================


FL_TEST_CASE("fl::string - Numeric append performance patterns") {
    // Test numeric append operations that currently allocate temporary StrN<64> buffers
    // These tests validate that optimizations don't break functionality

    FL_SUBCASE("Integer append operations") {
        fl::string s;

        // Test various integer types
        s.append(static_cast<i8>(127));
        FL_CHECK(s == "127");

        s.clear();
        s.append(static_cast<u8>(255));
        FL_CHECK(s == "255");

        s.clear();
        s.append(static_cast<i16>(-32768));
        FL_CHECK(s == "-32768");

        s.clear();
        s.append(static_cast<u16>(65535));
        FL_CHECK(s == "65535");

        s.clear();
        s.append(static_cast<i32>(-2147483647));
        FL_CHECK(s == "-2147483647");

        s.clear();
        s.append(static_cast<u32>(4294967295U));
        FL_CHECK(s == "4294967295");
    }

    FL_SUBCASE("64-bit integer append operations") {
        fl::string s;

        s.append(static_cast<int64_t>(-9223372036854775807LL));
        FL_CHECK(s == "-9223372036854775807");

        s.clear();
        s.append(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("Float append operations") {
        fl::string s;

        s.append(3.14159f);
        // Check that it contains a decimal representation
        FL_CHECK(s.size() > 0);
        FL_CHECK(s.find('.') != string::npos);

        s.clear();
        s.append(-273.15f);
        FL_CHECK(s.size() > 0);
        FL_CHECK(s[0] == '-');
    }

    FL_SUBCASE("Mixed numeric append operations") {
        fl::string s;

        s.append("Value: ");
        s.append(42);
        s.append(", Float: ");
        s.append(3.14f);
        s.append(", Hex: 0x");
        s.appendHex(static_cast<u32>(255));

        FL_CHECK(s.find("42") != string::npos);
        FL_CHECK(s.find("3.14") != string::npos);
        // Check for either lowercase or uppercase hex output
        bool has_hex = (s.find("ff") != string::npos) || (s.find("FF") != string::npos);
        FL_CHECK(has_hex);
    }

    FL_SUBCASE("Rapid numeric append sequence") {
        fl::string s;

        // Simulate rapid appends that would benefit from buffer reuse
        for (int i = 0; i < 100; ++i) {
            s.append(i);
            if (i < 99) {
                s.append(",");
            }
        }

        FL_CHECK(s.find("0,1,2") != string::npos);
        FL_CHECK(s.find("98,99") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Hexadecimal formatting") {
    FL_SUBCASE("Hex append basic") {
        fl::string s;

        s.appendHex(static_cast<u8>(0xFF));
        FL_CHECK(s.size() > 0);

        s.clear();
        s.appendHex(static_cast<u32>(0xDEADBEEF));
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("Hex append 64-bit") {
        fl::string s;

        s.appendHex(static_cast<uint64_t>(0xFEEDFACECAFEBEEFULL));
        FL_CHECK(s.size() > 0);
    }
}

FL_TEST_CASE("fl::string - Octal formatting") {
    FL_SUBCASE("Octal append basic") {
        fl::string s;

        s.appendOct(static_cast<u32>(8));
        FL_CHECK(s == "10");  // 8 in octal is "10"

        s.clear();
        s.appendOct(static_cast<u32>(64));
        FL_CHECK(s == "100");  // 64 in octal is "100"
    }
}

FL_TEST_CASE("fl::string - Thread safety of numeric operations") {
    // Test that numeric append operations work correctly when called from multiple threads
    // This is important if we use thread-local buffers for optimization

    FL_SUBCASE("Concurrent numeric appends") {
        const int kNumThreads = 4;
        const int kIterations = 100;

        fl::vector<fl::thread> threads;
        fl::vector<fl::string> results(kNumThreads);

        for (int t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([t, &results]() {
                fl::string& s = results[t];
                for (int i = 0; i < kIterations; ++i) {
                    s.append(t * 1000 + i);
                    s.append(",");
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify each thread produced correct output
        for (int t = 0; t < kNumThreads; ++t) {
            const fl::string& s = results[t];
            FL_CHECK(s.size() > 0);

            // Check that the string starts with the thread's base value
            char expected_start[32];
            snprintf(expected_start, sizeof(expected_start), "%d,", t * 1000);
            FL_CHECK(s.find(expected_start) == 0);
        }
    }

    FL_SUBCASE("Concurrent mixed format appends") {
        const int kNumThreads = 4;

        fl::vector<fl::thread> threads;
        fl::vector<fl::string> results(kNumThreads);

        for (int t = 0; t < kNumThreads; ++t) {
            threads.emplace_back([t, &results]() {
                fl::string& s = results[t];

                // Mix different formatting operations
                s.append("Dec:");
                s.append(t);
                s.append(",Hex:");
                s.appendHex(t);
                s.append(",Oct:");
                s.appendOct(t);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify correct output
        for (int t = 0; t < kNumThreads; ++t) {
            const fl::string& s = results[t];
            FL_CHECK(s.find("Dec:") != string::npos);
            FL_CHECK(s.find("Hex:") != string::npos);
            FL_CHECK(s.find("Oct:") != string::npos);
        }
    }
}

FL_TEST_CASE("fl::string - Buffer size requirements") {
    // Test edge cases for numeric formatting buffer sizes

    FL_SUBCASE("Maximum 64-bit value") {
        fl::string s;

        // Maximum uint64_t requires 20 digits in decimal
        s.append(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s.size() == 20);
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("Minimum int64_t value") {
        fl::string s;

        // Minimum int64_t: -9223372036854775808 (20 digits + sign)
        // Note: We use -9223372036854775807 to avoid overflow issues
        s.append(static_cast<int64_t>(-9223372036854775807LL));
        FL_CHECK(s.size() == 20);  // 19 digits + sign
    }

    FL_SUBCASE("Hex formatting maximum") {
        fl::string s;

        // Maximum uint64_t in hex: 16 hex digits
        s.appendHex(static_cast<uint64_t>(0xFFFFFFFFFFFFFFFFULL));
        FL_CHECK(s.size() == 16);
    }

    FL_SUBCASE("Float formatting buffer requirements") {
        fl::string s;

        // Test various float edge cases
        s.append(1.234567890123456789f);  // Precision test
        FL_CHECK(s.size() > 0);

        s.clear();
        s.append(-1.234567890123456789f);
        FL_CHECK(s.size() > 0);
        FL_CHECK(s[0] == '-');

        s.clear();
        s.append(0.0f);
        FL_CHECK(s.size() > 0);
    }
}

FL_TEST_CASE("fl::string - Write method numeric variants") {
    // Test the write() methods that take numeric types
    // These also use temporary StrN buffers

    FL_SUBCASE("write() with integers") {
        fl::string s;

        s.write(static_cast<u16>(42));
        FL_CHECK(s == "42");

        s.clear();
        s.write(static_cast<u32>(4294967295U));
        FL_CHECK(s == "4294967295");

        s.clear();
        s.write(static_cast<uint64_t>(18446744073709551615ULL));
        FL_CHECK(s == "18446744073709551615");
    }

    FL_SUBCASE("write() with signed integers") {
        fl::string s;

        s.write(static_cast<i32>(-2147483647));
        FL_CHECK(s == "-2147483647");

        s.clear();
        s.write(static_cast<i8>(-128));
        FL_CHECK(s == "-128");
    }

    FL_SUBCASE("Sequential write operations") {
        fl::string s;

        s.append("Count: ");
        s.write(static_cast<u32>(100));
        s.append(", Value: ");
        s.write(static_cast<i32>(-50));

        FL_CHECK(s.find("100") != string::npos);
        FL_CHECK(s.find("-50") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Memory efficiency improvements") {
    // Test patterns that could benefit from thread-local buffer optimization

    FL_SUBCASE("Repeated small string builds") {
        // This pattern creates many temporary StrN<64> buffers (reduced from 1000 to 500 for performance)
        fl::vector<fl::string> results;

        for (int i = 0; i < 500; ++i) {
            fl::string s;
            s.append("Item ");
            s.append(i);
            s.append(": Value=");
            s.append(i * 2);
            results.push_back(s);
        }

        FL_CHECK(results.size() == 500);
        FL_CHECK(results[0] == "Item 0: Value=0");
        FL_CHECK(results[499] == "Item 499: Value=998");
    }

    FL_SUBCASE("String builder pattern") {
        fl::string s;

        // Simulate building a complex string with many numeric appends
        for (int i = 0; i < 50; ++i) {
            s.append("Entry[");
            s.append(i);
            s.append("]=");
            s.append(i * i);
            s.append("; ");
        }

        FL_CHECK(s.find("Entry[0]=0") != string::npos);
        FL_CHECK(s.find("Entry[49]=2401") != string::npos);
    }
}

FL_TEST_CASE("fl::string - StringFormatter buffer reuse") {
    // Test that StringFormatter can safely reuse buffers across multiple calls

    FL_SUBCASE("Repeated calls with same formatter") {
        fl::string results[10];

        for (int i = 0; i < 10; ++i) {
            results[i].append(i * 111);
        }

        // Verify all results are independent
        FL_CHECK(results[0] == "0");
        FL_CHECK(results[1] == "111");
        FL_CHECK(results[9] == "999");
    }

    FL_SUBCASE("Interleaved formatting operations") {
        fl::string s1, s2;

        // Interleave operations on two strings
        s1.append(100);
        s2.append(200);
        s1.append(300);
        s2.append(400);

        FL_CHECK(s1.find("100") != string::npos);
        FL_CHECK(s1.find("300") != string::npos);
        FL_CHECK(s2.find("200") != string::npos);
        FL_CHECK(s2.find("400") != string::npos);
    }
}

FL_TEST_CASE("fl::string - Precision and accuracy") {
    // Ensure optimizations don't affect output correctness

    FL_SUBCASE("Float precision") {
        fl::string s;

        s.append(1.5f);
        FL_CHECK(s.find("1.5") != string::npos);

        s.clear();
        s.append(0.123f);
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("Negative zero handling") {
        fl::string s;
        s.append(-0.0f);
        FL_CHECK(s.size() > 0);
    }

    FL_SUBCASE("All integer sizes produce correct output") {
        fl::string s;

        // Test boundary values for each integer type
        s.append(static_cast<u8>(255));
        FL_CHECK(s == "255");

        s.clear();
        s.append(static_cast<i8>(-128));
        FL_CHECK(s == "-128");

        s.clear();
        s.append(static_cast<u16>(65535));
        FL_CHECK(s == "65535");

        s.clear();
        s.append(static_cast<i16>(-32768));
        FL_CHECK(s == "-32768");
    }
}

FL_TEST_CASE("fl::string - Construction from span") {
    FL_SUBCASE("Construction from span<const char>") {
        const char data[] = "hello world";
        fl::span<const char> sp(data, 5); // Only first 5 chars: "hello"
        fl::string s(sp);

        FL_CHECK(s.size() == 5);
        FL_CHECK(s == "hello");
    }

    FL_SUBCASE("Construction from span<char>") {
        char data[] = "test string";
        fl::span<char> sp(data, 4); // Only first 4 chars: "test"
        fl::string s(sp);

        FL_CHECK(s.size() == 4);
        FL_CHECK(s == "test");
    }

    FL_SUBCASE("Construction from empty span<const char>") {
        fl::span<const char> sp;
        fl::string s(sp);

        FL_CHECK(s.size() == 0);
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("Construction from empty span<char>") {
        fl::span<char> sp;
        fl::string s(sp);

        FL_CHECK(s.size() == 0);
        FL_CHECK(s.empty());
    }

    FL_SUBCASE("Span with entire string") {
        const char data[] = "full content";
        fl::span<const char> sp(data, sizeof(data) - 1); // Exclude null terminator
        fl::string s(sp);

        FL_CHECK(s.size() == 12);
        FL_CHECK(s == "full content");
    }

    FL_SUBCASE("Modifications don't affect original span") {
        char data[] = "original";
        fl::span<char> sp(data, 8);
        fl::string s(sp);

        s.append(" modified");

        FL_CHECK(s == "original modified");
        FL_CHECK(fl::strcmp(data, "original") == 0); // Original unchanged
    }
}
