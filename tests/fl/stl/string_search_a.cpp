// fl::string basic ops (part A2: rfind, find_first_of, find_last_of).
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

} // FL_TEST_FILE
