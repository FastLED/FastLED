// Test file for fl/stl/cstring.h
//
// This file tests all string and memory functions provided by fl/stl/cstring.h,
// which wraps standard C library functions in the fl:: namespace.
//
// Functions tested:
// String functions: strlen, strcmp, strncmp, strcpy, strncpy, strcat, strncat,
//                   strstr, strchr, strrchr, strspn, strcspn, strpbrk, strtok
// Memory functions: memcpy, memcmp, memmove, memset, memchr
// Legacy functions: memfill, memcopy
//
// Note: PROGMEM functions (_P variants) are platform-specific (AVR only)
// and are not tested in this host-based test suite.

#include "fl/stl/cstring.h"
#include "fl/stl/cstddef.h"
#include "doctest.h"

// Note: We do NOT use "using namespace fl;" because string functions
// would conflict with system C library functions (strcmp, strcpy, etc.)
// Instead, we explicitly use fl:: prefix for all function calls.

// ============================================================================
// String Length and Comparison Tests
// ============================================================================

TEST_CASE("fl::strlen") {
    SUBCASE("empty string") {
        CHECK_EQ(fl::strlen(""), 0);
    }

    SUBCASE("single character") {
        CHECK_EQ(fl::strlen("a"), 1);
    }

    SUBCASE("normal string") {
        CHECK_EQ(fl::strlen("hello"), 5);
        CHECK_EQ(fl::strlen("FastLED"), 7);
    }

    SUBCASE("string with spaces") {
        CHECK_EQ(fl::strlen("hello world"), 11);
    }

    SUBCASE("string with special characters") {
        CHECK_EQ(fl::strlen("test\n\t"), 6);
    }
}

TEST_CASE("fl::strcmp") {
    SUBCASE("equal strings") {
        CHECK_EQ(fl::strcmp("hello", "hello"), 0);
        CHECK_EQ(fl::strcmp("", ""), 0);
    }

    SUBCASE("first less than second") {
        CHECK(fl::strcmp("abc", "abd") < 0);
        CHECK(fl::strcmp("a", "b") < 0);
    }

    SUBCASE("first greater than second") {
        CHECK(fl::strcmp("abd", "abc") > 0);
        CHECK(fl::strcmp("b", "a") > 0);
    }

    SUBCASE("different lengths") {
        CHECK(fl::strcmp("hello", "hello world") < 0);
        CHECK(fl::strcmp("hello world", "hello") > 0);
    }

    SUBCASE("case sensitive") {
        CHECK(fl::strcmp("Hello", "hello") != 0);
        CHECK(fl::strcmp("HELLO", "hello") != 0);
    }
}

TEST_CASE("fl::strncmp") {
    SUBCASE("compare n characters - equal") {
        CHECK_EQ(fl::strncmp("hello", "hello", 5), 0);
        CHECK_EQ(fl::strncmp("hello world", "hello there", 5), 0);
    }

    SUBCASE("compare n characters - different") {
        CHECK(fl::strncmp("abc", "abd", 3) < 0);
        CHECK(fl::strncmp("abd", "abc", 3) > 0);
    }

    SUBCASE("compare less than full length") {
        CHECK_EQ(fl::strncmp("hello", "help", 2), 0);  // "he" == "he"
        CHECK(fl::strncmp("hello", "help", 4) != 0);   // "hell" != "help"
    }

    SUBCASE("compare with n=0") {
        CHECK_EQ(fl::strncmp("abc", "xyz", 0), 0);
    }

    SUBCASE("compare n > string length") {
        CHECK_EQ(fl::strncmp("abc", "abc", 100), 0);
    }
}

// ============================================================================
// String Copy and Concatenation Tests
// ============================================================================

