
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "crgb.h"
#include <sstream>

#include "fl/namespace.h"


TEST_CASE("Str basic operations") {
    SUBCASE("Construction and assignment") {
        Str s1;
        CHECK(s1.size() == 0);
        CHECK(s1.c_str()[0] == '\0');

        Str s2("hello");
        CHECK(s2.size() == 5);
        CHECK(fl::strcmp(s2.c_str(), "hello") == 0);

        Str s3 = s2;
        CHECK(s3.size() == 5);
        CHECK(fl::strcmp(s3.c_str(), "hello") == 0);

        s1 = "world";
        CHECK(s1.size() == 5);
        CHECK(fl::strcmp(s1.c_str(), "world") == 0);
    }

    SUBCASE("Comparison operators") {
        Str s1("hello");
        Str s2("hello");
        Str s3("world");

        CHECK(s1 == s2);
        CHECK(s1 != s3);
    }

    SUBCASE("Indexing") {
        Str s("hello");
        CHECK(s[0] == 'h');
        CHECK(s[4] == 'o');
        CHECK(s[5] == '\0');  // Null terminator
    }

    SUBCASE("Append") {
        Str s("hello");
        s.append(" world");
        CHECK(s.size() == 11);
        CHECK(fl::strcmp(s.c_str(), "hello world") == 0);
    }

    SUBCASE("CRGB to Str") {
        CRGB c(255, 0, 0);
        Str s = c.toString();
        CHECK_EQ(s, "CRGB(255,0,0)");
    }

    SUBCASE("Copy-on-write behavior") {
        Str s1("hello");
        Str s2 = s1;
        s2.append(" world");
        CHECK(fl::strcmp(s1.c_str(), "hello") == 0);
        CHECK(fl::strcmp(s2.c_str(), "hello world") == 0);
    }
}


TEST_CASE("Str::reserve") {
    Str s;
    s.reserve(10);
    CHECK(s.size() == 0);
    CHECK(s.capacity() >= 10);

    s.reserve(5);
    CHECK(s.size() == 0);
    CHECK(s.capacity() >= 10);

    s.reserve(500);
    CHECK(s.size() == 0);
    CHECK(s.capacity() >= 500);
    // s << "hello";
    s.append("hello");
    CHECK(s.size() == 5);
    CHECK_EQ(s, "hello");
}

TEST_CASE("Str with fl::FixedVector") {
    fl::FixedVector<Str, 10> vec;
    vec.push_back(Str("hello"));
    vec.push_back(Str("world"));

    CHECK(vec.size() == 2);
    CHECK(fl::strcmp(vec[0].c_str(), "hello") == 0);
    CHECK(fl::strcmp(vec[1].c_str(), "world") == 0);
}

TEST_CASE("Str with long strings") {
    const char* long_string = "This is a very long string that exceeds the inline buffer size and should be allocated on the heap";
    Str s(long_string);
    CHECK(s.size() == fl::strlen(long_string));
    CHECK(fl::strcmp(s.c_str(), long_string) == 0);

    Str s2 = s;
    CHECK(s2.size() == fl::strlen(long_string));
    CHECK(fl::strcmp(s2.c_str(), long_string) == 0);

    s2.append(" with some additional text");
    CHECK(fl::strcmp(s.c_str(), long_string) == 0);  // Original string should remain unchanged
}

TEST_CASE("Str overflowing inline data") {
    SUBCASE("Construction with long string") {
        std::string long_string(FASTLED_STR_INLINED_SIZE + 10, 'a');  // Create a string longer than the inline buffer
        Str s(long_string.c_str());
        CHECK(s.size() == long_string.length());
        CHECK(fl::strcmp(s.c_str(), long_string.c_str()) == 0);
    }

    SUBCASE("Appending to overflow") {
        Str s("Short string");
        std::string append_string(FASTLED_STR_INLINED_SIZE, 'b');  // String to append that will cause overflow
        s.append(append_string.c_str());
        CHECK(s.size() == fl::strlen("Short string") + append_string.length());
        CHECK(s[0] == 'S');
        CHECK(s[s.size() - 1] == 'b');
    }

    SUBCASE("Copy on write with long string") {
        std::string long_string(FASTLED_STR_INLINED_SIZE + 20, 'c');
        Str s1(long_string.c_str());
        Str s2 = s1;
        CHECK(s1.size() == s2.size());
        CHECK(fl::strcmp(s1.c_str(), s2.c_str()) == 0);

        s2.append("extra");
        CHECK(s1.size() == long_string.length());
        CHECK(s2.size() == long_string.length() + 5);
        CHECK(fl::strcmp(s1.c_str(), long_string.c_str()) == 0);
        CHECK(s2[s2.size() - 1] == 'a');
    }
}

TEST_CASE("String concatenation operators") {
    SUBCASE("String literal + fl::to_string") {
        // Test the specific case mentioned in the user query
        fl::string val = "string" + fl::to_string(5);
        CHECK(fl::strcmp(val.c_str(), "string5") == 0);
    }

    SUBCASE("fl::to_string + string literal") {
        fl::string val = fl::to_string(10) + " is a number";
        CHECK(fl::strcmp(val.c_str(), "10 is a number") == 0);
    }

    SUBCASE("String literal + fl::string") {
        fl::string str = "world";
        fl::string result = "Hello " + str;
        CHECK(fl::strcmp(result.c_str(), "Hello world") == 0);
    }

    SUBCASE("fl::string + string literal") {
        fl::string str = "Hello";
        fl::string result = str + " world";
        CHECK(fl::strcmp(result.c_str(), "Hello world") == 0);
    }

    SUBCASE("fl::string + fl::string") {
        fl::string str1 = "Hello";
        fl::string str2 = "World";
        fl::string result = str1 + " " + str2;
        CHECK(fl::strcmp(result.c_str(), "Hello World") == 0);
    }

    SUBCASE("Complex concatenation") {
        fl::string result = "Value: " + fl::to_string(42) + " and " + fl::to_string(3.14f);
        // Check that it contains the expected parts rather than exact match
        CHECK(result.find("Value: ") != fl::string::npos);
        CHECK(result.find("42") != fl::string::npos);
        CHECK(result.find("and") != fl::string::npos);
        CHECK(result.find("3.14") != fl::string::npos);
    }

    SUBCASE("Number + string literal") {
        fl::string result = fl::to_string(100) + " percent";
        CHECK(fl::strcmp(result.c_str(), "100 percent") == 0);
    }

    SUBCASE("String literal + number") {
        fl::string result = "Count: " + fl::to_string(7);
        CHECK(fl::strcmp(result.c_str(), "Count: 7") == 0);
    }
}

TEST_CASE("String insert operations") {
    SUBCASE("Insert character at beginning") {
        fl::string s = "world";
        s.insert(0, 1, 'H');
        CHECK_EQ(s, "Hworld");
        CHECK(s.size() == 6);
    }

    SUBCASE("Insert character in middle") {
        fl::string s = "helo";
        s.insert(2, 1, 'l');
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
    }

    SUBCASE("Insert character at end") {
        fl::string s = "hello";
        s.insert(5, 1, '!');
        CHECK_EQ(s, "hello!");
        CHECK(s.size() == 6);
    }

    SUBCASE("Insert multiple characters") {
        fl::string s = "hello";
        s.insert(5, 3, '!');
        CHECK_EQ(s, "hello!!!");
        CHECK(s.size() == 8);
    }

    SUBCASE("Insert c-string") {
        fl::string s = "hello";
        s.insert(5, " world");
        CHECK_EQ(s, "hello world");
        CHECK(s.size() == 11);
    }

    SUBCASE("Insert c-string at beginning") {
        fl::string s = "world";
        s.insert(0, "hello ");
        CHECK_EQ(s, "hello world");
    }

    SUBCASE("Insert partial c-string") {
        fl::string s = "hello";
        s.insert(5, " wonderful world", 10);
        CHECK_EQ(s, "hello wonderful");
    }

    SUBCASE("Insert fl::string") {
        fl::string s = "hello";
        fl::string insert_str = " world";
        s.insert(5, insert_str);
        CHECK_EQ(s, "hello world");
    }

    SUBCASE("Insert substring of fl::string") {
        fl::string s = "hello";
        fl::string insert_str = "the world";
        s.insert(5, insert_str, 3, 6);  // Insert " world"
        CHECK_EQ(s, "hello world");
    }

    SUBCASE("Insert substring with npos") {
        fl::string s = "hello";
        fl::string insert_str = "the world";
        s.insert(5, insert_str, 3);  // Insert " world" (to end)
        CHECK_EQ(s, "hello world");
    }

    SUBCASE("Insert causing inline to heap transition") {
        fl::string s = "short";
        // Create a long string that will cause heap allocation
        fl::string long_insert(FASTLED_STR_INLINED_SIZE, 'x');
        s.insert(5, long_insert);
        CHECK(s.size() == 5 + FASTLED_STR_INLINED_SIZE);
        CHECK(s[0] == 's');
        CHECK(s[5] == 'x');
    }

    SUBCASE("Insert on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 10, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Insert into s2 should trigger COW
        s2.insert(5, "XXX");

        // s1 should remain unchanged
        CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 10);
        for (fl::size i = 0; i < s1.size(); ++i) {
            CHECK(s1[i] == 'a');
        }

        // s2 should have the insertion
        CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 13);
        CHECK(s2[5] == 'X');
        CHECK(s2[6] == 'X');
        CHECK(s2[7] == 'X');
    }

    SUBCASE("Insert with invalid position clamped") {
        fl::string s = "hello";
        s.insert(100, " world");  // Position beyond end
        CHECK_EQ(s, "hello world");  // Should append at end
    }

    SUBCASE("Insert zero characters") {
        fl::string s = "hello";
        s.insert(2, 0, 'x');
        CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    SUBCASE("Insert empty string") {
        fl::string s = "hello";
        s.insert(2, "");
        CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    // Note: Iterator-based insert tests disabled due to ambiguity issues
    // They can be re-enabled once better type disambiguation is implemented
}

TEST_CASE("String erase operations") {
    SUBCASE("Erase from beginning") {
        fl::string s = "hello world";
        s.erase(0, 6);
        CHECK_EQ(s, "world");
        CHECK(s.size() == 5);
    }

    SUBCASE("Erase from middle") {
        fl::string s = "hello world";
        s.erase(5, 1);  // Remove the space
        CHECK_EQ(s, "helloworld");
        CHECK(s.size() == 10);
    }

    SUBCASE("Erase to end with npos") {
        fl::string s = "hello world";
        s.erase(5);  // Erase from position 5 to end (default count=npos)
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
    }

    SUBCASE("Erase to end explicit") {
        fl::string s = "hello world";
        s.erase(5, fl::string::npos);
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
    }

    SUBCASE("Erase entire string") {
        fl::string s = "hello";
        s.erase(0);
        CHECK_EQ(s, "");
        CHECK(s.size() == 0);
        CHECK(s.empty());
    }

    SUBCASE("Erase with count larger than remaining") {
        fl::string s = "hello world";
        s.erase(5, 100);  // Count exceeds string length
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
    }

    SUBCASE("Erase zero characters") {
        fl::string s = "hello";
        s.erase(2, 0);
        CHECK_EQ(s, "hello");  // Should remain unchanged
    }

    SUBCASE("Erase with invalid position") {
        fl::string s = "hello";
        s.erase(100, 5);  // Position beyond end
        CHECK_EQ(s, "hello");  // Should remain unchanged (no-op)
    }

    SUBCASE("Erase on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Erase from s2 should trigger COW
        s2.erase(5, 10);

        // s1 should remain unchanged
        CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 20);
        for (fl::size i = 0; i < s1.size(); ++i) {
            CHECK(s1[i] == 'a');
        }

        // s2 should have the erasure
        CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 10);
        for (fl::size i = 0; i < s2.size(); ++i) {
            CHECK(s2[i] == 'a');
        }
    }

    SUBCASE("Erase single character at position") {
        fl::string s = "hello";
        s.erase(1, 1);  // Remove 'e'
        CHECK_EQ(s, "hllo");
        CHECK(s.size() == 4);
    }

    // Note: Iterator-based erase tests commented out due to ambiguity issues
    // The problem is that erase(0) is ambiguous: 0 can be fl::size or char* (nullptr)
    // These can be re-enabled once better type disambiguation is implemented

    /*
    SUBCASE("Iterator-based erase single character") {
        fl::string s = "hello";
        char* it = s.begin() + 1;  // Point to 'e'
        char* result = s.erase(it);
        CHECK_EQ(s, "hllo");
        CHECK(s.size() == 4);
        // Result should point to 'l' (the character after erased 'e')
        CHECK(*result == 'l');
    }

    SUBCASE("Iterator-based erase range") {
        fl::string s = "hello world";
        char* first = s.begin() + 5;  // Point to space
        char* last = s.begin() + 11;  // Point to end
        char* result = s.erase(first, last);
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
        // Result should point to end
        CHECK(result == s.end());
    }

    SUBCASE("Iterator-based erase middle range") {
        fl::string s = "hello world";
        char* first = s.begin() + 2;  // Point to first 'l'
        char* last = s.begin() + 9;   // Point to 'r'
        s.erase(first, last);
        CHECK_EQ(s, "herld");
        CHECK(s.size() == 5);
    }

    SUBCASE("Iterator-based erase at beginning") {
        fl::string s = "hello";
        char* it = s.begin();
        s.erase(it);
        CHECK_EQ(s, "ello");
        CHECK(s.size() == 4);
    }

    SUBCASE("Iterator-based erase at end-1") {
        fl::string s = "hello";
        char* it = s.end() - 1;  // Point to 'o'
        s.erase(it);
        CHECK_EQ(s, "hell");
        CHECK(s.size() == 4);
    }
    */

    SUBCASE("Erase and verify null termination") {
        fl::string s = "hello world";
        s.erase(5);
        CHECK(s.c_str()[5] == '\0');
        CHECK(fl::strlen(s.c_str()) == s.size());
    }

    SUBCASE("Multiple consecutive erases") {
        fl::string s = "abcdefgh";
        s.erase(2, 2);  // Remove "cd" -> "abefgh"
        CHECK_EQ(s, "abefgh");
        s.erase(2, 2);  // Remove "ef" -> "abgh"
        CHECK_EQ(s, "abgh");
        s.erase(2, 2);  // Remove "gh" -> "ab"
        CHECK_EQ(s, "ab");
        CHECK(s.size() == 2);
    }
}

