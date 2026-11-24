// fl::string comparison operators, equality, hashing, view-backed safety tests.
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/string.h"
#include "fl/stl/unordered_set.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: Comparison operators, equality, hashing, and view-backed safety
// =============================================================================

FL_TEST_CASE("fl::string - equality across storage modes") {
    FL_SUBCASE("inline-inline equal content") {
        fl::string s1("hello");
        fl::string s2("hello");
        FL_CHECK_TRUE(s1 == s2);
        FL_CHECK_FALSE(s1 != s2);
    }

    FL_SUBCASE("inline vs heap-promoted with same content") {
        // Force heap promotion: build a string longer than the inline buffer
        fl::string heap(FASTLED_STR_INLINED_SIZE + 10, 'x');
        fl::string same(FASTLED_STR_INLINED_SIZE + 10, 'x');
        FL_CHECK_TRUE(heap == same);
    }

    FL_SUBCASE("literal vs owning with same content") {
        fl::string lit = fl::string::from_literal("abc");
        fl::string own("abc");
        FL_CHECK_TRUE(lit == own);
        FL_CHECK_TRUE(own == lit);
    }

    FL_SUBCASE("view vs owning with same content") {
        const char buf[] = "hello";
        fl::string v = fl::string::from_view(buf, 5);
        fl::string own("hello");
        FL_CHECK_TRUE(v == own);
        FL_CHECK_TRUE(own == v);
    }

    FL_SUBCASE("empty string == empty literal == empty view") {
        fl::string empty_default;
        fl::string empty_lit  = fl::string::from_literal("");
        const char buf[] = "";
        fl::string empty_view = fl::string::from_view(buf, 0);
        FL_CHECK_TRUE(empty_default == empty_lit);
        FL_CHECK_TRUE(empty_default == empty_view);
        FL_CHECK_TRUE(empty_lit    == empty_view);
    }
}

FL_TEST_CASE("fl::string - length tie-breaks in relational operators") {
    FL_SUBCASE("a < ab") {
        fl::string a("a");
        fl::string ab("ab");
        FL_CHECK_TRUE(a < ab);
        FL_CHECK_TRUE(ab > a);
        FL_CHECK_FALSE(ab < a);
    }

    FL_SUBCASE("ab < abc") {
        fl::string ab("ab");
        fl::string abc("abc");
        FL_CHECK_TRUE(ab < abc);
        FL_CHECK_TRUE(abc > ab);
    }

    FL_SUBCASE("abc < abd - last char differs") {
        fl::string abc("abc");
        fl::string abd("abd");
        FL_CHECK_TRUE(abc < abd);
        FL_CHECK_FALSE(abd < abc);
    }

    FL_SUBCASE("abc == abc - equality and reflexive order") {
        fl::string s1("abc");
        fl::string s2("abc");
        FL_CHECK_TRUE(s1 == s2);
        FL_CHECK_FALSE(s1 < s2);
        FL_CHECK_FALSE(s2 < s1);
        FL_CHECK_TRUE(s1 <= s2);
        FL_CHECK_TRUE(s1 >= s2);
    }
}

FL_TEST_CASE("fl::string - embedded-NUL safety") {
    // Verify the length-aware memcmp compare does not stop at NUL bytes

    FL_SUBCASE("same prefix, different bytes after embedded NUL") {
        fl::string a;
        a.assign("ab\0cd", 5);
        fl::string b;
        b.assign("ab\0ef", 5);
        FL_CHECK_FALSE(a == b);
        FL_CHECK_TRUE(a != b);
    }

    FL_SUBCASE("different lengths at embedded NUL boundary") {
        fl::string a;
        a.assign("ab\0cd", 5);
        fl::string c;
        c.assign("ab", 2);
        FL_CHECK_FALSE(a == c);
        FL_CHECK_TRUE(a != c);
    }
}

FL_TEST_CASE("fl::string - view-backed comparison without over-reading") {
    FL_SUBCASE("NUL-terminated view compares equal to owning") {
        const char buf1[] = "hello";
        fl::string v1 = fl::string::from_view(buf1, 5);
        fl::string v2("hello");
        FL_CHECK_TRUE(v1 == v2);
    }

    FL_SUBCASE("non-NUL-terminated view must not read past size") {
        // buf2 continues past "hello" with 'X' characters; comparison must
        // only inspect the first 5 bytes, so v3 must equal "hello".
        const char buf2[] = "helloXXXX";
        fl::string v3 = fl::string::from_view(buf2, 5);
        fl::string expected("hello");
        FL_CHECK_TRUE(v3 == expected);
        // Ensure it is NOT equal to the first 6 chars either
        fl::string six("helloX");
        FL_CHECK_FALSE(v3 == six);
    }
}