TEST_CASE("fl::strcpy") {
    char buffer[100];

    SUBCASE("copy empty string") {
        fl::strcpy(buffer, "");
        CHECK_EQ(fl::strlen(buffer), 0);
    }

    SUBCASE("copy short string") {
        fl::strcpy(buffer, "hello");
        CHECK_EQ(fl::strcmp(buffer, "hello"), 0);
        CHECK_EQ(fl::strlen(buffer), 5);
    }

    SUBCASE("copy replaces previous content") {
        fl::strcpy(buffer, "first");
        fl::strcpy(buffer, "new");
        CHECK_EQ(fl::strcmp(buffer, "new"), 0);
        CHECK_EQ(fl::strlen(buffer), 3);
    }

    SUBCASE("return value is destination") {
        char* result = fl::strcpy(buffer, "test");
        CHECK_EQ(result, buffer);
    }
}

TEST_CASE("fl::strncpy") {
    char buffer[100];

    SUBCASE("copy n characters") {
        fl::strncpy(buffer, "hello", 3);
        buffer[3] = '\0';  // null terminate manually
        CHECK_EQ(fl::strcmp(buffer, "hel"), 0);
    }

    SUBCASE("copy with n >= source length - pads with zeros") {
        fl::memset(buffer, 'X', sizeof(buffer));
        fl::strncpy(buffer, "hi", 5);
        CHECK_EQ(buffer[0], 'h');
        CHECK_EQ(buffer[1], 'i');
        CHECK_EQ(buffer[2], '\0');
        CHECK_EQ(buffer[3], '\0');
        CHECK_EQ(buffer[4], '\0');
    }

    SUBCASE("copy with n < source length - no null terminator") {
        fl::memset(buffer, 'X', sizeof(buffer));
        fl::strncpy(buffer, "hello", 3);
        CHECK_EQ(buffer[0], 'h');
        CHECK_EQ(buffer[1], 'e');
        CHECK_EQ(buffer[2], 'l');
        CHECK_EQ(buffer[3], 'X');  // Original content preserved
    }

    SUBCASE("return value is destination") {
        char* result = fl::strncpy(buffer, "test", 4);
        CHECK_EQ(result, buffer);
    }
}

TEST_CASE("fl::strcat") {
    char buffer[100];

    SUBCASE("concatenate to empty string") {
        buffer[0] = '\0';
        fl::strcat(buffer, "hello");
        CHECK_EQ(fl::strcmp(buffer, "hello"), 0);
    }

    SUBCASE("concatenate two strings") {
        fl::strcpy(buffer, "hello");
        fl::strcat(buffer, " world");
        CHECK_EQ(fl::strcmp(buffer, "hello world"), 0);
        CHECK_EQ(fl::strlen(buffer), 11);
    }

    SUBCASE("concatenate multiple times") {
        fl::strcpy(buffer, "a");
        fl::strcat(buffer, "b");
        fl::strcat(buffer, "c");
        CHECK_EQ(fl::strcmp(buffer, "abc"), 0);
    }

    SUBCASE("return value is destination") {
        fl::strcpy(buffer, "test");
        char* result = fl::strcat(buffer, "123");
        CHECK_EQ(result, buffer);
    }
}

TEST_CASE("fl::strncat") {
    char buffer[100];

    SUBCASE("concatenate n characters") {
        fl::strcpy(buffer, "hello");
        fl::strncat(buffer, " world", 3);
        CHECK_EQ(fl::strcmp(buffer, "hello wo"), 0);
    }

    SUBCASE("concatenate with n >= source length") {
        fl::strcpy(buffer, "hello");
        fl::strncat(buffer, " world", 100);
        CHECK_EQ(fl::strcmp(buffer, "hello world"), 0);
    }

    SUBCASE("concatenate with n=0") {
        fl::strcpy(buffer, "hello");
        fl::strncat(buffer, " world", 0);
        CHECK_EQ(fl::strcmp(buffer, "hello"), 0);
    }

    SUBCASE("return value is destination") {
        fl::strcpy(buffer, "test");
        char* result = fl::strncat(buffer, "123", 3);
        CHECK_EQ(result, buffer);
    }
}

// ============================================================================
// String Search Tests
// ============================================================================

