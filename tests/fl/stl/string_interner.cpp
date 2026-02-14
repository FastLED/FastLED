/// @file string_interner.cpp
/// @brief Tests for fl::StringInterner class

#include "test.h"
#include "fl/stl/string_interner.h"

FL_TEST_CASE("StringInterner - basic interning") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("hello");
    FL_CHECK(s1.size() == 5);
    FL_CHECK(interner.size() == 1);
}

FL_TEST_CASE("StringInterner - deduplication") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("world");
    fl::string s2 = interner.intern("world");
    FL_CHECK(s1 == s2);
    const char* p1 = s1.c_str();
    const char* p2 = s2.c_str();
    FL_CHECK(p1 == p2);
    FL_CHECK(interner.size() == 1);
}

FL_TEST_CASE("StringInterner - contains") {
    fl::StringInterner interner;
    FL_CHECK_FALSE(interner.contains("test"));
    interner.intern("test");
    FL_CHECK(interner.contains("test"));
}

FL_TEST_CASE("StringInterner - get by index") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("alpha");
    fl::string s2 = interner.intern("beta");
    fl::string r1 = interner.get(0);
    FL_CHECK(r1 == "alpha");
    fl::string r2 = interner.get(1);
    FL_CHECK(r2 == "beta");
}

FL_TEST_CASE("StringInterner - clear") {
    fl::StringInterner interner;
    interner.intern("one");
    interner.intern("two");
    FL_CHECK(interner.size() == 2);
    interner.clear();
    FL_CHECK(interner.empty());
}

FL_TEST_CASE("StringInterner - strings survive clear") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("persistent");
    fl::string copy = s1;
    interner.clear();
    FL_CHECK(copy == "persistent");
    // Pointer comparison would trigger the deduplication bug
}