TEST_CASE("String replace operations") {
    SUBCASE("Replace with shorter string") {
        fl::string s = "hello world";
        s.replace(6, 5, "C++");  // Replace "world" with "C++"
        CHECK_EQ(s, "hello C++");
        CHECK(s.size() == 9);
    }

    SUBCASE("Replace with longer string") {
        fl::string s = "hello";
        s.replace(0, 5, "goodbye");  // Replace "hello" with "goodbye"
        CHECK_EQ(s, "goodbye");
        CHECK(s.size() == 7);
    }

    SUBCASE("Replace with equal length string") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");  // Replace "world" with "there"
        CHECK_EQ(s, "hello there");
        CHECK(s.size() == 11);
    }

    SUBCASE("Replace at beginning") {
        fl::string s = "hello world";
        s.replace(0, 5, "hi");  // Replace "hello" with "hi"
        CHECK_EQ(s, "hi world");
        CHECK(s.size() == 8);
    }

    SUBCASE("Replace in middle") {
        fl::string s = "hello world";
        s.replace(5, 1, "---");  // Replace space with "---"
        CHECK_EQ(s, "hello---world");
        CHECK(s.size() == 13);
    }

    SUBCASE("Replace to end with npos") {
        fl::string s = "hello world";
        s.replace(6, fl::string::npos, "everyone");  // Replace "world" to end
        CHECK_EQ(s, "hello everyone");
        CHECK(s.size() == 14);
    }

    SUBCASE("Replace entire string") {
        fl::string s = "hello";
        s.replace(0, 5, "goodbye world");
        CHECK_EQ(s, "goodbye world");
        CHECK(s.size() == 13);
    }

    SUBCASE("Replace with empty string (delete)") {
        fl::string s = "hello world";
        s.replace(5, 6, "");  // Remove " world"
        CHECK_EQ(s, "hello");
        CHECK(s.size() == 5);
    }

    SUBCASE("Replace with c-string") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");
        CHECK_EQ(s, "hello there");
    }

    SUBCASE("Replace with partial c-string") {
        fl::string s = "hello world";
        s.replace(6, 5, "wonderful place", 9);  // Use first 9 chars
        CHECK_EQ(s, "hello wonderful");
        CHECK(s.size() == 15);
    }

    SUBCASE("Replace with fl::string") {
        fl::string s = "hello world";
        fl::string replacement = "everyone";
        s.replace(6, 5, replacement);
        CHECK_EQ(s, "hello everyone");
    }

    SUBCASE("Replace with substring of fl::string") {
        fl::string s = "hello world";
        fl::string source = "the wonderful place";
        s.replace(6, 5, source, 4, 9);  // Use "wonderful"
        CHECK_EQ(s, "hello wonderful");
    }

    SUBCASE("Replace with substring using npos") {
        fl::string s = "hello world";
        fl::string source = "the wonderful";
        s.replace(6, 5, source, 4);  // Use "wonderful" to end
        CHECK_EQ(s, "hello wonderful");
    }

    SUBCASE("Replace with repeated character") {
        fl::string s = "hello world";
        s.replace(6, 5, 3, '!');  // Replace "world" with "!!!"
        CHECK_EQ(s, "hello !!!");
        CHECK(s.size() == 9);
    }

    SUBCASE("Replace with zero characters") {
        fl::string s = "hello world";
        s.replace(6, 5, 0, 'x');  // Replace "world" with nothing
        CHECK_EQ(s, "hello ");
        CHECK(s.size() == 6);
    }

    SUBCASE("Replace with count larger than string") {
        fl::string s = "hello world";
        s.replace(6, 100, "everyone");  // Count exceeds string length
        CHECK_EQ(s, "hello everyone");
    }

    SUBCASE("Replace causing heap growth") {
        fl::string s = "hello";
        // Create a long replacement string
        fl::string long_replacement(FASTLED_STR_INLINED_SIZE, 'x');
        s.replace(0, 5, long_replacement);
        CHECK(s.size() == FASTLED_STR_INLINED_SIZE);
        CHECK(s[0] == 'x');
        CHECK(s[FASTLED_STR_INLINED_SIZE - 1] == 'x');
    }

    SUBCASE("Replace on shared heap data (COW test)") {
        // Create a string that uses heap
        fl::string long_str(FASTLED_STR_INLINED_SIZE + 20, 'a');
        fl::string s1 = long_str;
        fl::string s2 = s1;  // Share heap data

        // Replace in s2 should trigger COW
        s2.replace(5, 10, "XXX");

        // s1 should remain unchanged
        CHECK(s1.size() == FASTLED_STR_INLINED_SIZE + 20);
        for (fl::size i = 0; i < s1.size(); ++i) {
            CHECK(s1[i] == 'a');
        }

        // s2 should have the replacement
        CHECK(s2.size() == FASTLED_STR_INLINED_SIZE + 13);  // -10 + 3
        CHECK(s2[5] == 'X');
        CHECK(s2[6] == 'X');
        CHECK(s2[7] == 'X');
        CHECK(s2[8] == 'a');
    }

    SUBCASE("Replace with invalid position") {
        fl::string s = "hello world";
        s.replace(100, 5, "test");  // Position beyond end
        CHECK_EQ(s, "hello world");  // Should remain unchanged (no-op)
    }

    SUBCASE("Replace zero count at position") {
        fl::string s = "hello world";
        s.replace(5, 0, "XXX");  // Replace nothing, effectively insert
        CHECK_EQ(s, "helloXXX world");
        CHECK(s.size() == 14);
    }

    SUBCASE("Replace and verify null termination") {
        fl::string s = "hello world";
        s.replace(6, 5, "there");
        CHECK(s.c_str()[11] == '\0');
        CHECK(fl::strlen(s.c_str()) == s.size());
    }

    SUBCASE("Multiple consecutive replaces") {
        fl::string s = "hello world";
        s.replace(0, 5, "hi");      // "hi world"
        CHECK_EQ(s, "hi world");
        s.replace(3, 5, "there");   // "hi there"
        CHECK_EQ(s, "hi there");
        s.replace(0, 2, "hello");   // "hello there"
        CHECK_EQ(s, "hello there");
        CHECK(s.size() == 11);
    }

    SUBCASE("Replace shrinking inline string") {
        fl::string s = "hello world";
        s.replace(6, 5, "!");  // Replace "world" with "!"
        CHECK_EQ(s, "hello !");
        CHECK(s.size() == 7);
    }

    SUBCASE("Replace growing inline string") {
        fl::string s = "hi";
        s.replace(0, 2, "hello world");
        CHECK_EQ(s, "hello world");
        CHECK(s.size() == 11);
    }

    SUBCASE("Replace with same content") {
        fl::string s = "hello world";
        s.replace(0, 5, "hello");
        CHECK_EQ(s, "hello world");
        CHECK(s.size() == 11);
    }

    SUBCASE("Replace at end position") {
        fl::string s = "hello";
        s.replace(5, 0, " world");  // Insert at end
        CHECK_EQ(s, "hello world");
        CHECK(s.size() == 11);
    }

    SUBCASE("Replace with null pointer (should erase)") {
        fl::string s = "hello world";
        s.replace(6, 5, static_cast<const char*>(nullptr));
        CHECK_EQ(s, "hello ");
        CHECK(s.size() == 6);
    }

    // Note: Iterator-based replace tests disabled due to ambiguity issues
    // (same as insert/erase iterator variants)
}

TEST_CASE("String rfind operations") {
    SUBCASE("rfind character in string") {
        fl::string s = "hello world";
        CHECK(s.rfind('o') == 7);  // Last 'o' in "world"
        CHECK(s.rfind('l') == 9);  // Last 'l' in "world"
        CHECK(s.rfind('h') == 0);  // First and only 'h'
        CHECK(s.rfind('x') == fl::string::npos);  // Not found
    }

    SUBCASE("rfind character from specific position") {
        fl::string s = "hello world";
        CHECK(s.rfind('o', 10) == 7);  // Search from pos 10, find 'o' at 7
        CHECK(s.rfind('o', 7) == 7);   // Search from pos 7, find 'o' at 7
        CHECK(s.rfind('o', 6) == 4);   // Search from pos 6, find 'o' at 4 in "hello"
        CHECK(s.rfind('l', 3) == 3);   // Find 'l' at position 3
        CHECK(s.rfind('l', 2) == 2);   // Find 'l' at position 2
        CHECK(s.rfind('h', 0) == 0);   // Find 'h' at position 0
    }

    SUBCASE("rfind character with pos beyond string length") {
        fl::string s = "hello";
        CHECK(s.rfind('o', 100) == 4);  // Should search from end
        CHECK(s.rfind('h', 1000) == 0); // Should find 'h' at start
    }

    SUBCASE("rfind character in empty string") {
        fl::string s = "";
        CHECK(s.rfind('x') == fl::string::npos);
        CHECK(s.rfind('x', 0) == fl::string::npos);
    }

    SUBCASE("rfind substring") {
        fl::string s = "hello world hello";
        CHECK(s.rfind("hello") == 12);  // Last occurrence
        CHECK(s.rfind("world") == 6);   // Only occurrence
        CHECK(s.rfind("o w") == 4);     // Substring spanning words
        CHECK(s.rfind("xyz") == fl::string::npos);  // Not found
    }

    SUBCASE("rfind substring with position") {
        fl::string s = "hello world hello";
        CHECK(s.rfind("hello", 15) == 12);  // Find last "hello"
        CHECK(s.rfind("hello", 11) == 0);   // Find first "hello" (search before last one)
        CHECK(s.rfind("world", 10) == 6);   // Find "world"
        CHECK(s.rfind("world", 5) == fl::string::npos);  // Can't find before position 6
    }

    SUBCASE("rfind with c-string and count") {
        fl::string s = "hello world";
        CHECK(s.rfind("world", fl::string::npos, 5) == 6);  // Full match
        CHECK(s.rfind("world", fl::string::npos, 3) == 6);  // Match "wor"
        CHECK(s.rfind("world", 10, 3) == 6);  // Match "wor" from position 10
        CHECK(s.rfind("hello", 10, 3) == 0);  // Match "hel"
    }

    SUBCASE("rfind empty string") {
        fl::string s = "hello";
        CHECK(s.rfind("") == 5);  // Empty string matches at end
        CHECK(s.rfind("", 2) == 2);  // Empty string matches at position
        CHECK(s.rfind("", 10) == 5);  // Position beyond end returns length
        CHECK(s.rfind("", fl::string::npos, 0) == 5);  // Empty with count=0
    }

    SUBCASE("rfind fl::string") {
        fl::string s = "hello world hello";
        fl::string pattern1 = "hello";
        fl::string pattern2 = "world";
        fl::string pattern3 = "xyz";

        CHECK(s.rfind(pattern1) == 12);  // Last "hello"
        CHECK(s.rfind(pattern2) == 6);   // "world"
        CHECK(s.rfind(pattern3) == fl::string::npos);  // Not found
    }

    SUBCASE("rfind fl::string with position") {
        fl::string s = "hello world hello";
        fl::string pattern = "hello";

        CHECK(s.rfind(pattern, 15) == 12);  // Last occurrence
        CHECK(s.rfind(pattern, 11) == 0);   // First occurrence
        CHECK(s.rfind(pattern, 5) == 0);    // Before second occurrence
    }

    SUBCASE("rfind at beginning of string") {
        fl::string s = "hello world";
        CHECK(s.rfind("hello") == 0);
        CHECK(s.rfind('h') == 0);
    }

    SUBCASE("rfind at end of string") {
        fl::string s = "hello world";
        CHECK(s.rfind('d') == 10);
        CHECK(s.rfind("world") == 6);
        CHECK(s.rfind("ld") == 9);
    }

    SUBCASE("rfind single character string") {
        fl::string s = "hello";
        CHECK(s.rfind("o") == 4);
        CHECK(s.rfind("h") == 0);
    }

    SUBCASE("rfind with repeated pattern") {
        fl::string s = "aaaaaaa";
        CHECK(s.rfind('a') == 6);  // Last 'a'
        CHECK(s.rfind('a', 3) == 3);  // 'a' at position 3
        CHECK(s.rfind("aa") == 5);  // Last occurrence of "aa"
        CHECK(s.rfind("aaa") == 4);  // Last occurrence of "aaa"
    }

    SUBCASE("rfind substring longer than string") {
        fl::string s = "hi";
        CHECK(s.rfind("hello") == fl::string::npos);
        CHECK(s.rfind("hello world") == fl::string::npos);
    }

    SUBCASE("rfind on inline string") {
        fl::string s = "short";
        CHECK(s.rfind('o') == 2);
        CHECK(s.rfind("ort") == 2);
        CHECK(s.rfind('s') == 0);
    }

    SUBCASE("rfind on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'a');
        s.replace(5, 1, "b");  // Put a 'b' at position 5
        s.replace(s.size() - 5, 1, "b");  // Put a 'b' near the end

        CHECK(s.rfind('b') == s.size() - 5);  // Find last 'b'
        CHECK(s.rfind('b', s.size() - 6) == 5);  // Find first 'b'
        CHECK(s.rfind('a') == s.size() - 1);  // Last 'a'
    }

    SUBCASE("rfind with overlapping matches") {
        fl::string s = "aaaa";
        CHECK(s.rfind("aa") == 2);  // Last possible match at position 2
        CHECK(s.rfind("aa", 1) == 1);  // Match at position 1
        CHECK(s.rfind("aa", 0) == 0);  // Match at position 0
    }

    SUBCASE("rfind case sensitive") {
        fl::string s = "Hello World";
        CHECK(s.rfind('h') == fl::string::npos);  // Lowercase 'h' not found
        CHECK(s.rfind('H') == 0);  // Uppercase 'H' found
        CHECK(s.rfind("hello") == fl::string::npos);  // Case mismatch
        CHECK(s.rfind("Hello") == 0);  // Exact match
    }

    SUBCASE("rfind with null terminator in count") {
        fl::string s = "hello\0world";  // Contains embedded null
        // Note: string is actually "hello" due to constructor behavior
        CHECK(s.size() == 5);  // Only "hello" is stored
        CHECK(s.rfind("hello") == 0);
    }

    SUBCASE("rfind comparison with find") {
        fl::string s = "test";
        // For strings with unique characters, rfind should equal find
        CHECK(s.rfind('t') == 3);  // Last 't'
        CHECK(s.find('t') == 0);   // First 't'

        fl::string s2 = "unique";
        CHECK(s2.rfind('u') == 4);  // Last 'u'
        CHECK(s2.find('u') == 0);   // First 'u'
    }

    SUBCASE("rfind with position 0") {
        fl::string s = "hello world";
        CHECK(s.rfind('h', 0) == 0);  // Can find at position 0
        CHECK(s.rfind("hello", 0) == 0);  // Can find at position 0
        CHECK(s.rfind('e', 0) == fl::string::npos);  // 'e' is after position 0
        CHECK(s.rfind("world", 0) == fl::string::npos);  // "world" is after position 0
    }

    SUBCASE("rfind performance - multiple occurrences") {
        fl::string s = "the quick brown fox jumps over the lazy dog";
        CHECK(s.rfind("the") == 31);  // Last occurrence of "the" in "the lazy"
        CHECK(s.rfind("the", 30) == 0);  // First occurrence of "the" (before position 31)
        CHECK(s.rfind(' ') == 39);  // Last space (before "dog")
        CHECK(s.rfind('o') == 41);  // Last 'o' in "dog"
    }
}