TEST_CASE("fl::strstr") {
    SUBCASE("find substring at beginning") {
        const char* result = fl::strstr("hello world", "hello");
        CHECK(result != nullptr);
        CHECK_EQ(result, static_cast<const char*>("hello world"));
    }

    SUBCASE("find substring in middle") {
        const char* haystack = "hello world";
        const char* result = fl::strstr(haystack, "lo wo");
        CHECK(result != nullptr);
        CHECK_EQ(result, haystack + 3);
    }

    SUBCASE("find substring at end") {
        const char* haystack = "hello world";
        const char* result = fl::strstr(haystack, "world");
        CHECK(result != nullptr);
        CHECK_EQ(result, haystack + 6);
    }

    SUBCASE("substring not found") {
        const char* result = fl::strstr("hello world", "xyz");
        CHECK(result == nullptr);
    }

    SUBCASE("empty needle returns haystack") {
        const char* haystack = "hello";
        const char* result = fl::strstr(haystack, "");
        CHECK_EQ(result, haystack);
    }

    SUBCASE("case sensitive") {
        const char* result = fl::strstr("hello world", "WORLD");
        CHECK(result == nullptr);
    }
}

TEST_CASE("fl::strchr") {
    char buffer[] = "hello world";

    SUBCASE("find character at beginning") {
        char* result = fl::strchr(buffer, 'h');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer);
    }

    SUBCASE("find character in middle") {
        char* result = fl::strchr(buffer, 'o');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 4);  // First 'o' in "hello"
    }

    SUBCASE("find character at end") {
        char* result = fl::strchr(buffer, 'd');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 10);
    }

    SUBCASE("character not found") {
        char* result = fl::strchr(buffer, 'x');
        CHECK(result == nullptr);
    }

    SUBCASE("find null terminator") {
        char* result = fl::strchr(buffer, '\0');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 11);
    }

    SUBCASE("const version") {
        const char* const_buffer = "hello";
        const char* result = fl::strchr(const_buffer, 'l');
        CHECK(result != nullptr);
        CHECK_EQ(result, const_buffer + 2);
    }
}

TEST_CASE("fl::strrchr") {
    char buffer[] = "hello world";

    SUBCASE("find last occurrence in middle") {
        char* result = fl::strrchr(buffer, 'o');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 7);  // Second 'o' in "world"
    }

    SUBCASE("find single occurrence") {
        char* result = fl::strrchr(buffer, 'h');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer);
    }

    SUBCASE("character not found") {
        char* result = fl::strrchr(buffer, 'x');
        CHECK(result == nullptr);
    }

    SUBCASE("find null terminator") {
        char* result = fl::strrchr(buffer, '\0');
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 11);
    }

    SUBCASE("const version") {
        const char* const_buffer = "hello";
        const char* result = fl::strrchr(const_buffer, 'l');
        CHECK(result != nullptr);
        CHECK_EQ(result, const_buffer + 3);  // Last 'l'
    }
}

TEST_CASE("fl::strspn") {
    SUBCASE("count matching prefix") {
        CHECK_EQ(fl::strspn("abcdefg", "abc"), 3);
        CHECK_EQ(fl::strspn("1234abc", "0123456789"), 4);
    }

    SUBCASE("no matching characters") {
        CHECK_EQ(fl::strspn("hello", "xyz"), 0);
    }

    SUBCASE("all characters match") {
        CHECK_EQ(fl::strspn("aaa", "a"), 3);
        CHECK_EQ(fl::strspn("abc", "cba"), 3);
    }

    SUBCASE("empty strings") {
        CHECK_EQ(fl::strspn("", "abc"), 0);
        CHECK_EQ(fl::strspn("abc", ""), 0);
    }
}

TEST_CASE("fl::strcspn") {
    SUBCASE("count non-matching prefix") {
        CHECK_EQ(fl::strcspn("hello world", " "), 5);
        CHECK_EQ(fl::strcspn("abc123", "0123456789"), 3);
    }

    SUBCASE("no matching characters - returns length") {
        CHECK_EQ(fl::strcspn("hello", "xyz"), 5);
    }

    SUBCASE("first character matches") {
        CHECK_EQ(fl::strcspn("hello", "h"), 0);
    }

    SUBCASE("empty strings") {
        CHECK_EQ(fl::strcspn("", "abc"), 0);
        CHECK_EQ(fl::strcspn("abc", ""), 3);
    }
}

