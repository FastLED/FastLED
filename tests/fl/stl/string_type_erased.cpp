// fl::string type-erased basic_string optimization tests.
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: Type-erased basic_string optimization tests
//=============================================================================

FL_TEST_CASE("basic_string COW safety") {
    FL_SUBCASE("Copy shares heap data") {
        // Create a string long enough to be heap-allocated (> 64 chars)
        fl::string original(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage");
        fl::string copy = original;

        // Both should have the same content
        FL_CHECK(original == copy);
        FL_CHECK(original.c_str() == copy.c_str()); // Same pointer = shared
    }

    FL_SUBCASE("Mutable iterator detaches COW") {
        fl::string original(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage");
        fl::string copy = original;

        // Verify sharing
        FL_CHECK(original.c_str() == copy.c_str());

        // Modify copy via mutable iterator — should COW-detach
        auto it = copy.begin();
        *it = 'X';

        // copy should be modified, original unchanged
        FL_CHECK(copy[0] == 'X');
        FL_CHECK(original[0] == 'T');
        FL_CHECK(original.c_str() != copy.c_str()); // No longer shared
    }

    FL_SUBCASE("Append to shared string detaches COW") {
        fl::string original(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage");
        fl::string copy = original;

        copy.append("!");

        FL_CHECK(original.size() != copy.size());
        FL_CHECK(fl::strcmp(original.c_str(),
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage") == 0);
    }
}

FL_TEST_CASE("basic_string move semantics") {
    FL_SUBCASE("Move construct from inline") {
        fl::string src("hello");
        const char* old_data = src.c_str();
        (void)old_data;

        fl::string dst(fl::move(src));
        FL_CHECK(dst == "hello");
        FL_CHECK(src.empty());
    }

    FL_SUBCASE("Move construct from heap") {
        fl::string src(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage");
        fl::string dst(fl::move(src));

        FL_CHECK(fl::strcmp(dst.c_str(),
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage") == 0);
        FL_CHECK(src.empty());
    }

    FL_SUBCASE("Move assign") {
        fl::string dst("old");
        fl::string src("new value");

        dst = fl::move(src);
        FL_CHECK(dst == "new value");
        FL_CHECK(src.empty());
    }
}

FL_TEST_CASE("basic_string inline to heap transition") {
    FL_SUBCASE("Append beyond inline capacity") {
        fl::string s;
        // Inline capacity is 64 chars
        for (int i = 0; i < 100; ++i) {
            s.append("x");
        }
        FL_CHECK(s.size() == 100);
        // Verify all chars are correct
        for (fl::size i = 0; i < s.size(); ++i) {
            FL_CHECK(s[i] == 'x');
        }
    }

    FL_SUBCASE("Copy after transition preserves data") {
        fl::string s;
        for (int i = 0; i < 100; ++i) {
            s.append("A");
        }
        fl::string copy = s;
        FL_CHECK(copy.size() == 100);
        FL_CHECK(copy == s);
    }
}

FL_TEST_CASE("basic_string self operations") {
    FL_SUBCASE("Self copy assignment") {
        fl::string s("hello world");
        s = s; // Self-assign
        FL_CHECK(s == "hello world");
    }

    FL_SUBCASE("Self copy with heap data") {
        fl::string s(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage");
        fl::string& ref = s;
        s = ref; // Self-assign via reference
        FL_CHECK(fl::strcmp(s.c_str(),
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated for storage") == 0);
    }
}

// Historically tested cross-size StrN<N> copies; with StrN removed
// and all callers using `fl::string`, these become same-size sanity
// checks for assignment + heap-promotion still covering the same
// behaviors (long strings spill from inline buffer to heap).
FL_TEST_CASE("basic_string assignment + heap promotion") {
    FL_SUBCASE("Copy between same-size fl::string instances") {
        fl::string big("hello from big buffer");
        fl::string small = big;
        FL_CHECK(small == "hello from big buffer");
    }

    FL_SUBCASE("Copy between two fl::string instances (reverse direction)") {
        fl::string small("hello from small buffer");
        fl::string big = small;
        FL_CHECK(big == "hello from small buffer");
    }

    FL_SUBCASE("Copy long string that overflows the inline buffer") {
        // String is longer than FASTLED_STR_INLINED_SIZE (default 64)
        // so heap promotion fires on construction and copy.
        const char* long_literal =
            "This is a deliberately long string that exceeds the default "
            "FASTLED_STR_INLINED_SIZE of 64 bytes so heap promotion fires "
            "on both construction and copy.";
        fl::string big(long_literal);
        FL_CHECK(big.size() > FASTLED_STR_INLINED_SIZE);
        fl::string tiny = big;
        FL_CHECK(tiny == long_literal);
        FL_CHECK(tiny.size() == big.size());
    }
}

FL_TEST_CASE("basic_string self-referential append") {
    FL_SUBCASE("Append self when heap grow needed") {
        // Create a heap-allocated string near capacity
        fl::string s(
            "This string is long enough to exceed the inline buffer capacity "
            "and must be heap allocated");
        fl::string expected = s;
        expected.append(s.c_str());

        s.append(s); // Self-referential - must not use-after-free

        FL_CHECK(s == expected);
    }

    FL_SUBCASE("Append self inline string") {
        fl::string s("hello");
        s.append(s);
        FL_CHECK(s == "hellohello");
    }
}

FL_TEST_CASE("basic_string insert on literal/view") {
    FL_SUBCASE("Insert into literal string") {
        fl::string s = fl::string::from_literal("hello world");
        s.insert(5, "!!");
        FL_CHECK(s == "hello!! world");
    }

    FL_SUBCASE("Replace on literal string") {
        fl::string s = fl::string::from_literal("hello world");
        s.replace(0, 5, "goodbye");
        FL_CHECK(s == "goodbye world");
    }
}

//=============================================================================

} // FL_TEST_FILE