TEST_CASE("String find_first_of operations") {
    SUBCASE("find_first_of with character set") {
        fl::string s = "hello world";
        CHECK(s.find_first_of("aeiou") == 1);  // 'e' at position 1
        CHECK(s.find_first_of("xyz") == fl::string::npos);  // None found
        CHECK(s.find_first_of("wo") == 4);  // 'o' in "hello" at position 4
    }

    SUBCASE("find_first_of single character") {
        fl::string s = "hello world";
        CHECK(s.find_first_of('o') == 4);  // First 'o'
        CHECK(s.find_first_of('h') == 0);  // At beginning
        CHECK(s.find_first_of('d') == 10);  // At end
        CHECK(s.find_first_of('x') == fl::string::npos);  // Not found
    }

    SUBCASE("find_first_of with position offset") {
        fl::string s = "hello world";
        CHECK(s.find_first_of("aeiou", 0) == 1);  // 'e' from start
        CHECK(s.find_first_of("aeiou", 2) == 4);  // 'o' at position 4
        CHECK(s.find_first_of("aeiou", 5) == 7);  // 'o' in "world" at position 7
        CHECK(s.find_first_of("aeiou", 8) == fl::string::npos);  // No vowels after 'o'
    }

    SUBCASE("find_first_of beyond string length") {
        fl::string s = "hello";
        CHECK(s.find_first_of("aeiou", 100) == fl::string::npos);
        CHECK(s.find_first_of('o', 100) == fl::string::npos);
    }

    SUBCASE("find_first_of in empty string") {
        fl::string s = "";
        CHECK(s.find_first_of("abc") == fl::string::npos);
        CHECK(s.find_first_of('x') == fl::string::npos);
        CHECK(s.find_first_of("") == fl::string::npos);
    }

    SUBCASE("find_first_of with empty set") {
        fl::string s = "hello";
        CHECK(s.find_first_of("") == fl::string::npos);
        CHECK(s.find_first_of("", 0, 0) == fl::string::npos);
    }

    SUBCASE("find_first_of with null pointer") {
        fl::string s = "hello";
        CHECK(s.find_first_of(static_cast<const char*>(nullptr)) == fl::string::npos);
    }

    SUBCASE("find_first_of with counted string") {
        fl::string s = "hello world";
        CHECK(s.find_first_of("aeiou", 0, 3) == 1);  // Search for "aei", find 'e'
        CHECK(s.find_first_of("xyz", 0, 2) == fl::string::npos);  // Search for "xy"
        CHECK(s.find_first_of("world", 0, 1) == 6);  // Search for "w", found at position 6
    }

    SUBCASE("find_first_of with fl::string") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";
        fl::string consonants = "bcdfghjklmnpqrstvwxyz";
        fl::string digits = "0123456789";

        CHECK(s.find_first_of(vowels) == 1);  // 'e' at position 1
        CHECK(s.find_first_of(consonants) == 0);  // 'h' at position 0
        CHECK(s.find_first_of(digits) == fl::string::npos);  // No digits
    }

    SUBCASE("find_first_of with fl::string and position") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";

        CHECK(s.find_first_of(vowels, 0) == 1);  // 'e' from start
        CHECK(s.find_first_of(vowels, 2) == 4);  // 'o' at position 4
        CHECK(s.find_first_of(vowels, 5) == 7);  // 'o' in "world"
    }

    SUBCASE("find_first_of whitespace") {
        fl::string s = "hello world";
        CHECK(s.find_first_of(" \t\n") == 5);  // Space at position 5

        fl::string s2 = "no-spaces-here";
        CHECK(s2.find_first_of(" \t\n") == fl::string::npos);
    }

    SUBCASE("find_first_of digits in mixed string") {
        fl::string s = "abc123def456";
        CHECK(s.find_first_of("0123456789") == 3);  // '1' at position 3
        CHECK(s.find_first_of("0123456789", 4) == 4);  // '2' at position 4
        CHECK(s.find_first_of("0123456789", 6) == 9);  // '4' at position 9
    }

    SUBCASE("find_first_of punctuation") {
        fl::string s = "hello, world!";
        CHECK(s.find_first_of(",.;:!?") == 5);  // ',' at position 5
        CHECK(s.find_first_of(",.;:!?", 6) == 12);  // '!' at position 12
    }

    SUBCASE("find_first_of case sensitive") {
        fl::string s = "Hello World";
        CHECK(s.find_first_of("h") == fl::string::npos);  // Lowercase 'h' not found
        CHECK(s.find_first_of("H") == 0);  // Uppercase 'H' found
        CHECK(s.find_first_of("hH") == 0);  // Either case, finds 'H'
    }

    SUBCASE("find_first_of with repeated characters in set") {
        fl::string s = "hello world";
        CHECK(s.find_first_of("ooo") == 4);  // Duplicates in set don't matter
        CHECK(s.find_first_of("llllll") == 2);  // First 'l' at position 2
    }

    SUBCASE("find_first_of all characters match") {
        fl::string s = "aaaa";
        CHECK(s.find_first_of("a") == 0);  // First match at start
        CHECK(s.find_first_of("a", 1) == 1);  // From position 1
        CHECK(s.find_first_of("a", 3) == 3);  // From position 3
    }

    SUBCASE("find_first_of no characters match") {
        fl::string s = "hello";
        CHECK(s.find_first_of("xyz") == fl::string::npos);
        CHECK(s.find_first_of("123") == fl::string::npos);
        CHECK(s.find_first_of("XYZ") == fl::string::npos);
    }

    SUBCASE("find_first_of at string boundaries") {
        fl::string s = "hello";
        CHECK(s.find_first_of("h") == 0);  // First character
        CHECK(s.find_first_of("o") == 4);  // Last character
        CHECK(s.find_first_of("ho") == 0);  // Either boundary
    }

    SUBCASE("find_first_of with special characters") {
        fl::string s = "path/to/file.txt";
        CHECK(s.find_first_of("/\\") == 4);  // First '/' or '\'
        CHECK(s.find_first_of(".") == 12);  // First '.'
        CHECK(s.find_first_of("/.", 5) == 7);  // Next '/' or '.' after position 5
    }

    SUBCASE("find_first_of for tokenization") {
        fl::string s = "word1,word2;word3:word4";
        CHECK(s.find_first_of(",;:") == 5);  // First delimiter ','
        CHECK(s.find_first_of(",;:", 6) == 11);  // Second delimiter ';'
        CHECK(s.find_first_of(",;:", 12) == 17);  // Third delimiter ':'
    }

    SUBCASE("find_first_of on inline string") {
        fl::string s = "short";
        CHECK(s.find_first_of("aeiou") == 2);  // 'o' at position 2
        CHECK(s.find_first_of("xyz") == fl::string::npos);
    }

    SUBCASE("find_first_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "a");  // Put an 'a' at position 10
        s.replace(50, 1, "b");  // Put a 'b' at position 50

        CHECK(s.find_first_of("ab") == 10);  // Find 'a' at position 10
        CHECK(s.find_first_of("ab", 11) == 50);  // Find 'b' at position 50
        CHECK(s.find_first_of("ab", 51) == fl::string::npos);  // No more matches
    }

    SUBCASE("find_first_of comparison with find") {
        fl::string s = "hello world";
        // For single character, find_first_of should equal find
        CHECK(s.find_first_of('o') == s.find('o'));
        CHECK(s.find_first_of('h') == s.find('h'));
        CHECK(s.find_first_of('x') == s.find('x'));
    }

    SUBCASE("find_first_of from each position") {
        fl::string s = "abcdef";
        CHECK(s.find_first_of("cf", 0) == 2);  // 'c' at position 2
        CHECK(s.find_first_of("cf", 1) == 2);  // 'c' at position 2
        CHECK(s.find_first_of("cf", 2) == 2);  // 'c' at position 2
        CHECK(s.find_first_of("cf", 3) == 5);  // 'f' at position 5
        CHECK(s.find_first_of("cf", 4) == 5);  // 'f' at position 5
        CHECK(s.find_first_of("cf", 5) == 5);  // 'f' at position 5
        CHECK(s.find_first_of("cf", 6) == fl::string::npos);  // Past end
    }

    SUBCASE("find_first_of with entire alphabet") {
        fl::string s = "123 hello";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        CHECK(s.find_first_of(alphabet) == 4);  // 'h' at position 4
    }

    SUBCASE("find_first_of realistic use case - trimming") {
        fl::string s = "   hello";
        CHECK(s.find_first_of("abcdefghijklmnopqrstuvwxyz") == 3);  // First letter at 3

        fl::string s2 = "\t\n  text";
        CHECK(s2.find_first_of("abcdefghijklmnopqrstuvwxyz") == 4);  // First letter at 4
    }
}

