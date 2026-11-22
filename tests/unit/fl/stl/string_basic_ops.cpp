// fl::string basic ops (part A1: construction, reserve, append, insert, erase, replace).
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstring.h"
#include "fl/stl/iterator.h"
#include "fl/stl/span.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
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

} // FL_TEST_FILE