TEST_CASE("fl::strpbrk") {
    char buffer[] = "hello world";

    SUBCASE("find first occurrence of any character") {
        char* result = fl::strpbrk(buffer, "aeiou");
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 1);  // First 'e'
    }

    SUBCASE("find with multiple matching characters") {
        char* result = fl::strpbrk(buffer, " w");
        CHECK(result != nullptr);
        CHECK_EQ(result, buffer + 5);  // Space comes first
    }

    SUBCASE("no matching characters") {
        char* result = fl::strpbrk(buffer, "xyz");
        CHECK(result == nullptr);
    }

    SUBCASE("empty search set") {
        char* result = fl::strpbrk(buffer, "");
        CHECK(result == nullptr);
    }

    SUBCASE("const version") {
        const char* const_buffer = "hello";
        const char* result = fl::strpbrk(const_buffer, "aeiou");
        CHECK(result != nullptr);
        CHECK_EQ(result, const_buffer + 1);
    }
}

TEST_CASE("fl::strtok") {
    char buffer[100];

    SUBCASE("tokenize with single delimiter") {
        fl::strcpy(buffer, "hello world test");
        char* token1 = fl::strtok(buffer, " ");
        CHECK(token1 != nullptr);
        CHECK_EQ(fl::strcmp(token1, "hello"), 0);

        char* token2 = fl::strtok(nullptr, " ");
        CHECK(token2 != nullptr);
        CHECK_EQ(fl::strcmp(token2, "world"), 0);

        char* token3 = fl::strtok(nullptr, " ");
        CHECK(token3 != nullptr);
        CHECK_EQ(fl::strcmp(token3, "test"), 0);

        char* token4 = fl::strtok(nullptr, " ");
        CHECK(token4 == nullptr);
    }

    SUBCASE("tokenize with multiple delimiters") {
        fl::strcpy(buffer, "one,two:three;four");
        char* token1 = fl::strtok(buffer, ",:;");
        CHECK_EQ(fl::strcmp(token1, "one"), 0);

        char* token2 = fl::strtok(nullptr, ",:;");
        CHECK_EQ(fl::strcmp(token2, "two"), 0);

        char* token3 = fl::strtok(nullptr, ",:;");
        CHECK_EQ(fl::strcmp(token3, "three"), 0);

        char* token4 = fl::strtok(nullptr, ",:;");
        CHECK_EQ(fl::strcmp(token4, "four"), 0);
    }

    SUBCASE("consecutive delimiters are skipped") {
        fl::strcpy(buffer, "a  b    c");
        char* token1 = fl::strtok(buffer, " ");
        CHECK_EQ(fl::strcmp(token1, "a"), 0);

        char* token2 = fl::strtok(nullptr, " ");
        CHECK_EQ(fl::strcmp(token2, "b"), 0);

        char* token3 = fl::strtok(nullptr, " ");
        CHECK_EQ(fl::strcmp(token3, "c"), 0);
    }
}

// ============================================================================
// Memory Operation Tests
// ============================================================================

TEST_CASE("fl::memcpy") {
    SUBCASE("copy small block") {
        char src[] = "hello";
        char dest[10];
        fl::memcpy(dest, src, 6);  // Include null terminator
        CHECK_EQ(fl::strcmp(dest, "hello"), 0);
    }

    SUBCASE("copy integers") {
        int src[] = {1, 2, 3, 4, 5};
        int dest[5];
        fl::memcpy(dest, src, sizeof(src));
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(dest[i], src[i]);
        }
    }

    SUBCASE("copy structs") {
        struct Test {
            int a;
            double b;
        };
        Test src = {42, 3.14};
        Test dest;
        fl::memcpy(&dest, &src, sizeof(Test));
        CHECK_EQ(dest.a, 42);
        CHECK_EQ(dest.b, 3.14);
    }

    SUBCASE("copy zero bytes is safe") {
        char src[] = "hello";
        char dest[10] = "world";
        fl::memcpy(dest, src, 0);
        CHECK_EQ(fl::strcmp(dest, "world"), 0);  // Unchanged
    }

    SUBCASE("return value is destination") {
        char src[] = "test";
        char dest[10];
        void* result = fl::memcpy(dest, src, 5);
        CHECK_EQ(result, static_cast<void*>(dest));
    }
}

