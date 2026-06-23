// fl::string basic ops part B: reverse iterators, compare, comparison operators.
// Extracted from string.cpp (sub-issue of #3131, meta #3127).
// (Second half of the original str.cpp section content.)

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstring.h"
#include "fl/stl/iterator.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// Test reverse iterators (fl::string compatibility)
FL_TEST_CASE("string reverse iterators") {
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

FL_TEST_CASE("string comparison operators") {
    FL_SUBCASE("operator< basic comparison") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        fl::string s3 = "abc";

        FL_CHECK(s1 < s2);      // "abc" < "def"
        FL_CHECK_FALSE(s2 < s1); // NOT "def" < "abc"
        FL_CHECK_FALSE(s1 < s3); // NOT "abc" < "abc" (equal)
    }

    FL_SUBCASE("operator> basic comparison") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        fl::string s3 = "abc";

        FL_CHECK(s2 > s1);      // "def" > "abc"
        FL_CHECK_FALSE(s1 > s2); // NOT "abc" > "def"
        FL_CHECK_FALSE(s1 > s3); // NOT "abc" > "abc" (equal)
    }

    FL_SUBCASE("operator<= basic comparison") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        fl::string s3 = "abc";

        FL_CHECK(s1 <= s2);     // "abc" <= "def"
        FL_CHECK(s1 <= s3);     // "abc" <= "abc" (equal)
        FL_CHECK_FALSE(s2 <= s1); // NOT "def" <= "abc"
    }

    FL_SUBCASE("operator>= basic comparison") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        fl::string s3 = "abc";

        FL_CHECK(s2 >= s1);     // "def" >= "abc"
        FL_CHECK(s1 >= s3);     // "abc" >= "abc" (equal)
        FL_CHECK_FALSE(s1 >= s2); // NOT "abc" >= "def"
    }

    FL_SUBCASE("comparison with different template sizes") {
        fl::string s1 = "abc";
        fl::string s2 = "def";
        fl::string s3 = "abc";

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
        fl::string empty1 = "";
        fl::string empty2 = "";
        fl::string nonempty = "abc";

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
        fl::string s1 = "abc";
        fl::string s2 = "abcd";

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
        fl::string lower = "abc";
        fl::string upper = "ABC";

        // Uppercase letters have lower ASCII values than lowercase
        FL_CHECK(upper < lower);      // "ABC" < "abc" (ASCII 65 < 97)
        FL_CHECK_FALSE(upper > lower); // NOT "ABC" > "abc"
        FL_CHECK(upper <= lower);     // "ABC" <= "abc"
        FL_CHECK_FALSE(upper >= lower); // NOT "ABC" >= "abc"
    }

    FL_SUBCASE("lexicographical ordering for sorting") {
        fl::string s1 = "apple";
        fl::string s2 = "banana";
        fl::string s3 = "cherry";
        fl::string s4 = "apple";

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
        fl::string s1 = "abc!";
        fl::string s2 = "abc@";
        fl::string s3 = "abc#";

        // ASCII: ! (33) < # (35) < @ (64)
        FL_CHECK(s1 < s3);       // "abc!" < "abc#"
        FL_CHECK(s3 < s2);       // "abc#" < "abc@"
        FL_CHECK(s1 < s2);       // "abc!" < "abc@"

        FL_CHECK(s2 > s3);       // "abc@" > "abc#"
        FL_CHECK(s3 > s1);       // "abc#" > "abc!"
        FL_CHECK(s2 > s1);       // "abc@" > "abc!"
    }

    FL_SUBCASE("comparison with number strings") {
        fl::string s1 = "10";
        fl::string s2 = "2";
        fl::string s3 = "100";

        // Lexicographical, not numeric: "10" < "2" because '1' < '2'
        FL_CHECK(s1 < s2);       // "10" < "2" (lexicographical)
        FL_CHECK(s3 < s2);       // "100" < "2" (lexicographical)

        FL_CHECK(s2 > s1);       // "2" > "10"
        FL_CHECK(s2 > s3);       // "2" > "100"
    }

    FL_SUBCASE("consistency with equality operators") {
        fl::string s1 = "test";
        fl::string s2 = "test";
        fl::string s3 = "different";

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
        fl::string s1 = "abc";
        fl::string s2 = "def";

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
        fl::string short1 = "short";
        fl::string short2 = "short";

        // Long string (heap storage) - exceeds 64 bytes
        fl::string long1 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";
        fl::string long2 = "this is a very long string that definitely exceeds the inline buffer size of 64 bytes";

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

} // FL_TEST_FILE