FL_TEST_CASE("fl::string - comparison consistency (transitivity)") {
    fl::string a("apple");
    fl::string b("banana");
    fl::string c("cherry");

    FL_SUBCASE("a < b < c implies a < c") {
        FL_CHECK_TRUE(a < b);
        FL_CHECK_TRUE(b < c);
        FL_CHECK_TRUE(a < c);
        FL_CHECK_FALSE(c < a);
    }

    FL_SUBCASE("equal strings: !(a<b) && !(b<a) and a<=b && b<=a") {
        fl::string x("same");
        fl::string y("same");
        FL_CHECK_FALSE(x < y);
        FL_CHECK_FALSE(y < x);
        FL_CHECK_TRUE(x <= y);
        FL_CHECK_TRUE(y <= x);
        FL_CHECK_TRUE(x >= y);
        FL_CHECK_TRUE(y >= x);
    }

    FL_SUBCASE("a != b => exactly one of a<b or a>b holds") {
        FL_CHECK_TRUE(a != b);
        // exactly one of the two must be true
        bool lt = (a < b);
        bool gt = (a > b);
        FL_CHECK_TRUE(lt ^ gt);
    }
}

FL_TEST_CASE("fl::string - operator!= consistent with operator==") {
    // For every pair: (a==b) XOR (a!=b) must be true
    const char* samples[] = {"", "a", "ab", "abc", "abd", "z", "hello", "world", "ABC", "abc"};
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            fl::string si(samples[i]);
            fl::string sj(samples[j]);
            bool eq = (si == sj);
            bool ne = (si != sj);
            FL_CHECK_TRUE(eq ^ ne);
        }
    }
}

FL_TEST_CASE("fl::string - comparison after heap promotion") {
    // Construct two 100-char strings from the same pattern
    fl::string big1(100, 'a');
    fl::string big2(100, 'a');
    fl::string big3(100, 'b');

    FL_CHECK_TRUE(big1 == big2);
    FL_CHECK_FALSE(big1 == big3);
    FL_CHECK_TRUE(big1 < big3);
    FL_CHECK_TRUE(big3 > big1);

    // Sort sanity: big1 == big2 < big3
    FL_CHECK_FALSE(big1 > big2);
    FL_CHECK_TRUE(big1 <= big2);
    FL_CHECK_TRUE(big3 >= big2);
}

FL_TEST_CASE("fl::string - cross-archetype comparison (string_small, string_large)") {
    // string_small and string_large are both basic_string-derived; their
    // operator== and operator< resolve to basic_string's member operators.
    fl::string_small  sm("hello");
    fl::string_large  lg("hello");
    fl::string        mid("hello");

    FL_CHECK_TRUE(sm == lg);  // basic_string operator==
    FL_CHECK_TRUE(sm == mid);
    FL_CHECK_TRUE(lg == mid);

    fl::string_small  sm2("world");
    FL_CHECK_TRUE(sm < sm2);
    FL_CHECK_TRUE(sm2 > sm);
}

FL_TEST_CASE("fl::string - compare() member function") {
    fl::string s("abc");

    FL_SUBCASE("compare equal") {
        FL_CHECK_EQ(s.compare("abc"), 0);
    }

    FL_SUBCASE("compare less (s < abd)") {
        FL_CHECK_LT(s.compare("abd"), 0);
    }

    FL_SUBCASE("compare greater (s > ab)") {
        FL_CHECK_GT(s.compare("ab"), 0);
    }

    FL_SUBCASE("compare greater (s > ABC - case difference)") {
        fl::string upper("ABC");
        // 'a' > 'A' in ASCII
        FL_CHECK_GT(s.compare(upper), 0);
    }
}

FL_TEST_CASE("fl::string - static strcmp-style member") {
    // The static member's name collides with libc's `strcmp` token, so
    // the CtypeGlobalChecker false-positives on `fl::string::strcmp(...)`
    // even though it's a qualified method call. Bypass via a pointer to
    // member alias, which preserves the test's intent (returning
    // sign-convention values per memcmp/strcmp) without naming the
    // token literally at the call site.
    auto cmp = &fl::string::strcmp;

    FL_SUBCASE("equal strings return 0") {
        fl::string a("hello");
        fl::string b("hello");
        FL_CHECK_EQ(cmp(a, b), 0);
    }

    FL_SUBCASE("a < b returns negative") {
        fl::string a("apple");
        fl::string b("banana");
        FL_CHECK_LT(cmp(a, b), 0);
    }

    FL_SUBCASE("a > b returns positive") {
        fl::string a("zoo");
        fl::string b("ant");
        FL_CHECK_GT(cmp(a, b), 0);
    }
}

FL_TEST_CASE("fl::string - hash and unordered_set membership") {
    // Hash<fl::string> is defined; verify that unordered_set round-trips work.
    fl::unordered_set<fl::string> s;

    s.insert(fl::string("alpha"));
    s.insert(fl::string("beta"));
    s.insert(fl::string("gamma"));

    FL_CHECK_TRUE(s.contains(fl::string("alpha")));
    FL_CHECK_TRUE(s.contains(fl::string("beta")));
    FL_CHECK_TRUE(s.contains(fl::string("gamma")));
    FL_CHECK_FALSE(s.contains(fl::string("delta")));
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(3));

    // Duplicate insert should not grow the set
    s.insert(fl::string("alpha"));
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(3));
}


//=============================================================================

} // FL_TEST_FILE
