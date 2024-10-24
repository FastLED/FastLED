
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "str.h"
#include "fixed_vector.h"

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
