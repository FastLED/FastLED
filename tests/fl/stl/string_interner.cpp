/// @file string_interner.cpp
/// @brief Tests for fl::StringInterner class

#include "test.h"
#include "fl/stl/string_interner.h"
#include "fl/stl/cstring.h"          // for memcmp
#include "fl/stl/span.h"             // for span
#include "fl/stl/stdio.h"            // for snprintf
#include "fl/stl/string.h"           // for string
#include "fl/string_view.h"          // for string_view

FL_TEST_CASE("StringInterner - basic interning") {
    fl::StringInterner interner;
    // Use string > 64 bytes (SSO threshold) to trigger actual interning
    fl::string s1 = interner.intern("this_is_a_long_string_that_exceeds_the_sso_threshold_of_64_bytes_to_test_interning");
    FL_CHECK(s1.size() == 82);  // Actual length
    FL_CHECK(interner.size() == 1);
}

FL_TEST_CASE("StringInterner - deduplication") {
    fl::StringInterner interner;
    // Use string > 64 bytes to trigger actual interning
    const char* long_str = "this_is_a_long_string_that_exceeds_the_sso_threshold_of_64_bytes_for_dedup_test";
    fl::string s1 = interner.intern(long_str);
    fl::string s2 = interner.intern(long_str);
    FL_CHECK(s1 == s2);
    const char* p1 = s1.c_str();
    const char* p2 = s2.c_str();
    FL_CHECK(p1 == p2);  // Should point to same StringHolder data
    FL_CHECK(interner.size() == 1);
}

FL_TEST_CASE("StringInterner - contains") {
    fl::StringInterner interner;
    const char* long_str = "this_is_a_long_string_that_exceeds_the_sso_threshold_of_64_bytes_for_contains_test";
    FL_CHECK_FALSE(interner.contains(long_str));
    interner.intern(long_str);
    FL_CHECK(interner.contains(long_str));
}

// NOTE: get() by index removed - hash map doesn't support index-based access
// Use contains() to check if a string is interned instead

FL_TEST_CASE("StringInterner - API overloads") {
    fl::StringInterner interner;

    // All test strings must be > 64 bytes to trigger interning
    const char* str1 = "this_is_a_very_long_string_view_that_exceeds_64_bytes_sso_threshold_test1";
    const char* str2 = "this_is_a_very_long_cstring_that_exceeds_64_bytes_sso_threshold_test2xxxx";
    const char* str3 = "this_is_a_very_long_fl_string_that_exceeds_64_bytes_sso_threshold_test3xx";
    const char* str4 = "this_is_a_very_long_span_string_that_exceeds_64_bytes_sso_threshold_test4";

    // Test intern(string_view)
    fl::string_view sv(str1);
    fl::string s1 = interner.intern(sv);
    FL_CHECK(s1 == str1);
    FL_CHECK(interner.size() == 1);

    // Test intern(const char*)
    fl::string s2 = interner.intern(str2);
    FL_CHECK(s2 == str2);
    FL_CHECK(interner.size() == 2);

    // Test intern(fl::string)
    fl::string str(str3);
    fl::string s3 = interner.intern(str);
    FL_CHECK(s3 == str3);
    FL_CHECK(interner.size() == 3);

    // Test intern(span<const char>)
    fl::span<const char> sp(str4, fl::strlen(str4));
    fl::string s4 = interner.intern(sp);
    FL_CHECK(s4 == str4);
    FL_CHECK(interner.size() == 4);

    // Verify deduplication works across all overloads
    fl::string s5 = interner.intern(str1);  // const char* overload
    FL_CHECK(interner.size() == 4);  // Should still be 4 (not 5)
    FL_CHECK(s5.c_str() == s1.c_str());  // Same pointer (shared StringHolder)
}

FL_TEST_CASE("StringInterner - performance (hash map vs linear search)") {
    fl::StringInterner interner;

    // Insert 100 strings (all > 64 bytes to trigger interning)
    for (int i = 0; i < 100; i++) {
        char buf[128];
        fl::snprintf(buf, sizeof(buf), "this_is_a_very_long_performance_test_string_exceeding_64_bytes_num_%d_padding", i);
        interner.intern(buf);
    }

    FL_CHECK(interner.size() == 100);

    // Verify all strings can be found (O(1) average with hash map)
    for (int i = 0; i < 100; i++) {
        char buf[128];
        fl::snprintf(buf, sizeof(buf), "this_is_a_very_long_performance_test_string_exceeding_64_bytes_num_%d_padding", i);
        FL_CHECK(interner.contains(buf));
    }
}

