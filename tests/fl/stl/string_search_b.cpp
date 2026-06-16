// fl::string basic ops (part A3: find_first_not_of, find_last_not_of).
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