TEST_CASE("String find_last_of operations") {
    SUBCASE("find_last_of with character set") {
        fl::string s = "hello world";
        CHECK(s.find_last_of("aeiou") == 7);  // Last vowel 'o' in "world" at position 7
        CHECK(s.find_last_of("xyz") == fl::string::npos);  // None found
        CHECK(s.find_last_of("hl") == 9);  // Last 'l' at position 9
    }

    SUBCASE("find_last_of single character") {
        fl::string s = "hello world";
        CHECK(s.find_last_of('o') == 7);  // Last 'o' in "world"
        CHECK(s.find_last_of('h') == 0);  // Only 'h' at beginning
        CHECK(s.find_last_of('d') == 10);  // 'd' at end
        CHECK(s.find_last_of('x') == fl::string::npos);  // Not found
    }

    SUBCASE("find_last_of with position limit") {
        fl::string s = "hello world";
        CHECK(s.find_last_of("aeiou") == 7);  // Last 'o' from end
        CHECK(s.find_last_of("aeiou", 6) == 4);  // Last 'o' in "hello" at position 4
        CHECK(s.find_last_of("aeiou", 3) == 1);  // 'e' at position 1
        CHECK(s.find_last_of("aeiou", 0) == fl::string::npos);  // No vowels at position 0
    }

    SUBCASE("find_last_of with pos beyond string length") {
        fl::string s = "hello";
        CHECK(s.find_last_of("aeiou", 100) == 4);  // Should search from end, find 'o'
        CHECK(s.find_last_of('o', 1000) == 4);  // Should find 'o' at position 4
    }

    SUBCASE("find_last_of with pos = npos") {
        fl::string s = "hello world";
        CHECK(s.find_last_of("aeiou", fl::string::npos) == 7);  // Search from end
        CHECK(s.find_last_of('l', fl::string::npos) == 9);  // Last 'l'
    }

    SUBCASE("find_last_of in empty string") {
        fl::string s = "";
        CHECK(s.find_last_of("abc") == fl::string::npos);
        CHECK(s.find_last_of('x') == fl::string::npos);
        CHECK(s.find_last_of("") == fl::string::npos);
    }

    SUBCASE("find_last_of with empty set") {
        fl::string s = "hello";
        CHECK(s.find_last_of("") == fl::string::npos);
        CHECK(s.find_last_of("", fl::string::npos, 0) == fl::string::npos);
    }

    SUBCASE("find_last_of with null pointer") {
        fl::string s = "hello";
        CHECK(s.find_last_of(static_cast<const char*>(nullptr)) == fl::string::npos);
    }

    SUBCASE("find_last_of with counted string") {
        fl::string s = "hello world";
        // With "aeiou" and count=3, search for "aei" (first 3 chars)
        // In "hello world", 'e' at position 1 is the last occurrence of any char from "aei"
        CHECK(s.find_last_of("aeiou", fl::string::npos, 3) == 1);  // Search for "aei", last is 'e' at position 1
        CHECK(s.find_last_of("world", fl::string::npos, 1) == 6);  // Search for "w", found at position 6
    }

    SUBCASE("find_last_of with fl::string") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";
        fl::string consonants = "bcdfghjklmnpqrstvwxyz";
        fl::string digits = "0123456789";

        CHECK(s.find_last_of(vowels) == 7);  // Last 'o' at position 7
        CHECK(s.find_last_of(consonants) == 10);  // Last 'd' at position 10
        CHECK(s.find_last_of(digits) == fl::string::npos);  // No digits
    }

    SUBCASE("find_last_of with fl::string and position") {
        fl::string s = "hello world";
        fl::string vowels = "aeiou";

        CHECK(s.find_last_of(vowels) == 7);  // Last 'o' from end
        CHECK(s.find_last_of(vowels, 6) == 4);  // Last vowel at or before position 6 is 'o' at 4
        CHECK(s.find_last_of(vowels, 3) == 1);  // Last vowel at or before position 3 is 'e' at 1
    }

    SUBCASE("find_last_of whitespace") {
        fl::string s = "hello world test";
        CHECK(s.find_last_of(" \t\n") == 11);  // Last space at position 11

        fl::string s2 = "no-spaces-here";
        CHECK(s2.find_last_of(" \t\n") == fl::string::npos);
    }

    SUBCASE("find_last_of digits in mixed string") {
        fl::string s = "abc123def456";
        CHECK(s.find_last_of("0123456789") == 11);  // Last digit '6' at position 11
        CHECK(s.find_last_of("0123456789", 8) == 5);  // Last digit at or before 8 is '3' at position 5
        CHECK(s.find_last_of("0123456789", 2) == fl::string::npos);  // No digits before position 3
    }

    SUBCASE("find_last_of punctuation") {
        fl::string s = "hello, world!";
        CHECK(s.find_last_of(",.;:!?") == 12);  // Last '!' at position 12
        CHECK(s.find_last_of(",.;:!?", 11) == 5);  // ',' at position 5
    }

    SUBCASE("find_last_of case sensitive") {
        fl::string s = "Hello World";
        CHECK(s.find_last_of("h") == fl::string::npos);  // Lowercase 'h' not found
        CHECK(s.find_last_of("H") == 0);  // Uppercase 'H' found
        CHECK(s.find_last_of("hH") == 0);  // Either case, finds 'H'
    }

    SUBCASE("find_last_of with repeated characters in set") {
        fl::string s = "hello world";
        CHECK(s.find_last_of("ooo") == 7);  // Duplicates in set don't matter
        CHECK(s.find_last_of("llllll") == 9);  // Last 'l' at position 9
    }

    SUBCASE("find_last_of all characters match") {
        fl::string s = "aaaa";
        CHECK(s.find_last_of("a") == 3);  // Last match at end
        CHECK(s.find_last_of("a", 2) == 2);  // From position 2
        CHECK(s.find_last_of("a", 0) == 0);  // From position 0
    }

    SUBCASE("find_last_of no characters match") {
        fl::string s = "hello";
        CHECK(s.find_last_of("xyz") == fl::string::npos);
        CHECK(s.find_last_of("123") == fl::string::npos);
        CHECK(s.find_last_of("XYZ") == fl::string::npos);
    }

    SUBCASE("find_last_of at string boundaries") {
        fl::string s = "hello";
        CHECK(s.find_last_of("h") == 0);  // First character (also last occurrence)
        CHECK(s.find_last_of("o") == 4);  // Last character
        CHECK(s.find_last_of("ho") == 4);  // Last occurrence is 'o'
    }

    SUBCASE("find_last_of with special characters") {
        fl::string s = "path/to/file.txt";
        CHECK(s.find_last_of("/\\") == 7);  // Last '/' at position 7
        CHECK(s.find_last_of(".") == 12);  // Last '.' at position 12
        CHECK(s.find_last_of("/.") == 12);  // Last '/' or '.' is '.' at position 12
    }

    SUBCASE("find_last_of for reverse tokenization") {
        fl::string s = "word1,word2;word3:word4";
        CHECK(s.find_last_of(",;:") == 17);  // Last delimiter ':' at position 17
        CHECK(s.find_last_of(",;:", 16) == 11);  // Previous delimiter ';' at position 11
        CHECK(s.find_last_of(",;:", 10) == 5);  // First delimiter ',' at position 5
    }

    SUBCASE("find_last_of on inline string") {
        fl::string s = "short";
        CHECK(s.find_last_of("aeiou") == 2);  // Last (and only) vowel 'o' at position 2
        CHECK(s.find_last_of("xyz") == fl::string::npos);
    }

    SUBCASE("find_last_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "a");  // Put an 'a' at position 10
        s.replace(50, 1, "b");  // Put a 'b' at position 50

        CHECK(s.find_last_of("ab") == 50);  // Last match is 'b' at position 50
        CHECK(s.find_last_of("ab", 49) == 10);  // Before position 50, 'a' at position 10
        CHECK(s.find_last_of("ab", 9) == fl::string::npos);  // No matches before position 10
    }

    SUBCASE("find_last_of comparison with rfind") {
        fl::string s = "hello world";
        // For single character, find_last_of should equal rfind
        CHECK(s.find_last_of('o') == s.rfind('o'));
        CHECK(s.find_last_of('h') == s.rfind('h'));
        CHECK(s.find_last_of('l') == s.rfind('l'));
        CHECK(s.find_last_of('x') == s.rfind('x'));
    }

    SUBCASE("find_last_of from each position") {
        fl::string s = "abcdef";
        CHECK(s.find_last_of("cf", 5) == 5);  // 'f' at position 5
        CHECK(s.find_last_of("cf", 4) == 2);  // 'c' at position 2
        CHECK(s.find_last_of("cf", 3) == 2);  // 'c' at position 2
        CHECK(s.find_last_of("cf", 2) == 2);  // 'c' at position 2
        CHECK(s.find_last_of("cf", 1) == fl::string::npos);  // No match at or before position 1
        CHECK(s.find_last_of("cf", 0) == fl::string::npos);  // No match at position 0
    }

    SUBCASE("find_last_of with entire alphabet") {
        fl::string s = "123 hello 456";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        CHECK(s.find_last_of(alphabet) == 8);  // Last letter 'o' at position 8
    }

    SUBCASE("find_last_of realistic use case - trailing whitespace") {
        fl::string s = "hello   ";
        CHECK(s.find_last_of("abcdefghijklmnopqrstuvwxyz") == 4);  // Last letter 'o' at position 4

        fl::string s2 = "text\t\n  ";
        CHECK(s2.find_last_of("abcdefghijklmnopqrstuvwxyz") == 3);  // Last letter 't' at position 3
    }

    SUBCASE("find_last_of with overlapping character sets") {
        fl::string s = "hello123world456";
        CHECK(s.find_last_of("0123456789") == 15);  // Last digit '6'
        CHECK(s.find_last_of("abcdefghijklmnopqrstuvwxyz") == 12);  // Last letter 'd'
        CHECK(s.find_last_of("0123456789abcdefghijklmnopqrstuvwxyz") == 15);  // Last alphanumeric
    }

    SUBCASE("find_last_of at position 0") {
        fl::string s = "hello world";
        CHECK(s.find_last_of('h', 0) == 0);  // Can find at position 0
        CHECK(s.find_last_of("h", 0) == 0);  // Can find at position 0
        CHECK(s.find_last_of('e', 0) == fl::string::npos);  // 'e' is after position 0
        CHECK(s.find_last_of("world", 0) == fl::string::npos);  // No characters from "world" at position 0
    }

    SUBCASE("find_last_of with multiple occurrences") {
        fl::string s = "the quick brown fox jumps over the lazy dog";
        // Positions: "the quick brown fox jumps over the lazy dog"
        //             0123456789...
        // Last 'o' in "dog" is at position 41
        // Last space before "dog" is at position 39
        // Last occurrence of 't', 'h', or 'e' - 'e' in "the" at position 33
        CHECK(s.find_last_of("aeiou") == 41);  // Last vowel 'o' in "dog"
        CHECK(s.find_last_of(" ") == 39);  // Last space (before "dog")
        CHECK(s.find_last_of("the") == 33);  // Last 'e' in "the lazy" at position 33
    }

    SUBCASE("find_last_of single character string") {
        fl::string s = "hello";
        CHECK(s.find_last_of("o") == 4);
        CHECK(s.find_last_of("h") == 0);
        CHECK(s.find_last_of("l") == 3);
    }

    SUBCASE("find_last_of with repeated pattern") {
        fl::string s = "aaaaaaa";
        CHECK(s.find_last_of('a') == 6);  // Last 'a'
        CHECK(s.find_last_of('a', 3) == 3);  // 'a' at position 3
        CHECK(s.find_last_of('a', 0) == 0);  // 'a' at position 0
    }

    SUBCASE("find_last_of for file extension detection") {
        fl::string s = "file.backup.txt";
        CHECK(s.find_last_of(".") == 11);  // Last '.' before extension
        fl::size ext_pos = s.find_last_of(".");
        CHECK(s.substr(ext_pos + 1) == "txt");  // Extract extension
    }

    SUBCASE("find_last_of for path separator") {
        fl::string s = "C:\\path\\to\\file.txt";
        // String is: "C:\path\to\file.txt" - backslashes are interpreted
        // Actually the backslash escapes: C:\p\t\f.txt with special chars
        // Better test: just use forward slashes or raw positions
        // Positions: C(0) :(1) \(2) p(3) a(4) t(5) h(6) \(7) t(8) o(9) \(10)...
        CHECK(s.find_last_of("\\/") == 10);  // Last separator at position 10
    }

    SUBCASE("find_last_of comparison find_first_of") {
        fl::string s = "test string";
        fl::string charset = "st";
        // find_first_of finds first occurrence of any character from set
        // find_last_of finds last occurrence of any character from set
        // "test string" = positions: t(0)e(1)s(2)t(3) (4)s(5)t(6)r(7)i(8)n(9)g(10)
        CHECK(s.find_first_of(charset) == 0);  // First 't' at position 0
        CHECK(s.find_last_of(charset) == 6);  // Last 't' at position 6
    }
}