TEST_CASE("fl::memmove") {
    SUBCASE("move non-overlapping regions") {
        char src[] = "hello";
        char dest[10];
        fl::memmove(dest, src, 6);
        CHECK_EQ(fl::strcmp(dest, "hello"), 0);
    }

    SUBCASE("move overlapping regions - forward") {
        char buffer[] = "hello world";
        fl::memmove(buffer + 2, buffer, 5);  // Move "hello" to position 2
        CHECK_EQ(buffer[2], 'h');
        CHECK_EQ(buffer[3], 'e');
        CHECK_EQ(buffer[4], 'l');
        CHECK_EQ(buffer[5], 'l');
        CHECK_EQ(buffer[6], 'o');
    }

    SUBCASE("move overlapping regions - backward") {
        char buffer[] = "hello world";
        fl::memmove(buffer, buffer + 6, 5);  // Move "world" to beginning
        CHECK_EQ(buffer[0], 'w');
        CHECK_EQ(buffer[1], 'o');
        CHECK_EQ(buffer[2], 'r');
        CHECK_EQ(buffer[3], 'l');
        CHECK_EQ(buffer[4], 'd');
    }

    SUBCASE("return value is destination") {
        char buffer[] = "test";
        void* result = fl::memmove(buffer, buffer, 4);
        CHECK_EQ(result, static_cast<void*>(buffer));
    }
}

TEST_CASE("fl::memset") {
    SUBCASE("set bytes to value") {
        char buffer[10];
        fl::memset(buffer, 'A', 5);
        CHECK_EQ(buffer[0], 'A');
        CHECK_EQ(buffer[1], 'A');
        CHECK_EQ(buffer[4], 'A');
    }

    SUBCASE("clear buffer to zero") {
        int buffer[5] = {1, 2, 3, 4, 5};
        fl::memset(buffer, 0, sizeof(buffer));
        for (int i = 0; i < 5; i++) {
            CHECK_EQ(buffer[i], 0);
        }
    }

    SUBCASE("set with n=0 is safe") {
        char buffer[] = "hello";
        fl::memset(buffer, 'X', 0);
        CHECK_EQ(fl::strcmp(buffer, "hello"), 0);  // Unchanged
    }

    SUBCASE("return value is destination") {
        char buffer[10];
        void* result = fl::memset(buffer, 0, 10);
        CHECK_EQ(result, static_cast<void*>(buffer));
    }
}

TEST_CASE("fl::memcmp") {
    SUBCASE("equal memory regions") {
        char a[] = "hello";
        char b[] = "hello";
        CHECK_EQ(fl::memcmp(a, b, 5), 0);
    }

    SUBCASE("first less than second") {
        char a[] = "abc";
        char b[] = "abd";
        CHECK(fl::memcmp(a, b, 3) < 0);
    }

    SUBCASE("first greater than second") {
        char a[] = "abd";
        char b[] = "abc";
        CHECK(fl::memcmp(a, b, 3) > 0);
    }

    SUBCASE("compare fewer bytes than difference") {
        char a[] = "hello";
        char b[] = "help!";
        CHECK_EQ(fl::memcmp(a, b, 2), 0);  // "he" == "he"
        CHECK(fl::memcmp(a, b, 4) != 0);   // "hell" != "help"
    }

    SUBCASE("compare integers") {
        int a[] = {1, 2, 3};
        int b[] = {1, 2, 3};
        CHECK_EQ(fl::memcmp(a, b, sizeof(a)), 0);
    }

    SUBCASE("compare with n=0") {
        CHECK_EQ(fl::memcmp("abc", "xyz", 0), 0);
    }
}