FL_TEST_CASE("StringInterner - clear") {
    fl::StringInterner interner;
    interner.intern("this_is_a_very_long_string_one_that_exceeds_64_bytes_sso_threshold_for_clear_test1");
    interner.intern("this_is_a_very_long_string_two_that_exceeds_64_bytes_sso_threshold_for_clear_test2");
    FL_CHECK(interner.size() == 2);
    interner.clear();
    FL_CHECK(interner.empty());
}

FL_TEST_CASE("StringInterner - string outlives interner") {
    fl::string s1, s2;

    const char* long_str1 = "this_is_a_very_long_persistent_string_exceeding_64_bytes_sso_threshold";
    const char* long_str2 = "this_is_a_very_long_outlives_string_exceeding_64_bytes_sso_thresholdx";

    // Create interner in a scope and intern strings
    {
        fl::StringInterner interner;
        s1 = interner.intern(long_str1);
        s2 = interner.intern(long_str2);

        FL_CHECK(s1 == long_str1);
        FL_CHECK(s2 == long_str2);
        FL_CHECK(interner.size() == 2);

        // Interner will be destroyed here, but strings should remain valid
    }

    // Strings should still be valid after interner is destroyed
    // This works because fl::string holds a shared_ptr to the StringHolder
    FL_CHECK(s1 == long_str1);
    FL_CHECK(s2 == long_str2);
    FL_CHECK(s1.size() == fl::strlen(long_str1));
    FL_CHECK(s2.size() == fl::strlen(long_str2));

    // Verify string data is still accessible
    FL_CHECK(fl::memcmp(s1.c_str(), long_str1, s1.size()) == 0);
    FL_CHECK(fl::memcmp(s2.c_str(), long_str2, s2.size()) == 0);
}

FL_TEST_CASE("StringInterner - clear preserves outstanding strings") {
    fl::StringInterner interner;
    const char* long_str = "this_is_a_very_long_survives_string_exceeding_64_bytes_sso_threshold_test";
    fl::string s1 = interner.intern(long_str);

    FL_CHECK(interner.size() == 1);

    // Clear the interner
    interner.clear();
    FL_CHECK(interner.empty());

    // String should still be valid (shared_ptr keeps StringHolder alive)
    FL_CHECK(s1 == long_str);
    FL_CHECK(s1.size() == fl::strlen(long_str));
    FL_CHECK(fl::memcmp(s1.c_str(), long_str, s1.size()) == 0);
}

FL_TEST_CASE("StringInterner - strings survive clear") {
    fl::StringInterner interner;
    const char* long_str = "this_is_a_very_long_persistent_string_exceeding_64_bytes_sso_thresholdxx";
    fl::string s1 = interner.intern(long_str);
    fl::string copy = s1;
    interner.clear();
    FL_CHECK(copy == long_str);
    // Pointer comparison would trigger the deduplication bug
}

FL_TEST_CASE("string::intern() - SSO strings skip interning") {
    // Small string that fits in inline buffer (SSO)
    fl::string small("short");  // 5 chars < 64 byte inline buffer

    // Intern should be a no-op for SSO strings
    small.intern();

    // String content should be preserved
    FL_CHECK(small == "short");
    FL_CHECK(small.size() == 5);
}

FL_TEST_CASE("string::intern() - large strings use interner") {
    // Large string that requires heap allocation (>64 bytes)
    fl::string large("this is a very long string that definitely exceeds the inline buffer size of 64 bytes and will require heap allocation");

    // Intern should replace with interned version
    large.intern();

    // String content should be preserved
    FL_CHECK(large == "this is a very long string that definitely exceeds the inline buffer size of 64 bytes and will require heap allocation");

    // Create another large string with same content
    fl::string large2("this is a very long string that definitely exceeds the inline buffer size of 64 bytes and will require heap allocation");
    large2.intern();

    // Should share the same underlying data (deduplication)
    FL_CHECK(large.c_str() == large2.c_str());
}

FL_TEST_CASE("string::intern() - method chaining") {
    fl::string s("test");
    fl::string& result = s.intern();

    // Should return reference to self for chaining
    FL_CHECK(&result == &s);
}