TEST_CASE("String find_first_not_of operations") {
    SUBCASE("find_first_not_of single character") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_first_not_of('a') == 3);  // First 'b' at position 3
        CHECK(s.find_first_not_of('b') == 0);  // First 'a' at position 0
        CHECK(s.find_first_not_of('x') == 0);  // First char (no match with 'x')
    }

    SUBCASE("find_first_not_of with character set") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_first_not_of("ab") == 6);  // First 'c' at position 6
        CHECK(s.find_first_not_of("abc") == fl::string::npos);  // All chars are in set
        CHECK(s.find_first_not_of("xyz") == 0);  // First char not in set
    }

    SUBCASE("find_first_not_of for trimming whitespace") {
        fl::string s = "   hello world";
        CHECK(s.find_first_not_of(" ") == 3);  // First non-space at position 3
        CHECK(s.find_first_not_of(" \t\n\r") == 3);  // First non-whitespace

        fl::string s2 = "\t\n  text";
        CHECK(s2.find_first_not_of(" \t\n\r") == 4);  // First non-whitespace at position 4
    }

    SUBCASE("find_first_not_of with position offset") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_first_not_of("a", 0) == 3);  // From start, first non-'a'
        CHECK(s.find_first_not_of("a", 3) == 3);  // From position 3, first non-'a' is at 3
        CHECK(s.find_first_not_of("b", 3) == 6);  // From position 3, first non-'b' is 'c' at 6
        CHECK(s.find_first_not_of("c", 6) == fl::string::npos);  // From position 6, all are 'c'
    }

    SUBCASE("find_first_not_of beyond string length") {
        fl::string s = "hello";
        CHECK(s.find_first_not_of("xyz", 100) == fl::string::npos);
        CHECK(s.find_first_not_of('x', 100) == fl::string::npos);
    }

    SUBCASE("find_first_not_of in empty string") {
        fl::string s = "";
        CHECK(s.find_first_not_of("abc") == fl::string::npos);
        CHECK(s.find_first_not_of('x') == fl::string::npos);
        CHECK(s.find_first_not_of("") == fl::string::npos);
    }

    SUBCASE("find_first_not_of with empty set") {
        fl::string s = "hello";
        // Empty set means every character is "not in the set"
        CHECK(s.find_first_not_of("") == 0);  // First char
        CHECK(s.find_first_not_of("", 0, 0) == 0);  // First char
        CHECK(s.find_first_not_of("", 2) == 2);  // From position 2
    }

    SUBCASE("find_first_not_of with null pointer") {
        fl::string s = "hello";
        // Null pointer means every character is "not in the set"
        CHECK(s.find_first_not_of(static_cast<const char*>(nullptr)) == 0);
        CHECK(s.find_first_not_of(static_cast<const char*>(nullptr), 2) == 2);
    }

    SUBCASE("find_first_not_of with counted string") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_first_not_of("abc", 0, 2) == 6);  // Search for NOT "ab", find 'c' at position 6
        CHECK(s.find_first_not_of("abc", 0, 1) == 3);  // Search for NOT "a", find 'b' at position 3
        CHECK(s.find_first_not_of("xyz", 0, 2) == 0);  // Search for NOT "xy", find 'a' at position 0
    }

    SUBCASE("find_first_not_of with fl::string") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";
        fl::string letters = "abcdefghijklmnopqrstuvwxyz";
        fl::string punct = ",.;:!?";

        CHECK(s.find_first_not_of(digits) == 3);  // First letter 'a' at position 3
        CHECK(s.find_first_not_of(letters) == 0);  // First digit '1' at position 0
        CHECK(s.find_first_not_of(punct) == 0);  // First char '1' not punctuation
    }

    SUBCASE("find_first_not_of with fl::string and position") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";

        CHECK(s.find_first_not_of(digits, 0) == 3);  // First non-digit from start
        CHECK(s.find_first_not_of(digits, 3) == 3);  // First non-digit from position 3
        CHECK(s.find_first_not_of(digits, 4) == 4);  // 'b' at position 4
        CHECK(s.find_first_not_of(digits, 6) == fl::string::npos);  // All digits from position 6
    }

    SUBCASE("find_first_not_of for parsing digits") {
        fl::string s = "123abc";
        CHECK(s.find_first_not_of("0123456789") == 3);  // First non-digit 'a'

        fl::string s2 = "999";
        CHECK(s2.find_first_not_of("0123456789") == fl::string::npos);  // All digits
    }

    SUBCASE("find_first_not_of for alphanumeric detection") {
        fl::string s = "hello_world";
        CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == 5);  // '_' at position 5

        fl::string s2 = "abc123";
        CHECK(s2.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    SUBCASE("find_first_not_of case sensitive") {
        fl::string s = "Hello World";
        CHECK(s.find_first_not_of("hello") == 0);  // 'H' not in lowercase set
        CHECK(s.find_first_not_of("HELLO") == 1);  // 'e' not in uppercase set
        CHECK(s.find_first_not_of("HELOelo") == 5);  // Space at position 5
    }

    SUBCASE("find_first_not_of with repeated characters in set") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_first_not_of("aaa") == 3);  // Duplicates don't matter, first non-'a'
        CHECK(s.find_first_not_of("ababab") == 6);  // First non-'a' or 'b' is 'c'
    }

    SUBCASE("find_first_not_of all characters match") {
        fl::string s = "aaaa";
        CHECK(s.find_first_not_of("a") == fl::string::npos);  // All are 'a'
        CHECK(s.find_first_not_of("a", 0) == fl::string::npos);
        CHECK(s.find_first_not_of("a", 2) == fl::string::npos);
    }

    SUBCASE("find_first_not_of no characters match") {
        fl::string s = "hello";
        CHECK(s.find_first_not_of("xyz") == 0);  // First char not in set
        CHECK(s.find_first_not_of("123") == 0);
        CHECK(s.find_first_not_of("XYZ") == 0);
    }

    SUBCASE("find_first_not_of at string boundaries") {
        fl::string s = "hello";
        CHECK(s.find_first_not_of("h") == 1);  // First non-'h' is 'e'
        CHECK(s.find_first_not_of("hel") == 4);  // First not 'h','e','l' is 'o' at position 4
        CHECK(s.find_first_not_of("helo") == fl::string::npos);  // All chars are in set
    }

    SUBCASE("find_first_not_of with special characters") {
        fl::string s = "///path/to/file";
        CHECK(s.find_first_not_of("/") == 3);  // First non-'/' is 'p' at position 3

        fl::string s2 = "...file.txt";
        CHECK(s2.find_first_not_of(".") == 3);  // First non-'.' is 'f' at position 3
    }

    SUBCASE("find_first_not_of for tokenization") {
        fl::string s = "   word1   word2";
        fl::size first_non_space = s.find_first_not_of(" ");
        CHECK(first_non_space == 3);  // 'w' at position 3

        fl::size next_space = s.find_first_of(" ", first_non_space);
        CHECK(next_space == 8);  // Space after "word1"

        fl::size next_word = s.find_first_not_of(" ", next_space);
        CHECK(next_word == 11);  // 'w' of "word2"
    }

    SUBCASE("find_first_not_of on inline string") {
        fl::string s = "   text";
        CHECK(s.find_first_not_of(" ") == 3);
        CHECK(s.find_first_not_of(" \t") == 3);
    }

    SUBCASE("find_first_not_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "y");  // Put a 'y' at position 10
        s.replace(50, 1, "z");  // Put a 'z' at position 50

        CHECK(s.find_first_not_of("x") == 10);  // First non-'x' is 'y' at position 10
        CHECK(s.find_first_not_of("x", 11) == 50);  // Next non-'x' is 'z' at position 50
        CHECK(s.find_first_not_of("xyz") == fl::string::npos);  // All are x, y, or z
    }

    SUBCASE("find_first_not_of from each position") {
        fl::string s = "aaabbb";
        CHECK(s.find_first_not_of("a", 0) == 3);  // First non-'a' from start
        CHECK(s.find_first_not_of("a", 1) == 3);  // Still position 3
        CHECK(s.find_first_not_of("a", 2) == 3);  // Still position 3
        CHECK(s.find_first_not_of("a", 3) == 3);  // 'b' at position 3
        CHECK(s.find_first_not_of("a", 4) == 4);  // 'b' at position 4
        CHECK(s.find_first_not_of("b", 3) == fl::string::npos);  // All 'b' from position 3
    }

    SUBCASE("find_first_not_of realistic use case - leading whitespace") {
        fl::string s1 = "   hello";
        CHECK(s1.find_first_not_of(" \t\n\r") == 3);

        fl::string s2 = "\t\n  hello";
        CHECK(s2.find_first_not_of(" \t\n\r") == 4);

        fl::string s3 = "hello";
        CHECK(s3.find_first_not_of(" \t\n\r") == 0);  // No leading whitespace

        fl::string s4 = "    ";
        CHECK(s4.find_first_not_of(" \t\n\r") == fl::string::npos);  // All whitespace
    }

    SUBCASE("find_first_not_of realistic use case - parsing numbers") {
        fl::string s = "0000123";
        CHECK(s.find_first_not_of("0") == 4);  // First non-zero digit at position 4

        fl::string s2 = "00000";
        CHECK(s2.find_first_not_of("0") == fl::string::npos);  // All zeros
    }

    SUBCASE("find_first_not_of realistic use case - validation") {
        fl::string s1 = "12345";
        CHECK(s1.find_first_not_of("0123456789") == fl::string::npos);  // All digits (valid)

        fl::string s2 = "123a5";
        CHECK(s2.find_first_not_of("0123456789") == 3);  // Invalid char 'a' at position 3
    }

    SUBCASE("find_first_not_of with entire alphabet") {
        fl::string s = "123abc";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        CHECK(s.find_first_not_of(alphabet) == 0);  // First non-letter '1' at position 0
        CHECK(s.find_first_not_of(alphabet, 3) == fl::string::npos);  // All letters from position 3
    }

    SUBCASE("find_first_not_of with position at string end") {
        fl::string s = "hello";
        CHECK(s.find_first_not_of("xyz", 5) == fl::string::npos);  // Position at end
        CHECK(s.find_first_not_of("xyz", 4) == 4);  // 'o' not in "xyz"
    }

    SUBCASE("find_first_not_of comparison with find_first_of") {
        fl::string s = "aaabbbccc";
        // find_first_of finds first char that IS in set
        // find_first_not_of finds first char that is NOT in set
        CHECK(s.find_first_of("bc") == 3);  // First 'b' at position 3
        CHECK(s.find_first_not_of("ab") == 6);  // First non-'a' or 'b' is 'c' at position 6
    }

    SUBCASE("find_first_not_of single character repeated") {
        fl::string s = "aaaaaaa";
        CHECK(s.find_first_not_of('a') == fl::string::npos);  // All 'a'
        CHECK(s.find_first_not_of('b') == 0);  // First char not 'b'
    }

    SUBCASE("find_first_not_of mixed alphanumeric") {
        fl::string s = "abc123def456";
        CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz") == 3);  // First digit '1'
        CHECK(s.find_first_not_of("0123456789") == 0);  // First letter 'a'
        CHECK(s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    SUBCASE("find_first_not_of for prefix detection") {
        fl::string s = "0x1A2B";  // Hex number with prefix
        CHECK(s.find_first_not_of("0") == 1);  // 'x' at position 1
        CHECK(s.find_first_not_of("0x", 0) == 2);  // First non-prefix char '1' at position 2
    }

    SUBCASE("find_first_not_of multiple character types") {
        fl::string s = "!!!hello";
        CHECK(s.find_first_not_of("!") == 3);  // First letter at position 3

        fl::string s2 = "$$$100";
        CHECK(s2.find_first_not_of("$") == 3);  // First digit at position 3
    }

    SUBCASE("find_first_not_of with zero count") {
        fl::string s = "hello";
        // Count 0 means empty set, so every character is "not in the set"
        CHECK(s.find_first_not_of("xyz", 0, 0) == 0);  // First char
        CHECK(s.find_first_not_of("xyz", 2, 0) == 2);  // From position 2
    }

    SUBCASE("find_first_not_of for comment detection") {
        fl::string s = "### This is a comment";
        // String positions: #(0) #(1) #(2) space(3) T(4)...
        CHECK(s.find_first_not_of("#") == 3);  // First non-'#' is space at position 3
        CHECK(s.find_first_not_of("# ") == 4);  // First non-'#' or space is 'T' at position 4
    }

    SUBCASE("find_first_not_of comprehensive trim test") {
        fl::string s1 = "   \t\n  hello world  \t\n   ";
        fl::size start = s1.find_first_not_of(" \t\n\r");
        // String: space(0) space(1) space(2) tab(3) newline(4) space(5) space(6) h(7)
        CHECK(start == 7);  // 'h' at position 7

        fl::string s2 = "hello";
        CHECK(s2.find_first_not_of(" \t\n\r") == 0);  // No trimming needed
    }

    SUBCASE("find_first_not_of versus operator==") {
        fl::string s = "aaa";
        // All characters are 'a', so first not 'a' is npos
        CHECK(s.find_first_not_of("a") == fl::string::npos);
        // Confirms all characters match the set

        fl::string s2 = "aab";
        CHECK(s2.find_first_not_of("a") == 2);  // 'b' at position 2
    }
}

TEST_CASE("String find_last_not_of operations") {
    SUBCASE("find_last_not_of single character") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of('c') == 5);  // Last 'b' at position 5
        CHECK(s.find_last_not_of('a') == 8);  // Last 'c' at position 8
        CHECK(s.find_last_not_of('x') == 8);  // Last char (no match with 'x')
    }

    SUBCASE("find_last_not_of with character set") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of("bc") == 2);  // Last 'a' at position 2
        CHECK(s.find_last_not_of("abc") == fl::string::npos);  // All chars are in set
        CHECK(s.find_last_not_of("xyz") == 8);  // Last char not in set
    }

    SUBCASE("find_last_not_of for trimming trailing whitespace") {
        fl::string s = "hello world   ";
        CHECK(s.find_last_not_of(" ") == 10);  // Last non-space 'd' at position 10
        CHECK(s.find_last_not_of(" \t\n\r") == 10);  // Last non-whitespace

        fl::string s2 = "text\t\n  ";
        CHECK(s2.find_last_not_of(" \t\n\r") == 3);  // Last non-whitespace 't' at position 3
    }

    SUBCASE("find_last_not_of with position limit") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of("c") == 5);  // From end, last non-'c'
        CHECK(s.find_last_not_of("c", 5) == 5);  // From position 5, last non-'c' is at 5
        CHECK(s.find_last_not_of("c", 4) == 4);  // From position 4, last non-'c' is at 4
        CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // From position 2, all are 'a'
    }

    SUBCASE("find_last_not_of with pos beyond string length") {
        fl::string s = "hello";
        CHECK(s.find_last_not_of("xyz", 100) == 4);  // Should search from end, find 'o'
        CHECK(s.find_last_not_of('x', 1000) == 4);  // Should find 'o' at position 4
    }

    SUBCASE("find_last_not_of with pos = npos") {
        fl::string s = "hello world";
        CHECK(s.find_last_not_of(" ", fl::string::npos) == 10);  // Search from end, last non-space 'd'
        CHECK(s.find_last_not_of('d', fl::string::npos) == 9);  // Last non-'d' is 'l'
    }

    SUBCASE("find_last_not_of in empty string") {
        fl::string s = "";
        CHECK(s.find_last_not_of("abc") == fl::string::npos);
        CHECK(s.find_last_not_of('x') == fl::string::npos);
        CHECK(s.find_last_not_of("") == fl::string::npos);
    }

    SUBCASE("find_last_not_of with empty set") {
        fl::string s = "hello";
        // Empty set means every character is "not in the set"
        CHECK(s.find_last_not_of("") == 4);  // Last char
        CHECK(s.find_last_not_of("", fl::string::npos, 0) == 4);  // Last char
        CHECK(s.find_last_not_of("", 2) == 2);  // From position 2
    }

    SUBCASE("find_last_not_of with null pointer") {
        fl::string s = "hello";
        // Null pointer means every character is "not in the set"
        CHECK(s.find_last_not_of(static_cast<const char*>(nullptr)) == 4);
        CHECK(s.find_last_not_of(static_cast<const char*>(nullptr), 2) == 2);
    }

    SUBCASE("find_last_not_of with counted string") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of("abc", fl::string::npos, 2) == 8);  // Search for NOT "ab", find 'c' at position 8
        CHECK(s.find_last_not_of("abc", fl::string::npos, 1) == 8);  // Search for NOT "a", find 'c' at position 8
        CHECK(s.find_last_not_of("xyz", fl::string::npos, 2) == 8);  // Search for NOT "xy", find last char
    }

    SUBCASE("find_last_not_of with fl::string") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";
        fl::string letters = "abcdefghijklmnopqrstuvwxyz";
        fl::string punct = ",.;:!?";

        CHECK(s.find_last_not_of(digits) == 5);  // Last letter 'c' at position 5
        CHECK(s.find_last_not_of(letters) == 8);  // Last digit '6' at position 8
        CHECK(s.find_last_not_of(punct) == 8);  // Last char not punctuation
    }

    SUBCASE("find_last_not_of with fl::string and position") {
        fl::string s = "123abc456";
        fl::string digits = "0123456789";

        CHECK(s.find_last_not_of(digits) == 5);  // Last non-digit from end
        CHECK(s.find_last_not_of(digits, 5) == 5);  // Last non-digit at or before position 5
        CHECK(s.find_last_not_of(digits, 4) == 4);  // 'b' at position 4
        CHECK(s.find_last_not_of(digits, 2) == fl::string::npos);  // All digits before and at position 2
    }

    SUBCASE("find_last_not_of for trailing zeros") {
        fl::string s = "1230000";
        CHECK(s.find_last_not_of("0") == 2);  // Last non-zero digit '3' at position 2

        fl::string s2 = "00000";
        CHECK(s2.find_last_not_of("0") == fl::string::npos);  // All zeros
    }

    SUBCASE("find_last_not_of for validation") {
        fl::string s1 = "12345";
        CHECK(s1.find_last_not_of("0123456789") == fl::string::npos);  // All digits (valid)

        fl::string s2 = "123a5";
        CHECK(s2.find_last_not_of("0123456789") == 3);  // Invalid char 'a' at position 3 is last non-digit
    }

    SUBCASE("find_last_not_of case sensitive") {
        fl::string s = "Hello World";
        // String: H(0) e(1) l(2) l(3) o(4) space(5) W(6) o(7) r(8) l(9) d(10)
        CHECK(s.find_last_not_of("world") == 6);  // 'W' not in lowercase set (case sensitive)
        CHECK(s.find_last_not_of("WORLD") == 10);  // 'd' not in uppercase set (case sensitive)
        CHECK(s.find_last_not_of("WORLDorld") == 5);  // Space at position 5
    }

    SUBCASE("find_last_not_of with repeated characters in set") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of("ccc") == 5);  // Duplicates don't matter, last non-'c'
        CHECK(s.find_last_not_of("bcbcbc") == 2);  // Last non-'b' or 'c' is 'a'
    }

    SUBCASE("find_last_not_of all characters match") {
        fl::string s = "aaaa";
        CHECK(s.find_last_not_of("a") == fl::string::npos);  // All are 'a'
        CHECK(s.find_last_not_of("a", 3) == fl::string::npos);
        CHECK(s.find_last_not_of("a", 1) == fl::string::npos);
    }

    SUBCASE("find_last_not_of no characters match") {
        fl::string s = "hello";
        CHECK(s.find_last_not_of("xyz") == 4);  // Last char not in set
        CHECK(s.find_last_not_of("123") == 4);
        CHECK(s.find_last_not_of("XYZ") == 4);
    }

    SUBCASE("find_last_not_of at string boundaries") {
        fl::string s = "hello";
        CHECK(s.find_last_not_of("o") == 3);  // Last non-'o' is 'l'
        CHECK(s.find_last_not_of("elo") == 0);  // Last not 'e','l','o' is 'h' at position 0
        CHECK(s.find_last_not_of("helo") == fl::string::npos);  // All chars are in set
    }

    SUBCASE("find_last_not_of with special characters") {
        fl::string s = "path/to/file///";
        CHECK(s.find_last_not_of("/") == 11);  // Last non-'/' is 'e' at position 11

        fl::string s2 = "file.txt...";
        CHECK(s2.find_last_not_of(".") == 7);  // Last non-'.' is 't' at position 7
    }

    SUBCASE("find_last_not_of for reverse tokenization") {
        fl::string s = "word1   word2   word3";
        fl::size last_non_space = s.find_last_not_of(" ");
        CHECK(last_non_space == 20);  // '3' at position 20

        fl::size prev_space = s.find_last_of(" ", last_non_space - 1);
        CHECK(prev_space == 15);  // Space before "word3"

        fl::size prev_word_end = s.find_last_not_of(" ", prev_space);
        CHECK(prev_word_end == 12);  // '2' at position 12
    }

    SUBCASE("find_last_not_of on inline string") {
        fl::string s = "text   ";
        CHECK(s.find_last_not_of(" ") == 3);
        CHECK(s.find_last_not_of(" \t") == 3);
    }

    SUBCASE("find_last_not_of on heap string") {
        // Create a string that uses heap allocation
        fl::string s(FASTLED_STR_INLINED_SIZE + 10, 'x');
        s.replace(10, 1, "y");  // Put a 'y' at position 10
        s.replace(50, 1, "z");  // Put a 'z' at position 50

        CHECK(s.find_last_not_of("x") == 50);  // Last non-'x' is 'z' at position 50
        CHECK(s.find_last_not_of("x", 49) == 10);  // Previous non-'x' is 'y' at position 10
        CHECK(s.find_last_not_of("xyz") == fl::string::npos);  // All are x, y, or z
    }

    SUBCASE("find_last_not_of from each position") {
        fl::string s = "aaabbb";
        CHECK(s.find_last_not_of("b", 5) == 2);  // Last non-'b' from position 5 is 'a' at 2
        CHECK(s.find_last_not_of("b", 4) == 2);  // Still position 2
        CHECK(s.find_last_not_of("b", 3) == 2);  // Still position 2
        CHECK(s.find_last_not_of("b", 2) == 2);  // 'a' at position 2
        CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // All 'a' from position 2
    }

    SUBCASE("find_last_not_of realistic use case - trailing whitespace") {
        fl::string s1 = "hello   ";
        CHECK(s1.find_last_not_of(" \t\n\r") == 4);

        fl::string s2 = "hello\t\n  ";
        CHECK(s2.find_last_not_of(" \t\n\r") == 4);

        fl::string s3 = "hello";
        CHECK(s3.find_last_not_of(" \t\n\r") == 4);  // No trailing whitespace

        fl::string s4 = "    ";
        CHECK(s4.find_last_not_of(" \t\n\r") == fl::string::npos);  // All whitespace
    }

    SUBCASE("find_last_not_of realistic use case - trailing zeros") {
        fl::string s = "1230000";
        CHECK(s.find_last_not_of("0") == 2);  // Last non-zero digit at position 2

        fl::string s2 = "00000";
        CHECK(s2.find_last_not_of("0") == fl::string::npos);  // All zeros
    }

    SUBCASE("find_last_not_of realistic use case - file extension") {
        fl::string s = "file.txt   ";
        fl::size end = s.find_last_not_of(" ");
        CHECK(end == 7);  // Last non-space 't' at position 7
    }

    SUBCASE("find_last_not_of with entire alphabet") {
        fl::string s = "abc123";
        fl::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        CHECK(s.find_last_not_of(alphabet) == 5);  // Last non-letter '3' at position 5
        CHECK(s.find_last_not_of(alphabet, 2) == fl::string::npos);  // All letters up to position 2
    }

    SUBCASE("find_last_not_of with position at string end") {
        fl::string s = "hello";
        CHECK(s.find_last_not_of("xyz", 4) == 4);  // 'o' not in "xyz"
        CHECK(s.find_last_not_of("o", 4) == 3);  // 'l' at position 3
    }

    SUBCASE("find_last_not_of comparison with find_first_not_of") {
        fl::string s = "aaabbbccc";
        // find_first_not_of finds first char that is NOT in set
        // find_last_not_of finds last char that is NOT in set
        CHECK(s.find_first_not_of("a") == 3);  // First non-'a' at position 3
        CHECK(s.find_last_not_of("c") == 5);  // Last non-'c' at position 5
    }

    SUBCASE("find_last_not_of single character repeated") {
        fl::string s = "aaaaaaa";
        CHECK(s.find_last_not_of('a') == fl::string::npos);  // All 'a'
        CHECK(s.find_last_not_of('b') == 6);  // Last char not 'b'
    }

    SUBCASE("find_last_not_of mixed alphanumeric") {
        fl::string s = "abc123def456";
        CHECK(s.find_last_not_of("0123456789") == 8);  // Last letter 'f' at position 8
        CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz") == 11);  // Last digit '6' at position 11
        CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz0123456789") == fl::string::npos);  // All alphanumeric
    }

    SUBCASE("find_last_not_of for suffix detection") {
        fl::string s = "hello!!!";
        CHECK(s.find_last_not_of("!") == 4);  // Last letter 'o' at position 4

        fl::string s2 = "value$$$";
        CHECK(s2.find_last_not_of("$") == 4);  // Last letter 'e' at position 4
    }

    SUBCASE("find_last_not_of multiple character types") {
        fl::string s = "hello!!!";
        CHECK(s.find_last_not_of("!") == 4);  // Last non-'!' at position 4

        fl::string s2 = "100$$$";
        CHECK(s2.find_last_not_of("$") == 2);  // Last digit '0' at position 2
    }

    SUBCASE("find_last_not_of with zero count") {
        fl::string s = "hello";
        // Count 0 means empty set, so every character is "not in the set"
        CHECK(s.find_last_not_of("xyz", fl::string::npos, 0) == 4);  // Last char
        CHECK(s.find_last_not_of("xyz", 2, 0) == 2);  // From position 2
    }

    SUBCASE("find_last_not_of for comment trailing spaces") {
        fl::string s = "This is a comment   ";
        CHECK(s.find_last_not_of(" ") == 16);  // Last non-space 't' at position 16
        CHECK(s.find_last_not_of(" \t") == 16);  // Last non-whitespace
    }

    SUBCASE("find_last_not_of comprehensive trim test") {
        fl::string s1 = "   \t\n  hello world  \t\n   ";
        fl::size end = s1.find_last_not_of(" \t\n\r");
        CHECK(end == 17);  // 'd' at position 17

        fl::string s2 = "hello";
        CHECK(s2.find_last_not_of(" \t\n\r") == 4);  // No trimming needed
    }

    SUBCASE("find_last_not_of versus operator==") {
        fl::string s = "aaa";
        // All characters are 'a', so last not 'a' is npos
        CHECK(s.find_last_not_of("a") == fl::string::npos);
        // Confirms all characters match the set

        fl::string s2 = "baa";
        CHECK(s2.find_last_not_of("a") == 0);  // 'b' at position 0
    }

    SUBCASE("find_last_not_of at position 0") {
        fl::string s = "hello world";
        CHECK(s.find_last_not_of('h', 0) == fl::string::npos);  // Can't find non-'h' at position 0
        CHECK(s.find_last_not_of("world", 0) == 0);  // 'h' not in "world"
        CHECK(s.find_last_not_of('e', 0) == 0);  // 'h' at position 0 is not 'e'
    }

    SUBCASE("find_last_not_of with overlapping character sets") {
        fl::string s = "hello123world456";
        CHECK(s.find_last_not_of("0123456789") == 12);  // Last letter 'd'
        CHECK(s.find_last_not_of("abcdefghijklmnopqrstuvwxyz") == 15);  // Last digit '6'
        CHECK(s.find_last_not_of("0123456789abcdefghijklmnopqrstuvwxyz") == fl::string::npos);  // All alphanumeric
    }

    SUBCASE("find_last_not_of for line ending detection") {
        fl::string s = "line of text\n\r\n";
        CHECK(s.find_last_not_of("\n\r") == 11);  // Last non-line-ending 't' at position 11
    }

    SUBCASE("find_last_not_of path trailing separators") {
        fl::string s = "path/to/dir///";
        CHECK(s.find_last_not_of("/") == 10);  // Last non-'/' is 'r' at position 10
    }

    SUBCASE("find_last_not_of comparison with rfind") {
        fl::string s = "hello world";
        // For strings without the target character, behavior differs:
        // rfind('x') returns npos (not found)
        // find_last_not_of('x') returns last position (all chars are not 'x')
        CHECK(s.rfind('x') == fl::string::npos);  // 'x' not found
        CHECK(s.find_last_not_of('x') == 10);  // Last char not 'x' is 'd' at position 10
    }

    SUBCASE("find_last_not_of with position exactly at boundary") {
        fl::string s = "aaabbbccc";
        CHECK(s.find_last_not_of("c", 5) == 5);  // Position 5 is 'b', which is not 'c'
        CHECK(s.find_last_not_of("b", 5) == 2);  // From position 5, last non-'b' is 'a' at 2
        CHECK(s.find_last_not_of("a", 2) == fl::string::npos);  // Positions 0-2 are all 'a'
    }

    SUBCASE("find_last_not_of for data validation - trailing invalid chars") {
        fl::string s = "12345xyz";
        CHECK(s.find_last_not_of("0123456789") == 7);  // Last non-digit 'z' at position 7

        fl::string s2 = "12345";
        CHECK(s2.find_last_not_of("0123456789") == fl::string::npos);  // All digits (valid)
    }

    SUBCASE("find_last_not_of empty string with various sets") {
        fl::string s = "";
        CHECK(s.find_last_not_of("abc") == fl::string::npos);
        CHECK(s.find_last_not_of("") == fl::string::npos);
        CHECK(s.find_last_not_of("xyz", 0) == fl::string::npos);
        CHECK(s.find_last_not_of('a') == fl::string::npos);
    }

    SUBCASE("find_last_not_of single character string") {
        fl::string s = "x";
        CHECK(s.find_last_not_of('x') == fl::string::npos);  // Only char is 'x'
        CHECK(s.find_last_not_of('y') == 0);  // Only char 'x' is not 'y'
        CHECK(s.find_last_not_of("xy") == fl::string::npos);  // 'x' is in set
        CHECK(s.find_last_not_of("yz") == 0);  // 'x' not in set
    }

    SUBCASE("find_last_not_of realistic trim implementation") {
        fl::string s = "   hello world   ";
        fl::size start = s.find_first_not_of(" \t\n\r");
        fl::size end = s.find_last_not_of(" \t\n\r");

        CHECK(start == 3);  // 'h' at position 3
        CHECK(end == 13);  // 'd' at position 13

        if (start != fl::string::npos && end != fl::string::npos) {
            fl::string trimmed = s.substr(start, end - start + 1);
            CHECK(trimmed == "hello world");
        }
    }

    // at() tests - bounds-checked element access (std::string compatibility)
    SUBCASE("at() basic access") {
        fl::string s = "Hello";
        CHECK(s.at(0) == 'H');
        CHECK(s.at(1) == 'e');
        CHECK(s.at(2) == 'l');
        CHECK(s.at(3) == 'l');
        CHECK(s.at(4) == 'o');
    }

    SUBCASE("at() const access") {
        const fl::string s = "World";
        CHECK(s.at(0) == 'W');
        CHECK(s.at(1) == 'o');
        CHECK(s.at(2) == 'r');
        CHECK(s.at(3) == 'l');
        CHECK(s.at(4) == 'd');
    }

    SUBCASE("at() modification") {
        fl::string s = "Hello";
        s.at(0) = 'h';
        s.at(4) = '!';
        CHECK(s == "hell!");
    }

    SUBCASE("at() out of bounds") {
        fl::string s = "test";
        // Out of bounds access returns dummy '\0'
        // (unlike std::string which would throw exception)
        CHECK(s.at(4) == '\0');  // pos == length
        CHECK(s.at(5) == '\0');  // pos > length
        CHECK(s.at(100) == '\0');  // far out of bounds
    }

    SUBCASE("at() const out of bounds") {
        const fl::string s = "test";
        CHECK(s.at(4) == '\0');
        CHECK(s.at(5) == '\0');
        CHECK(s.at(100) == '\0');
    }

    SUBCASE("at() empty string") {
        fl::string s;
        CHECK(s.at(0) == '\0');
        CHECK(s.at(1) == '\0');
    }

    SUBCASE("at() single character") {
        fl::string s = "A";
        CHECK(s.at(0) == 'A');
        CHECK(s.at(1) == '\0');  // out of bounds
    }

    SUBCASE("at() first and last") {
        fl::string s = "ABCDEF";
        CHECK(s.at(0) == 'A');  // first
        CHECK(s.at(5) == 'F');  // last
        CHECK(s.at(6) == '\0');  // past end
    }

    SUBCASE("at() vs operator[]") {
        fl::string s = "compare";
        // Both should behave the same for fl::string
        for (fl::size i = 0; i < s.size(); ++i) {
            CHECK(s.at(i) == s[i]);
        }
        // Out of bounds should also match
        CHECK(s.at(s.size()) == s[s.size()]);
    }

    SUBCASE("at() modification at boundaries") {
        fl::string s = "test";
        s.at(0) = 'T';  // first
        s.at(3) = 'T';  // last
        CHECK(s == "TesT");
    }

    SUBCASE("at() with inline string") {
        fl::string s = "short";  // inline buffer
        CHECK(s.at(0) == 's');
        CHECK(s.at(4) == 't');
        s.at(2) = 'x';
        CHECK(s == "shxrt");
    }

    SUBCASE("at() with heap string") {
        // Create a string that will use heap storage
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.push_back('A' + (i % 26));
        }
        CHECK(s.at(0) == 'A');
        CHECK(s.at(50) == 'A' + (50 % 26));
        CHECK(s.at(99) == 'A' + (99 % 26));
        s.at(50) = 'X';
        CHECK(s.at(50) == 'X');
    }

    SUBCASE("at() sequential access") {
        fl::string s = "0123456789";
        for (fl::size i = 0; i < 10; ++i) {
            CHECK(s.at(i) == '0' + i);
        }
    }

    SUBCASE("at() modify all characters") {
        fl::string s = "aaaaa";
        for (fl::size i = 0; i < s.size(); ++i) {
            s.at(i) = 'a' + i;
        }
        CHECK(s == "abcde");
    }

    SUBCASE("at() with special characters") {
        fl::string s = "!@#$%";
        CHECK(s.at(0) == '!');
        CHECK(s.at(1) == '@');
        CHECK(s.at(2) == '#');
        CHECK(s.at(3) == '$');
        CHECK(s.at(4) == '%');
    }

    SUBCASE("at() with numbers") {
        fl::string s = "0123456789";
        for (fl::size i = 0; i < 10; ++i) {
            CHECK(s.at(i) == ('0' + i));
        }
    }

    SUBCASE("at() case sensitivity") {
        fl::string s = "AaBbCc";
        CHECK(s.at(0) == 'A');
        CHECK(s.at(1) == 'a');
        CHECK(s.at(2) == 'B');
        CHECK(s.at(3) == 'b');
        CHECK(s.at(4) == 'C');
        CHECK(s.at(5) == 'c');
    }

    SUBCASE("at() with spaces") {
        fl::string s = "a b c";
        CHECK(s.at(0) == 'a');
        CHECK(s.at(1) == ' ');
        CHECK(s.at(2) == 'b');
        CHECK(s.at(3) == ' ');
        CHECK(s.at(4) == 'c');
    }

    SUBCASE("at() with newlines and tabs") {
        fl::string s = "a\nb\tc";
        CHECK(s.at(0) == 'a');
        CHECK(s.at(1) == '\n');
        CHECK(s.at(2) == 'b');
        CHECK(s.at(3) == '\t');
        CHECK(s.at(4) == 'c');
    }

    SUBCASE("at() after clear") {
        fl::string s = "test";
        s.clear();
        CHECK(s.at(0) == '\0');
    }

    SUBCASE("at() after erase") {
        fl::string s = "testing";
        s.erase(3, 4);  // "tes"
        CHECK(s.at(0) == 't');
        CHECK(s.at(1) == 'e');
        CHECK(s.at(2) == 's');
        CHECK(s.at(3) == '\0');  // now out of bounds
    }

    SUBCASE("at() after insert") {
        fl::string s = "test";
        s.insert(2, "XX");  // "teXXst"
        CHECK(s.at(0) == 't');
        CHECK(s.at(1) == 'e');
        CHECK(s.at(2) == 'X');
        CHECK(s.at(3) == 'X');
        CHECK(s.at(4) == 's');
        CHECK(s.at(5) == 't');
    }

    SUBCASE("at() after replace") {
        fl::string s = "Hello";
        s.replace(1, 3, "i");  // "Hio"
        CHECK(s.at(0) == 'H');
        CHECK(s.at(1) == 'i');
        CHECK(s.at(2) == 'o');
        CHECK(s.at(3) == '\0');
    }

    SUBCASE("at() with repeated characters") {
        fl::string s = "aaaaaaaaaa";
        for (fl::size i = 0; i < s.size(); ++i) {
            CHECK(s.at(i) == 'a');
        }
    }

    SUBCASE("at() boundary at length - 1") {
        fl::string s = "test";
        CHECK(s.at(s.size() - 1) == 't');  // last valid character
        CHECK(s.at(s.size()) == '\0');  // first invalid position
    }

    SUBCASE("at() return reference test") {
        fl::string s = "test";
        char& ref = s.at(0);
        ref = 'T';
        CHECK(s == "Test");
        CHECK(s.at(0) == 'T');
    }

    SUBCASE("at() const reference test") {
        const fl::string s = "test";
        const char& ref = s.at(0);
        CHECK(ref == 't');
        CHECK(&ref == &s.at(0));  // same memory location
    }

    SUBCASE("at() with zero position") {
        fl::string s = "test";
        CHECK(s.at(0) == 't');
        s.at(0) = 'T';
        CHECK(s.at(0) == 'T');
    }

    SUBCASE("at() comparison with front/back") {
        fl::string s = "test";
        CHECK(s.at(0) == s.front());
        CHECK(s.at(s.size() - 1) == s.back());
    }

    SUBCASE("at() with substring result") {
        fl::string s = "Hello World";
        fl::string sub = s.substr(6, 5);  // "World"
        CHECK(sub.at(0) == 'W');
        CHECK(sub.at(4) == 'd');
    }

    SUBCASE("at() access pattern") {
        fl::string s = "pattern";
        // Access in different order
        CHECK(s.at(3) == 't');
        CHECK(s.at(0) == 'p');
        CHECK(s.at(6) == 'n');
        CHECK(s.at(2) == 't');
        CHECK(s.at(5) == 'r');
    }

    SUBCASE("at() large index out of bounds") {
        fl::string s = "small";
        CHECK(s.at(1000) == '\0');
        CHECK(s.at(fl::size(-1) / 2) == '\0');  // very large index
    }
}

