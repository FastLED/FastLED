
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "str.h"
#include "fixed_vector.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("Str basic operations") {
    SUBCASE("Construction and assignment") {
        Str s1;
        CHECK(s1.size() == 0);
        CHECK(s1.c_str()[0] == '\0');

        Str s2("hello");
        CHECK(s2.size() == 5);
        CHECK(strcmp(s2.c_str(), "hello") == 0);

        Str s3 = s2;
        CHECK(s3.size() == 5);
        CHECK(strcmp(s3.c_str(), "hello") == 0);

        s1 = "world";
        CHECK(s1.size() == 5);
        CHECK(strcmp(s1.c_str(), "world") == 0);
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
        CHECK(strcmp(s.c_str(), "hello world") == 0);
    }

    SUBCASE("Copy-on-write behavior") {
        Str s1("hello");
        Str s2 = s1;
        s2.append(" world");
        CHECK(strcmp(s1.c_str(), "hello") == 0);
        CHECK(strcmp(s2.c_str(), "hello world") == 0);
    }
}

TEST_CASE("Str with FixedVector") {
    FixedVector<Str, 10> vec;
    vec.push_back(Str("hello"));
    vec.push_back(Str("world"));

    CHECK(vec.size() == 2);
    CHECK(strcmp(vec[0].c_str(), "hello") == 0);
    CHECK(strcmp(vec[1].c_str(), "world") == 0);
}

TEST_CASE("Str with long strings") {
    const char* long_string = "This is a very long string that exceeds the inline buffer size and should be allocated on the heap";
    Str s(long_string);
    CHECK(s.size() == strlen(long_string));
    CHECK(strcmp(s.c_str(), long_string) == 0);

    Str s2 = s;
    CHECK(s2.size() == strlen(long_string));
    CHECK(strcmp(s2.c_str(), long_string) == 0);

    s2.append(" with some additional text");
    CHECK(strcmp(s.c_str(), long_string) == 0);  // Original string should remain unchanged
}

TEST_CASE("Str overflowing inline data") {
    SUBCASE("Construction with long string") {
        std::string long_string(FASTLED_STR_INLINED_SIZE + 10, 'a');  // Create a string longer than the inline buffer
        Str s(long_string.c_str());
        CHECK(s.size() == long_string.length());
        CHECK(strcmp(s.c_str(), long_string.c_str()) == 0);
    }

    SUBCASE("Appending to overflow") {
        Str s("Short string");
        std::string append_string(FASTLED_STR_INLINED_SIZE, 'b');  // String to append that will cause overflow
        s.append(append_string.c_str());
        CHECK(s.size() == strlen("Short string") + append_string.length());
        CHECK(s[0] == 'S');
        CHECK(s[s.size() - 1] == 'b');
    }

    SUBCASE("Copy on write with long string") {
        std::string long_string(FASTLED_STR_INLINED_SIZE + 20, 'c');
        Str s1(long_string.c_str());
        Str s2 = s1;
        CHECK(s1.size() == s2.size());
        CHECK(strcmp(s1.c_str(), s2.c_str()) == 0);

        s2.append("extra");
        CHECK(s1.size() == long_string.length());
        CHECK(s2.size() == long_string.length() + 5);
        CHECK(strcmp(s1.c_str(), long_string.c_str()) == 0);
        CHECK(s2[s2.size() - 1] == 'a');
    }
}