TEST_CASE("fl::memchr") {
    SUBCASE("find byte in memory") {
        char buffer[] = "hello world";
        void* result = fl::memchr(buffer, 'o', 11);
        CHECK(result != nullptr);
        CHECK_EQ(static_cast<char*>(result), buffer + 4);  // First 'o'
    }

    SUBCASE("find byte at beginning") {
        char buffer[] = "hello";
        void* result = fl::memchr(buffer, 'h', 5);
        CHECK(result != nullptr);
        CHECK_EQ(static_cast<char*>(result), buffer);
    }

    SUBCASE("find byte at end") {
        char buffer[] = "hello";
        void* result = fl::memchr(buffer, 'o', 5);
        CHECK(result != nullptr);
        CHECK_EQ(static_cast<char*>(result), buffer + 4);
    }

    SUBCASE("byte not found") {
        char buffer[] = "hello";
        void* result = fl::memchr(buffer, 'x', 5);
        CHECK(result == nullptr);
    }

    SUBCASE("search limited by n parameter") {
        char buffer[] = "hello";
        void* result = fl::memchr(buffer, 'o', 3);  // Only search "hel"
        CHECK(result == nullptr);
    }

    SUBCASE("find zero byte") {
        char buffer[] = "hello";
        void* result = fl::memchr(buffer, 0, 6);
        CHECK(result != nullptr);
        CHECK_EQ(static_cast<char*>(result), buffer + 5);
    }

    SUBCASE("const version") {
        const char* const_buffer = "hello";
        const void* result = fl::memchr(const_buffer, 'l', 5);
        CHECK(result != nullptr);
        CHECK_EQ(static_cast<const char*>(result), const_buffer + 2);
    }
}

// ============================================================================
// Legacy Function Tests
// ============================================================================

TEST_CASE("fl::memfill") {
    SUBCASE("memfill is alias for memset") {
        char buffer[10];
        fl::memfill(buffer, 'X', 5);
        CHECK_EQ(buffer[0], 'X');
        CHECK_EQ(buffer[4], 'X');
    }

    SUBCASE("return value is destination") {
        char buffer[10];
        void* result = fl::memfill(buffer, 0, 10);
        CHECK_EQ(result, static_cast<void*>(buffer));
    }
}

TEST_CASE("fl::memcopy") {
    SUBCASE("memcopy is alias for memcpy") {
        char src[] = "hello";
        char dest[10];
        fl::memcopy(dest, src, 6);
        CHECK_EQ(fl::strcmp(dest, "hello"), 0);
    }

    SUBCASE("return value is destination") {
        char src[] = "test";
        char dest[10];
        void* result = fl::memcopy(dest, src, 5);
        CHECK_EQ(result, static_cast<void*>(dest));
    }
}

// ============================================================================
// Integration and Edge Case Tests
// ============================================================================

TEST_CASE("fl::cstring integration tests") {
    SUBCASE("build string with multiple operations") {
        char buffer[100];
        fl::strcpy(buffer, "Hello");
        fl::strcat(buffer, " ");
        fl::strcat(buffer, "World");
        CHECK_EQ(fl::strcmp(buffer, "Hello World"), 0);
        CHECK_EQ(fl::strlen(buffer), 11);
    }

    SUBCASE("manipulate string in place") {
        char buffer[] = "hello world";
        char* space = fl::strchr(buffer, ' ');
        CHECK(space != nullptr);
        *space = '_';
        CHECK_EQ(fl::strcmp(buffer, "hello_world"), 0);
    }

    SUBCASE("copy and compare memory") {
        int src[] = {1, 2, 3, 4, 5};
        int dest[5];
        fl::memcpy(dest, src, sizeof(src));
        CHECK_EQ(fl::memcmp(src, dest, sizeof(src)), 0);
    }

    SUBCASE("clear structure with memset") {
        struct Data {
            int x;
            int y;
            char name[10];
        };
        Data d;
        fl::memset(&d, 0, sizeof(d));
        CHECK_EQ(d.x, 0);
        CHECK_EQ(d.y, 0);
        CHECK_EQ(d.name[0], '\0');
    }
}

TEST_CASE("fl::cstring type safety") {
    SUBCASE("functions work with fl::size_t") {
        const char* str = "hello";
        size_t len = fl::strlen(str);
        CHECK_EQ(len, 5);

        char buffer[10];
        fl::memset(buffer, 0, len);
        fl::memcpy(buffer, str, len);
        CHECK_EQ(fl::memcmp(buffer, str, len), 0);
    }
}