// Test reverse iterators (std::string compatibility)
TEST_CASE("StrN reverse iterators") {
    SUBCASE("rbegin/rend on non-empty string") {
        fl::string s = "Hello";
        // rbegin() should point to last character
        CHECK(s.rbegin() != nullptr);
        CHECK(*s.rbegin() == 'o');

        // Manually iterate backwards
        char* it = s.rbegin();
        CHECK(*it == 'o'); it--;
        CHECK(*it == 'l'); it--;
        CHECK(*it == 'l'); it--;
        CHECK(*it == 'e'); it--;
        CHECK(*it == 'H');
        // rend() is one-before-first
        CHECK(it == s.rend() + 1);
    }

    SUBCASE("rbegin/rend on empty string") {
        fl::string s = "";
        CHECK(s.rbegin() == nullptr);
        CHECK(s.rend() == nullptr);
    }

    SUBCASE("const rbegin/rend") {
        const fl::string s = "World";
        CHECK(s.rbegin() != nullptr);
        CHECK(*s.rbegin() == 'd');

        const char* it = s.rbegin();
        CHECK(*it == 'd'); it--;
        CHECK(*it == 'l'); it--;
        CHECK(*it == 'r'); it--;
        CHECK(*it == 'o'); it--;
        CHECK(*it == 'W');
        CHECK(it == s.rend() + 1);
    }

    SUBCASE("crbegin/crend") {
        fl::string s = "Test";
        // crbegin/crend should return const iterators
        const char* crit = s.crbegin();
        CHECK(crit != nullptr);
        CHECK(*crit == 't');

        crit--;
        CHECK(*crit == 's'); crit--;
        CHECK(*crit == 'e'); crit--;
        CHECK(*crit == 'T');
        CHECK(crit == s.crend() + 1);
    }

    SUBCASE("reverse iteration with single character") {
        fl::string s = "X";
        CHECK(s.rbegin() != nullptr);
        CHECK(*s.rbegin() == 'X');
        CHECK(s.rbegin() == s.rend() + 1);  // One element: rbegin is one past rend
    }

    SUBCASE("reverse iteration builds reversed string") {
        fl::string s = "ABC";
        fl::string reversed;

        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "CBA");
    }

    SUBCASE("const reverse iteration") {
        const fl::string s = "12345";
        fl::string result;

        for (const char* it = s.rbegin(); it != s.rend(); --it) {
            result.push_back(*it);
        }
        CHECK(result == "54321");
    }

    SUBCASE("modification through reverse iterator") {
        fl::string s = "abcd";
        char* it = s.rbegin();
        *it = 'D';  // Change 'd' to 'D'
        CHECK(s == "abcD");

        it--;
        *it = 'C';  // Change 'c' to 'C'
        CHECK(s == "abCD");
    }

    SUBCASE("reverse iterator with inline string") {
        fl::string s = "Short";  // Fits in inline buffer
        CHECK(s.rbegin() != nullptr);
        CHECK(*s.rbegin() == 't');

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "trohS");
    }

    SUBCASE("reverse iterator with heap string") {
        // Create a string large enough to require heap allocation
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.push_back('A' + (i % 26));
        }

        CHECK(s.rbegin() != nullptr);
        CHECK(*s.rbegin() == 'V');  // 99 % 26 = 21, 'A' + 21 = 'V'

        // Verify first few characters in reverse
        char* it = s.rbegin();
        CHECK(*it == 'V'); it--;  // i=99: 99%26=21
        CHECK(*it == 'U'); it--;  // i=98: 98%26=20
        CHECK(*it == 'T');        // i=97: 97%26=19
    }

    SUBCASE("reverse iterator after modification") {
        fl::string s = "test";
        s.insert(2, "XX");  // "teXXst"

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "tsXXet");
    }

    SUBCASE("reverse iterator matches forward") {
        fl::string s = "abcdef";

        // Forward iteration
        fl::string forward;
        for (char* it = s.begin(); it != s.end(); ++it) {
            forward.push_back(*it);
        }

        // Reverse iteration
        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }

        CHECK(forward == "abcdef");
        CHECK(reversed == "fedcba");
    }

    SUBCASE("reverse iterator with special characters") {
        fl::string s = "!@#$%";
        CHECK(*s.rbegin() == '%');

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "%$#@!");
    }

    SUBCASE("reverse iterator with digits") {
        fl::string s = "0123456789";
        CHECK(*s.rbegin() == '9');

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "9876543210");
    }

    SUBCASE("reverse iterator with whitespace") {
        fl::string s = "a b c";
        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "c b a");
    }

    SUBCASE("reverse iterator pointer arithmetic") {
        fl::string s = "12345";
        char* last = s.rbegin();
        char* first_minus_one = s.rend();

        // rbegin is at last element, rend is one-before-first
        // Distance should be length
        CHECK(last - first_minus_one == static_cast<long>(s.size()));
    }

    SUBCASE("const correctness of reverse iterators") {
        fl::string s = "test";
        const fl::string& cs = s;

        // Non-const version returns char*
        char* it = s.rbegin();
        CHECK(it != nullptr);

        // Const version returns const char*
        const char* cit = cs.rbegin();
        CHECK(cit != nullptr);

        // crbegin always returns const
        const char* ccit = s.crbegin();
        CHECK(ccit != nullptr);
    }

    SUBCASE("reverse iterator bounds checking") {
        fl::string s = "ABC";
        char* it = s.rbegin();

        // Should be able to access all characters
        CHECK(*it == 'C'); it--;
        CHECK(*it == 'B'); it--;
        CHECK(*it == 'A');

        // Now at rend + 1, one more decrement gives rend (invalid)
        it--;
        CHECK(it == s.rend());
    }

    SUBCASE("reverse iterator with copy-on-write") {
        fl::string s1 = "shared";
        fl::string s2 = s1;  // COW: shares data

        // Read through reverse iterator (no copy)
        CHECK(*s1.rbegin() == 'd');
        CHECK(*s2.rbegin() == 'd');

        // Modify through reverse iterator (triggers copy)
        *s1.rbegin() = 'D';
        CHECK(s1 == "shareD");
        CHECK(s2 == "shared");  // s2 unchanged
    }

    SUBCASE("reverse iterator comparison with at()") {
        fl::string s = "test";
        CHECK(*s.rbegin() == s.at(s.size() - 1));
        CHECK(*(s.rbegin() - 1) == s.at(s.size() - 2));
        CHECK(*(s.rbegin() - 2) == s.at(s.size() - 3));
    }

    SUBCASE("reverse iterator with substr") {
        fl::string s = "Hello World";
        fl::string sub = s.substr(6, 5);  // "World"

        fl::string reversed;
        for (char* it = sub.rbegin(); it != sub.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "dlroW");
    }

    SUBCASE("reverse iterator empty after clear") {
        fl::string s = "test";
        s.clear();
        CHECK(s.rbegin() == nullptr);
        CHECK(s.rend() == nullptr);
    }

    SUBCASE("reverse iterator with repeated characters") {
        fl::string s = "aaaaaa";
        int count = 0;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            CHECK(*it == 'a');
            count++;
        }
        CHECK(count == 6);
    }

    SUBCASE("reverse iterator comparison with back()") {
        fl::string s = "example";
        CHECK(*s.rbegin() == s.back());
        CHECK(s.rbegin() == s.begin() + s.size() - 1);
    }

    SUBCASE("reverse iterator manual loop count") {
        fl::string s = "count";
        int iterations = 0;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            iterations++;
        }
        CHECK(iterations == static_cast<int>(s.size()));
    }

    SUBCASE("reverse iterator with newlines") {
        fl::string s = "a\nb\nc";
        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "c\nb\na");
    }

    SUBCASE("reverse iterator palindrome check") {
        fl::string s = "racecar";

        // Check if palindrome using reverse iteration
        char* fwd = s.begin();
        char* rev = s.rbegin();
        bool is_palindrome = true;

        while (fwd < s.end() && rev != s.rend()) {
            if (*fwd != *rev) {
                is_palindrome = false;
                break;
            }
            fwd++;
            rev--;
        }
        CHECK(is_palindrome == true);
    }

    SUBCASE("reverse iterator not palindrome") {
        fl::string s = "hello";

        char* fwd = s.begin();
        char* rev = s.rbegin();
        bool is_palindrome = true;

        while (fwd < s.end() && rev != s.rend()) {
            if (*fwd != *rev) {
                is_palindrome = false;
                break;
            }
            fwd++;
            rev--;
        }
        CHECK(is_palindrome == false);
    }

    SUBCASE("reverse iterator null terminator not included") {
        fl::string s = "test";
        // Reverse iterators should not include null terminator
        int count = 0;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            count++;
        }
        CHECK(count == 4);  // Only actual characters, not '\0'
    }

    SUBCASE("reverse iterator after erase") {
        fl::string s = "testing";
        s.erase(3, 3);  // Remove "tin" -> "tesg"

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "gset");
    }

    SUBCASE("reverse iterator after replace") {
        fl::string s = "test";
        s.replace(1, 2, "XX");  // "tXXt"

        fl::string reversed;
        for (char* it = s.rbegin(); it != s.rend(); --it) {
            reversed.push_back(*it);
        }
        CHECK(reversed == "tXXt");  // Palindrome!
    }
}

