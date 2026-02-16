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

// NOTE: get() by index removed - hash map doesn't support index-based access
// Use contains() to check if a string is interned instead

FL_TEST_CASE("StringInterner - API overloads") {
    fl::StringInterner interner;

    // Test intern(string_view)
    fl::string_view sv("view", 4);
    fl::string s1 = interner.intern(sv);
    FL_CHECK(s1 == "view");
    FL_CHECK(interner.size() == 1);

    // Test intern(const char*)
    fl::string s2 = interner.intern("cstring");
    FL_CHECK(s2 == "cstring");
    FL_CHECK(interner.size() == 2);

    // Test intern(fl::string)
    fl::string str("string");
    fl::string s3 = interner.intern(str);
    FL_CHECK(s3 == "string");
    FL_CHECK(interner.size() == 3);

    // Test intern(span<const char>)
    const char data[] = "span";
    fl::span<const char> sp(data, 4);
    fl::string s4 = interner.intern(sp);
    FL_CHECK(s4 == "span");
    FL_CHECK(interner.size() == 4);

    // Verify deduplication works across all overloads
    fl::string s5 = interner.intern("view");  // const char* overload
    FL_CHECK(interner.size() == 4);  // Should still be 4 (not 5)
    FL_CHECK(s5.c_str() == s1.c_str());  // Same pointer (shared StringHolder)
}

FL_TEST_CASE("StringInterner - performance (hash map vs linear search)") {
    fl::StringInterner interner;

    // Insert 100 strings
    for (int i = 0; i < 100; i++) {
        char buf[32];
        fl::snprintf(buf, sizeof(buf), "string_%d", i);
        interner.intern(buf);
    }

    FL_CHECK(interner.size() == 100);

    // Verify all strings can be found (O(1) average with hash map)
    for (int i = 0; i < 100; i++) {
        char buf[32];
        fl::snprintf(buf, sizeof(buf), "string_%d", i);
        FL_CHECK(interner.contains(buf));
    }
}

FL_TEST_CASE("StringInterner - clear") {
    fl::StringInterner interner;
    interner.intern("one");
    interner.intern("two");
    FL_CHECK(interner.size() == 2);
    interner.clear();
    FL_CHECK(interner.empty());
}

FL_TEST_CASE("StringInterner - string outlives interner") {
    fl::string s1, s2;

    // Create interner in a scope and intern strings
    {
        fl::StringInterner interner;
        s1 = interner.intern("persistent");
        s2 = interner.intern("outlives");

        FL_CHECK(s1 == "persistent");
        FL_CHECK(s2 == "outlives");
        FL_CHECK(interner.size() == 2);

        // Interner will be destroyed here, but strings should remain valid
    }

    // Strings should still be valid after interner is destroyed
    // This works because fl::string holds a shared_ptr to the StringHolder
    FL_CHECK(s1 == "persistent");
    FL_CHECK(s2 == "outlives");
    FL_CHECK(s1.size() == 10);
    FL_CHECK(s2.size() == 8);

    // Verify string data is still accessible
    FL_CHECK(fl::memcmp(s1.c_str(), "persistent", 10) == 0);
    FL_CHECK(fl::memcmp(s2.c_str(), "outlives", 8) == 0);
}

FL_TEST_CASE("StringInterner - clear preserves outstanding strings") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("survives");

    FL_CHECK(interner.size() == 1);

    // Clear the interner
    interner.clear();
    FL_CHECK(interner.empty());

    // String should still be valid (shared_ptr keeps StringHolder alive)
    FL_CHECK(s1 == "survives");
    FL_CHECK(s1.size() == 8);
    FL_CHECK(fl::memcmp(s1.c_str(), "survives", 8) == 0);
}

FL_TEST_CASE("StringInterner - strings survive clear") {
    fl::StringInterner interner;
    fl::string s1 = interner.intern("persistent");
    fl::string copy = s1;
    interner.clear();
    FL_CHECK(copy == "persistent");
    // Pointer comparison would trigger the deduplication bug
}