TEST_CASE("String compare operations") {
    // compare() returns <0 if this<other, 0 if equal, >0 if this>other
    // Like strcmp, provides three-way comparison for lexicographical ordering

    SUBCASE("compare with equal strings") {
        string s1 = "hello";
        string s2 = "hello";
        CHECK(s1.compare(s2) == 0);
        CHECK(s2.compare(s1) == 0);
    }

    SUBCASE("compare with different strings") {
        string s1 = "abc";
        string s2 = "def";
        CHECK(s1.compare(s2) < 0);  // "abc" < "def"
        CHECK(s2.compare(s1) > 0);  // "def" > "abc"
    }

    SUBCASE("compare empty strings") {
        string s1 = "";
        string s2 = "";
        CHECK(s1.compare(s2) == 0);

        string s3 = "hello";
        CHECK(s1.compare(s3) < 0);  // Empty < non-empty
        CHECK(s3.compare(s1) > 0);  // Non-empty > empty
    }

    SUBCASE("compare with C-string") {
        string s = "hello";
        CHECK(s.compare("hello") == 0);
        CHECK(s.compare("world") < 0);  // "hello" < "world"
        CHECK(s.compare("abc") > 0);    // "hello" > "abc"
    }

    SUBCASE("compare with null C-string") {
        string s = "hello";
        CHECK(s.compare(nullptr) > 0);  // Non-empty > null

        string empty = "";
        CHECK(empty.compare(nullptr) == 0);  // Empty == null
    }

    SUBCASE("compare prefix strings") {
        string s1 = "hello";
        string s2 = "hello world";
        CHECK(s1.compare(s2) < 0);  // Shorter prefix < longer
        CHECK(s2.compare(s1) > 0);  // Longer > shorter prefix
    }

    SUBCASE("compare case sensitivity") {
        string s1 = "Hello";
        string s2 = "hello";
        CHECK(s1.compare(s2) < 0);  // 'H' (72) < 'h' (104)
        CHECK(s2.compare(s1) > 0);
    }

    SUBCASE("compare substring with another string") {
        string s1 = "hello world";
        string s2 = "world";
        // Compare substring [6, 11) with "world"
        CHECK(s1.compare(6, 5, s2) == 0);

        // Compare substring [0, 5) with "world"
        CHECK(s1.compare(0, 5, s2) < 0);  // "hello" < "world"
    }

    SUBCASE("compare substring with npos count") {
        string s = "hello world";
        string s2 = "world";
        // npos means "until end of string"
        CHECK(s.compare(6, string::npos, s2) == 0);
    }

    SUBCASE("compare substring exceeding length") {
        string s = "hello";
        string s2 = "hello world";
        // Compare all of s with s2 (count is limited to available chars)
        CHECK(s.compare(0, 100, s2) < 0);  // "hello" < "hello world"
    }

    SUBCASE("compare substring with C-string") {
        string s = "hello world";
        CHECK(s.compare(0, 5, "hello") == 0);
        CHECK(s.compare(6, 5, "world") == 0);
        CHECK(s.compare(0, 5, "world") < 0);  // "hello" < "world"
    }

    SUBCASE("compare substring with substring") {
        string s1 = "prefix_data_suffix";
        string s2 = "other_data_end";
        // Compare "data" from s1 with "data" from s2
        CHECK(s1.compare(7, 4, s2, 6, 4) == 0);

        // Compare "prefix" from s1 with "other" from s2
        CHECK(s1.compare(0, 6, s2, 0, 5) > 0);  // "prefix" > "other"
    }

    SUBCASE("compare substring with npos in second string") {
        string s1 = "hello_world";
        string s2 = "world_is_beautiful";
        // Compare "world" from s1 with "world_is_beautiful" from s2
        CHECK(s1.compare(6, 5, s2, 0, string::npos) < 0);  // "world" < "world_is_beautiful"
    }

    SUBCASE("compare out of bounds position") {
        string s1 = "hello";
        string s2 = "world";
        // Out of bounds position returns comparison with empty string
        CHECK(s1.compare(100, 5, s2) < 0);  // "" < "world"
        CHECK(s2.compare(100, 5, "") == 0);  // "" == ""
    }

    SUBCASE("compare with count2 for C-string") {
        string s = "hello";
        // Compare with first 3 chars of "hello world"
        CHECK(s.compare(0, 3, "hello world", 3) == 0);  // "hel" == "hel"

        // Compare with first 5 chars
        CHECK(s.compare(0, 5, "hello world", 5) == 0);  // "hello" == "hello"

        // Compare with first 11 chars
        CHECK(s.compare(0, 5, "hello world", 11) < 0);  // "hello" < "hello world"
    }

    SUBCASE("compare substring length mismatch") {
        string s1 = "testing";
        string s2 = "test";
        // When actual compared portions are equal but lengths differ, shorter is "less"
        CHECK(s1.compare(0, 4, s2, 0, 4) == 0);  // "test" == "test"
        CHECK(s1.compare(0, 7, s2, 0, 4) > 0);   // "testing" > "test"
    }

    SUBCASE("compare with zero count") {
        string s1 = "hello";
        string s2 = "world";
        // Zero count means comparing empty strings
        CHECK(s1.compare(0, 0, s2, 0, 0) == 0);  // "" == ""
        CHECK(s1.compare(2, 0, s2, 3, 0) == 0);  // "" == ""
    }

    SUBCASE("compare for sorting") {
        string s1 = "apple";
        string s2 = "banana";
        string s3 = "cherry";

        CHECK(s1.compare(s2) < 0);
        CHECK(s2.compare(s3) < 0);
        CHECK(s1.compare(s3) < 0);

        // Verify transitivity
        CHECK((s1.compare(s2) < 0 && s2.compare(s3) < 0) == (s1.compare(s3) < 0));
    }

    SUBCASE("compare with special characters") {
        string s1 = "hello!";
        string s2 = "hello?";
        CHECK(s1.compare(s2) < 0);  // '!' (33) < '?' (63)

        string s3 = "hello\n";
        string s4 = "hello\t";
        CHECK(s3.compare(s4) > 0);  // '\n' (10) > '\t' (9), so s3 > s4
    }

    SUBCASE("compare numbers as strings") {
        string s1 = "10";
        string s2 = "9";
        // Lexicographical: '1' < '9', so "10" < "9"
        CHECK(s1.compare(s2) < 0);

        string s3 = "100";
        string s4 = "99";
        CHECK(s3.compare(s4) < 0);  // '1' < '9'
    }

    SUBCASE("compare position at string boundary") {
        string s = "hello";
        // Position at length() is valid (points to empty substring)
        CHECK(s.compare(5, 0, "") == 0);
        CHECK(s.compare(5, 0, "x") < 0);  // "" < "x"
    }

    SUBCASE("compare entire string via substring") {
        string s1 = "hello world";
        string s2 = "hello world";
        // These should be equivalent
        CHECK(s1.compare(s2) == s1.compare(0, string::npos, s2));
        CHECK(s1.compare(s2) == s1.compare(0, s1.length(), s2, 0, s2.length()));
    }

    SUBCASE("compare after string modifications") {
        string s1 = "hello";
        string s2 = "hello";
        CHECK(s1.compare(s2) == 0);

        s1.append(" world");
        CHECK(s1.compare(s2) > 0);  // "hello world" > "hello"

        s1.clear();
        CHECK(s1.compare(s2) < 0);  // "" < "hello"
    }

    SUBCASE("compare consistency with equality operators") {
        string s1 = "test";
        string s2 = "test";
        string s3 = "other";

        // compare() == 0 should match operator==
        CHECK((s1.compare(s2) == 0) == (s1 == s2));
        CHECK((s1.compare(s3) == 0) == (s1 == s3));

        // compare() != 0 should match operator!=
        CHECK((s1.compare(s3) != 0) == (s1 != s3));
    }

    SUBCASE("compare with repeated characters") {
        string s1 = "aaaa";
        string s2 = "aaab";
        CHECK(s1.compare(s2) < 0);  // Last char: 'a' < 'b'

        string s3 = "aaa";
        CHECK(s1.compare(s3) > 0);  // "aaaa" > "aaa"
    }

    SUBCASE("compare middle substrings") {
        string s = "the quick brown fox jumps";
        CHECK(s.compare(4, 5, "quick") == 0);
        CHECK(s.compare(10, 5, "brown") == 0);
        CHECK(s.compare(20, 5, "jumps") == 0);
    }

    SUBCASE("compare overlapping substrings of same string") {
        string s = "abcdefgh";
        // Compare "abc" with "def"
        CHECK(s.compare(0, 3, s, 3, 3) < 0);  // "abc" < "def"

        // Compare "def" with "abc"
        CHECK(s.compare(3, 3, s, 0, 3) > 0);  // "def" > "abc"
    }
}

TEST_CASE("StrN comparison operators") {
    SUBCASE("operator< basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        CHECK(s1 < s2);      // "abc" < "def"
        CHECK_FALSE(s2 < s1); // NOT "def" < "abc"
        CHECK_FALSE(s1 < s3); // NOT "abc" < "abc" (equal)
    }

    SUBCASE("operator> basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        CHECK(s2 > s1);      // "def" > "abc"
        CHECK_FALSE(s1 > s2); // NOT "abc" > "def"
        CHECK_FALSE(s1 > s3); // NOT "abc" > "abc" (equal)
    }

    SUBCASE("operator<= basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        CHECK(s1 <= s2);     // "abc" <= "def"
        CHECK(s1 <= s3);     // "abc" <= "abc" (equal)
        CHECK_FALSE(s2 <= s1); // NOT "def" <= "abc"
    }

    SUBCASE("operator>= basic comparison") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";
        fl::StrN<32> s3 = "abc";

        CHECK(s2 >= s1);     // "def" >= "abc"
        CHECK(s1 >= s3);     // "abc" >= "abc" (equal)
        CHECK_FALSE(s1 >= s2); // NOT "abc" >= "def"
    }

    SUBCASE("comparison with different template sizes") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<64> s2 = "def";
        fl::StrN<128> s3 = "abc";

        // Test < operator
        CHECK(s1 < s2);      // "abc" < "def"
        CHECK_FALSE(s2 < s1); // NOT "def" < "abc"
        CHECK_FALSE(s1 < s3); // NOT "abc" < "abc" (equal)

        // Test > operator
        CHECK(s2 > s1);      // "def" > "abc"
        CHECK_FALSE(s1 > s2); // NOT "abc" > "def"
        CHECK_FALSE(s1 > s3); // NOT "abc" > "abc" (equal)

        // Test <= operator
        CHECK(s1 <= s2);     // "abc" <= "def"
        CHECK(s1 <= s3);     // "abc" <= "abc" (equal)
        CHECK_FALSE(s2 <= s1); // NOT "def" <= "abc"

        // Test >= operator
        CHECK(s2 >= s1);     // "def" >= "abc"
        CHECK(s1 >= s3);     // "abc" >= "abc" (equal)
        CHECK_FALSE(s1 >= s2); // NOT "abc" >= "def"
    }

    SUBCASE("comparison with empty strings") {
        fl::StrN<32> empty1 = "";
        fl::StrN<32> empty2 = "";
        fl::StrN<32> nonempty = "abc";

        // Empty strings are equal to each other
        CHECK_FALSE(empty1 < empty2);  // NOT "" < ""
        CHECK_FALSE(empty1 > empty2);  // NOT "" > ""
        CHECK(empty1 <= empty2);       // "" <= ""
        CHECK(empty1 >= empty2);       // "" >= ""

        // Empty string is less than non-empty
        CHECK(empty1 < nonempty);      // "" < "abc"
        CHECK_FALSE(empty1 > nonempty); // NOT "" > "abc"
        CHECK(empty1 <= nonempty);     // "" <= "abc"
        CHECK_FALSE(empty1 >= nonempty); // NOT "" >= "abc"

        // Non-empty string is greater than empty
        CHECK_FALSE(nonempty < empty1); // NOT "abc" < ""
        CHECK(nonempty > empty1);      // "abc" > ""
        CHECK_FALSE(nonempty <= empty1); // NOT "abc" <= ""
        CHECK(nonempty >= empty1);     // "abc" >= ""
    }

    SUBCASE("comparison with prefix strings") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "abcd";

        CHECK(s1 < s2);       // "abc" < "abcd" (prefix is less)
        CHECK_FALSE(s1 > s2);  // NOT "abc" > "abcd"
        CHECK(s1 <= s2);      // "abc" <= "abcd"
        CHECK_FALSE(s1 >= s2); // NOT "abc" >= "abcd"

        CHECK_FALSE(s2 < s1);  // NOT "abcd" < "abc"
        CHECK(s2 > s1);       // "abcd" > "abc"
        CHECK_FALSE(s2 <= s1); // NOT "abcd" <= "abc"
        CHECK(s2 >= s1);      // "abcd" >= "abc"
    }

    SUBCASE("case sensitivity") {
        fl::StrN<32> lower = "abc";
        fl::StrN<32> upper = "ABC";

        // Uppercase letters have lower ASCII values than lowercase
        CHECK(upper < lower);      // "ABC" < "abc" (ASCII 65 < 97)
        CHECK_FALSE(upper > lower); // NOT "ABC" > "abc"
        CHECK(upper <= lower);     // "ABC" <= "abc"
        CHECK_FALSE(upper >= lower); // NOT "ABC" >= "abc"
    }

    SUBCASE("lexicographical ordering for sorting") {
        fl::StrN<32> s1 = "apple";
        fl::StrN<32> s2 = "banana";
        fl::StrN<32> s3 = "cherry";
        fl::StrN<32> s4 = "apple";

        // Verify transitivity and consistency for sorting
        CHECK(s1 < s2);
        CHECK(s2 < s3);
        CHECK(s1 < s3);  // Transitive: if a<b and b<c, then a<c

        CHECK(s1 <= s4);  // Equal strings
        CHECK(s4 <= s1);  // Equal strings
        CHECK(s1 >= s4);  // Equal strings
        CHECK(s4 >= s1);  // Equal strings

        // Check reverse ordering
        CHECK(s3 > s2);
        CHECK(s2 > s1);
        CHECK(s3 > s1);

        CHECK(s3 >= s2);
        CHECK(s2 >= s1);
        CHECK(s3 >= s1);
    }

    SUBCASE("comparison with special characters") {
        fl::StrN<32> s1 = "abc!";
        fl::StrN<32> s2 = "abc@";
        fl::StrN<32> s3 = "abc#";

        // ASCII: ! (33) < # (35) < @ (64)
        CHECK(s1 < s3);       // "abc!" < "abc#"
        CHECK(s3 < s2);       // "abc#" < "abc@"
        CHECK(s1 < s2);       // "abc!" < "abc@"

        CHECK(s2 > s3);       // "abc@" > "abc#"
        CHECK(s3 > s1);       // "abc#" > "abc!"
        CHECK(s2 > s1);       // "abc@" > "abc!"
    }

    SUBCASE("comparison with number strings") {
        fl::StrN<32> s1 = "10";
        fl::StrN<32> s2 = "2";
        fl::StrN<32> s3 = "100";

        // Lexicographical, not numeric: "10" < "2" because '1' < '2'
        CHECK(s1 < s2);       // "10" < "2" (lexicographical)
        CHECK(s3 < s2);       // "100" < "2" (lexicographical)

        CHECK(s2 > s1);       // "2" > "10"
        CHECK(s2 > s3);       // "2" > "100"
    }

    SUBCASE("consistency with equality operators") {
        fl::StrN<32> s1 = "test";
        fl::StrN<32> s2 = "test";
        fl::StrN<32> s3 = "different";

        // If s1 == s2, then s1 <= s2 and s1 >= s2
        CHECK(s1 == s2);
        CHECK(s1 <= s2);
        CHECK(s1 >= s2);
        CHECK_FALSE(s1 < s2);
        CHECK_FALSE(s1 > s2);

        // If s1 != s3, then either s1 < s3 or s1 > s3
        CHECK(s1 != s3);
        bool one_comparison_true = (s1 < s3) || (s1 > s3);
        CHECK(one_comparison_true);
    }

    SUBCASE("comparison operator completeness") {
        fl::StrN<32> s1 = "abc";
        fl::StrN<32> s2 = "def";

        // Exactly one of <, ==, > should be true
        int count = 0;
        if (s1 < s2) count++;
        if (s1 == s2) count++;
        if (s1 > s2) count++;
        CHECK(count == 1);  // Exactly one should be true

        // Verify <= is equivalent to (< or ==)
        CHECK((s1 <= s2) == ((s1 < s2) || (s1 == s2)));

        // Verify >= is equivalent to (> or ==)
        CHECK((s1 >= s2) == ((s1 > s2) || (s1 == s2)));

        // Verify < is the opposite of >=
        CHECK((s1 < s2) == !(s1 >= s2));

        // Verify > is the opposite of <=
        CHECK((s1 > s2) == !(s1 <= s2));
    }

    SUBCASE("comparison with heap vs inline storage") {
        // Short string (inline storage)
        fl::StrN<64> short1 = "short";
        fl::StrN<64> short2 = "short";

        // Long string (heap storage) - exceeds 64 bytes
        fl::StrN<64> long1 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";
        fl::StrN<64> long2 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";

        // Comparison should work correctly regardless of storage type
        CHECK(short1 == short2);
        CHECK(short1 <= short2);
        CHECK(short1 >= short2);
        CHECK_FALSE(short1 < short2);
        CHECK_FALSE(short1 > short2);

        CHECK(long1 == long2);
        CHECK(long1 <= long2);
        CHECK(long1 >= long2);
        CHECK_FALSE(long1 < long2);
        CHECK_FALSE(long1 > long2);

        // Mixed: short vs long
        CHECK(short1 < long1);  // "short" < "this is..."
        CHECK(long1 > short1);  // "this is..." > "short"
    }
}
