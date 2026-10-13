// fl::string construction, assignment, and iterator high-risk scenario tests.
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/iterator.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: Construction + Assignment + Iterator high-risk scenarios
//=============================================================================

FL_TEST_CASE("fl::string ctor - edge inputs") {
    FL_SUBCASE("default ctor is empty") {
        fl::string s;
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_EQ(s.c_str()[0], '\0');
    }

    FL_SUBCASE("nullptr ctor does not crash and is empty") {
        fl::string s(static_cast<const char*>(nullptr));
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("empty c-string ctor is empty") {
        fl::string s("");
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("c-string ctor size and content") {
        fl::string s("hello");
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }

    FL_SUBCASE("c-string + len truncates") {
        fl::string s("hello", 3);
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "hel");
    }

    FL_SUBCASE("c-string + len=0 is empty") {
        fl::string s("hello", 0);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("fill ctor count=0 is empty") {
        fl::string s(0u, 'x');
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("fill ctor count=5") {
        fl::string s(5u, 'x');
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "xxxxx");
    }

    FL_SUBCASE("fill ctor count=100 is heap-backed") {
        fl::string s(100u, 'x');
        FL_CHECK_EQ(s.size(), 100u);
        for (fl::size i = 0; i < 100; ++i) {
            FL_CHECK_EQ(s[i], 'x');
        }
    }

    FL_SUBCASE("string_view default ctor gives empty string") {
        fl::string_view sv;
        fl::string s(sv);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("string_view with content") {
        fl::string s(fl::string_view("abc", 3));
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "abc");
    }

    FL_SUBCASE("span<const char> empty") {
        fl::span<const char> sp;
        fl::string s(sp);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("span<char> empty") {
        fl::span<char> sp;
        fl::string s(sp);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("InputIt range ctor") {
        const char arr[] = "hello";
        fl::string s(arr, arr + 5);
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }

    FL_SUBCASE("array literal ctor excludes NUL") {
        const char arr[6] = "hello";
        fl::string s(arr);
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }
}

FL_TEST_CASE("fl::string operator= edge cases") {
    FL_SUBCASE("assign nullptr empties string") {
        fl::string s("hello");
        s = static_cast<const char*>(nullptr);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("assign empty c-string") {
        fl::string s("hello");
        s = "";
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("assign c-string") {
        fl::string s;
        s = "hello";
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }

    FL_SUBCASE("copy assignment is deep - modifying copy does not affect original") {
        fl::string a("hello");
        fl::string b;
        b = a;
        b.append(" world");
        FL_CHECK_EQ(a, "hello");
        FL_CHECK_EQ(b, "hello world");
    }

    FL_SUBCASE("move assignment - source is in valid state after move") {
        fl::string a("hello");
        fl::string b;
        b = fl::move(a);
        FL_CHECK_EQ(b, "hello");
        // a must be in a valid (destructible) state; size query must not crash
        FL_CHECK_GE(a.size(), 0u);
    }

    FL_SUBCASE("array literal assignment excludes NUL") {
        const char arr[6] = "hello";
        fl::string s;
        s = arr;
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }

    FL_SUBCASE("self copy assignment leaves string unchanged") {
        fl::string s("hello");
        fl::string& ref = s;
        s = ref;
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }

    FL_SUBCASE("self move assignment leaves string in valid state") {
        fl::string s("hello");
        s = fl::move(s);
        // After self-move the string must be destructible and size must be queryable
        FL_CHECK_GE(s.size(), 0u);
    }
}

FL_TEST_CASE("fl::string cross-size construction") {
    FL_SUBCASE("string_small construction from literal") {
        fl::string_small s("abc");
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "abc");
    }

    FL_SUBCASE("fl::string from string_small via basic_string ctor") {
        fl::string_small small("abc");
        fl::string s(static_cast<const fl::basic_string&>(small));
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "abc");
    }

    FL_SUBCASE("string_large from string_small") {
        fl::string_small small("abc");
        fl::string_large large(static_cast<const fl::basic_string&>(small));
        FL_CHECK_EQ(large.size(), 3u);
        FL_CHECK_EQ(large, "abc");
    }

    FL_SUBCASE("cross-size copies are independent") {
        fl::string_small small("abc");
        fl::string s(static_cast<const fl::basic_string&>(small));
        s.append("xyz");
        FL_CHECK_EQ(small, "abc");
        FL_CHECK_EQ(s, "abcxyz");
    }
}

FL_TEST_CASE("fl::string copy preserves storage independence") {
    FL_SUBCASE("copy of from_literal materializes independently") {
        fl::string a = fl::string::from_literal("hello");
        fl::string b = a;
        b.append(" world");
        FL_CHECK_EQ(a, "hello");
    }

    FL_SUBCASE("copy of heap-backed string is independent") {
        fl::string a(100u, 'A');
        fl::string b = a;
        b[0] = 'B';
        FL_CHECK_EQ(a[0], 'A');
        FL_CHECK_EQ(b[0], 'B');
    }
}

FL_TEST_CASE("fl::string move semantics") {
    FL_SUBCASE("move heap-backed: destination has content, source valid") {
        fl::string a(100u, 'Z');
        fl::string b(fl::move(a));
        FL_CHECK_EQ(b.size(), 100u);
        FL_CHECK_EQ(b[0], 'Z');
        FL_CHECK_EQ(b[99], 'Z');
        FL_CHECK_GE(a.size(), 0u);
    }

    FL_SUBCASE("move inline string: destination has content") {
        fl::string a("hi");
        fl::string b(fl::move(a));
        FL_CHECK_EQ(b, "hi");
        FL_CHECK_GE(a.size(), 0u);
    }
}

FL_TEST_CASE("fl::string iterator basics") {
    FL_SUBCASE("empty string: begin == end") {
        fl::string s;
        FL_CHECK_TRUE(s.begin() == s.end());
        FL_CHECK_TRUE(s.cbegin() == s.cend());
    }

    FL_SUBCASE("non-empty: *begin == s[0]") {
        fl::string s("hello");
        FL_CHECK_EQ(*s.begin(), s[0]);
    }

    FL_SUBCASE("end - begin == size") {
        fl::string s("hello");
        FL_CHECK_EQ(s.end() - s.begin(), static_cast<fl::ptrdiff_t>(s.size()));
    }

    FL_SUBCASE("cbegin value-equals begin") {
        fl::string s("hello");
        FL_CHECK_EQ(*s.cbegin(), *s.begin());
        FL_CHECK_EQ(s.cend() - s.cbegin(), static_cast<fl::ptrdiff_t>(s.size()));
    }

    FL_SUBCASE("rbegin != rend when non-empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.rbegin() != s.rend());
    }

    FL_SUBCASE("*rbegin == last char") {
        fl::string s("hello");
        FL_CHECK_EQ(*s.rbegin(), s[s.size() - 1]);
    }
}

FL_TEST_CASE("fl::string iterator arithmetic") {
    FL_SUBCASE("begin + 0 == begin") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.begin() + 0 == s.begin());
    }

    FL_SUBCASE("begin + size == end") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.begin() + static_cast<fl::ptrdiff_t>(s.size()) == s.end());
    }

    FL_SUBCASE("end - 1 points to last char") {
        fl::string s("hello");
        FL_CHECK_EQ(*(s.end() - 1), 'o');
    }

    FL_SUBCASE("begin < end when non-empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.begin() < s.end());
    }

    FL_SUBCASE("begin[n] == s[n] for all positions") {
        fl::string s("hello");
        for (fl::size i = 0; i < s.size(); ++i) {
            FL_CHECK_EQ(s.begin()[static_cast<fl::ptrdiff_t>(i)], s[i]);
        }
    }
}

FL_TEST_CASE("fl::string range-for") {
    FL_SUBCASE("empty string: range-for body executes zero times") {
        fl::string s;
        int count = 0;
        for (char c : s) {
            (void)c;
            ++count;
        }
        FL_CHECK_EQ(count, 0);
    }

    FL_SUBCASE("non-empty string: iterates chars in order") {
        fl::string s("abc");
        fl::string collected;
        for (char c : s) {
            collected.push_back(c);
        }
        FL_CHECK_EQ(collected, "abc");
    }
}

FL_TEST_CASE("fl::string assign variants") {
    FL_SUBCASE("assign(ptr, 3) truncates") {
        fl::string s;
        s.assign("hello", 3);
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "hel");
    }

    FL_SUBCASE("assign(ptr, 0) empties") {
        fl::string s("hello");
        s.assign("hello", 0);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("assign(string_view empty) empties") {
        fl::string s("hello");
        s.assign(fl::string_view());
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
    }

    FL_SUBCASE("assign(string_view with content)") {
        fl::string s;
        s.assign(fl::string_view("abc", 3));
        FL_CHECK_EQ(s.size(), 3u);
        FL_CHECK_EQ(s, "abc");
    }

    FL_SUBCASE("assign(count, char) fill") {
        fl::string s;
        s.assign(5u, 'x');
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "xxxxx");
    }

    FL_SUBCASE("assign(InputIt, InputIt) from range") {
        const char arr[] = "hello";
        fl::string s;
        s.assign(arr, arr + 5);
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_EQ(s, "hello");
    }
}

FL_TEST_CASE("fl::string reverse iterators") {
    FL_SUBCASE("rbegin is last char, incrementing walks backwards") {
        fl::string s("abc");
        auto it = s.rbegin();
        FL_CHECK_EQ(*it, 'c');
        ++it;
        FL_CHECK_EQ(*it, 'b');
        ++it;
        FL_CHECK_EQ(*it, 'a');
        ++it;
        FL_CHECK_TRUE(it == s.rend());
    }

    FL_SUBCASE("crbegin matches rbegin content") {
        fl::string s("abc");
        FL_CHECK_EQ(*s.crbegin(), *s.rbegin());
    }

    FL_SUBCASE("reverse walk matches forward walk reversed") {
        fl::string s("hello");
        fl::string reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) {
            reversed.push_back(*it);
        }
        FL_CHECK_EQ(reversed, "olleh");
    }
}

FL_TEST_CASE("fl::string at() vs operator[]") {
    FL_SUBCASE("at(0) and s[0] agree") {
        fl::string s("hello");
        FL_CHECK_EQ(s.at(0), s[0]);
    }

    FL_SUBCASE("at(size-1) and s[size-1] agree") {
        fl::string s("hello");
        FL_CHECK_EQ(s.at(s.size() - 1), s[s.size() - 1]);
    }

    FL_SUBCASE("at(size) returns null terminator (out-of-bounds is safe, returns 0)") {
        fl::string s("hello");
        FL_CHECK_EQ(s.at(s.size()), '\0');
    }
}

// ============================================================================
// Boundary conditions + storage-mode transitions
// (Cluster from test-writer-agent: empty variants, inline/heap boundary,
//  literal/view materialize, cross-size assign, shrink_to_fit, resize)
// ============================================================================

FL_TEST_CASE("string - empty variants all produce valid empty state") {
    FL_SUBCASE("default constructor") {
        fl::string s;
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_TRUE(s.c_str() != nullptr);
        FL_CHECK_EQ(s.c_str()[0], '\0');
        FL_CHECK_TRUE(s == "");
    }

    FL_SUBCASE("construction from empty literal") {
        fl::string s("");
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_EQ(s.c_str()[0], '\0');
        FL_CHECK_TRUE(s == "");
    }

    FL_SUBCASE("construction fill-count zero") {
        fl::string s(0u, 'x');
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_TRUE(s == "");
    }

    FL_SUBCASE("assign empty ptr+len") {
        fl::string s("hello");
        s.assign("", 0u);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_EQ(s.c_str()[0], '\0');
    }

    FL_SUBCASE("clear resets to empty") {
        fl::string s("hello");
        s.clear();
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_EQ(s.c_str()[0], '\0');
        FL_CHECK_TRUE(s == "");
    }
}

FL_TEST_CASE("string - sizes around FASTLED_STR_INLINED_SIZE boundary") {
    FL_SUBCASE("length 63 stays inline, content correct") {
        fl::string s(63u, 'a');
        FL_CHECK_EQ(s.size(), 63u);
        FL_CHECK_TRUE(s.is_owning());
        FL_CHECK_FALSE(s.is_referencing());
        FL_CHECK_EQ(s[62], 'a');
        FL_CHECK_EQ(s.at(63u), '\0');
    }

    FL_SUBCASE("length 64 exceeds inline, spills to heap") {
        fl::string s(64u, 'b');
        FL_CHECK_EQ(s.size(), 64u);
        FL_CHECK_TRUE(s.is_owning());
        FL_CHECK_FALSE(s.is_referencing());
        FL_CHECK_EQ(s[63], 'b');
        FL_CHECK_EQ(s.c_str()[64], '\0');
    }

    FL_SUBCASE("length 65 is heap-allocated") {
        fl::string s(65u, 'c');
        FL_CHECK_EQ(s.size(), 65u);
        FL_CHECK_TRUE(s.is_owning());
        FL_CHECK_EQ(s[64], 'c');
        FL_CHECK_EQ(s.c_str()[65], '\0');
    }
}

FL_TEST_CASE("string - inline to heap promotion preserves content") {
    fl::string s("hello");
    FL_CHECK_EQ(s.size(), 5u);
    FL_CHECK_TRUE(s.is_owning());

    fl::string suffix(60u, 'x');
    s.append(suffix.c_str());  // 5 + 60 = 65 > 64

    FL_CHECK_EQ(s.size(), 65u);
    FL_CHECK_TRUE(s.is_owning());
    FL_CHECK_FALSE(s.is_referencing());
    FL_CHECK_EQ(s[0], 'h');
    FL_CHECK_EQ(s[4], 'o');
    FL_CHECK_EQ(s[5], 'x');
    FL_CHECK_EQ(s[64], 'x');
    FL_CHECK_EQ(s.c_str()[65], '\0');
}

FL_TEST_CASE("string - literal to owning materialize on mutation") {
    const char* original = "foo";
    fl::string lit = fl::string::from_literal(original);

    FL_CHECK_TRUE(lit.is_referencing());
    FL_CHECK_FALSE(lit.is_owning());
    FL_CHECK_EQ(lit.size(), 3u);

    lit.append("x");

    FL_CHECK_FALSE(lit.is_referencing());
    FL_CHECK_TRUE(lit.is_owning());
    FL_CHECK_EQ(lit.size(), 4u);
    FL_CHECK_TRUE(lit == "foox");
    FL_CHECK_EQ(fl::strcmp(original, "foo"), 0);
}

FL_TEST_CASE("string - view to owning materialize via c_str") {
    char buf[8] = {'h', 'e', 'l', 'l', 'o', 'X', 'X', 'X'};
    fl::string sv = fl::string::from_view(buf, 5u);

    FL_CHECK_EQ(sv.size(), 5u);
    FL_CHECK_TRUE(sv.is_referencing());

    const char* cs = sv.c_str();

    FL_CHECK_FALSE(sv.is_referencing());
    FL_CHECK_TRUE(sv.is_owning());
    FL_CHECK_EQ(sv.size(), 5u);
    FL_CHECK_EQ(cs[5], '\0');
    FL_CHECK_EQ(fl::strcmp(cs, "hello"), 0);
}

FL_TEST_CASE("string - cross-size assignment small to large forces heap") {
    fl::string_small small_s;
    small_s = fl::string_large("longer than thirty-two characters here!!!");

    FL_CHECK_EQ(fl::strcmp(small_s.c_str(), "longer than thirty-two characters here!!!"), 0);
    FL_CHECK_EQ(small_s.size(), 41u);
    FL_CHECK_TRUE(small_s.is_owning());
    FL_CHECK_FALSE(small_s.is_referencing());
}

FL_TEST_CASE("string - shrink_to_fit drops heap back to inline when content fits") {
    fl::string s;
    s.reserve(200u);
    FL_CHECK_TRUE(s.is_owning());
    s.assign("short", 5u);
    FL_CHECK_EQ(s.size(), 5u);

    s.shrink_to_fit();

    FL_CHECK_TRUE(s.is_owning());
    FL_CHECK_FALSE(s.is_referencing());
    FL_CHECK_EQ(s.size(), 5u);
    FL_CHECK_TRUE(s == "short");
    FL_CHECK_LE(s.capacity(), static_cast<fl::size>(FASTLED_STR_INLINED_SIZE));
}

FL_TEST_CASE("string - reserve past inline then write short content") {
    fl::string s("hi");
    s.reserve(128u);
    FL_CHECK_GE(s.capacity(), 128u);
    FL_CHECK_EQ(s.size(), 2u);
    FL_CHECK_TRUE(s == "hi");
    FL_CHECK_TRUE(s.is_owning());
}

FL_TEST_CASE("string - resize boundaries") {
    FL_SUBCASE("resize(0) empties string") {
        fl::string s("hello");
        s.resize(0u);
        FL_CHECK_EQ(s.size(), 0u);
        FL_CHECK_TRUE(s.empty());
        FL_CHECK_EQ(s.c_str()[0], '\0');
    }

    FL_SUBCASE("resize(size()) is a no-op") {
        fl::string s("hello");
        s.resize(5u);
        FL_CHECK_EQ(s.size(), 5u);
        FL_CHECK_TRUE(s == "hello");
    }

    FL_SUBCASE("resize(size()+1, 'x') appends fill char") {
        fl::string s("hello");
        s.resize(6u, 'x');
        FL_CHECK_EQ(s.size(), 6u);
        FL_CHECK_EQ(s[5], 'x');
        FL_CHECK_EQ(s.c_str()[6], '\0');
        FL_CHECK_TRUE(s == "hellox");
    }

    FL_SUBCASE("resize(size()-1) shrinks by one") {
        fl::string s("hello");
        s.resize(4u);
        FL_CHECK_EQ(s.size(), 4u);
        FL_CHECK_TRUE(s == "hell");
        FL_CHECK_EQ(s.c_str()[4], '\0');
    }
}

// ============================================================================
// substr / find / rfind family + npos edge cases
// (Cluster from test-writer-agent: every overflow-prone path + boundary
//  positions + heap-promoted variants + starts_with/ends_with/contains)
// ============================================================================

FL_TEST_CASE("fl::string - substr npos overflow (regression: start+length wrap)") {
    FL_SUBCASE("substr(0, 0) -> empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substr(0, 0).empty());
    }

    FL_SUBCASE("substr(0, size()) -> full copy") {
        fl::string s("hello");
        FL_CHECK_EQ(s.substr(0, s.size()), s);
    }

    FL_SUBCASE("substr(0, npos) -> full copy [the fixed overflow]") {
        fl::string s("hello");
        fl::string result = s.substr(0, fl::string::npos);
        FL_CHECK_EQ(result, s);
        FL_CHECK_EQ(result.size(), static_cast<fl::size>(5));
    }

    FL_SUBCASE("substr(0, size()+10) -> full copy (clamped)") {
        fl::string s("hello");
        fl::string result = s.substr(0, s.size() + 10);
        FL_CHECK_EQ(result, s);
    }

    FL_SUBCASE("substr(size(), 0) -> empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substr(s.size(), 0).empty());
    }

    FL_SUBCASE("substr(size(), npos) -> empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substr(s.size(), fl::string::npos).empty());
    }

    FL_SUBCASE("substr(size()+1, npos) -> empty, must not crash") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substr(s.size() + 1, fl::string::npos).empty());
    }

    FL_SUBCASE("substr(2, npos) on 10-char string -> tail from pos 2") {
        fl::string s("0123456789");
        fl::string result = s.substr(2, fl::string::npos);
        FL_CHECK_EQ(result, fl::string("23456789"));
        FL_CHECK_EQ(result.size(), static_cast<fl::size>(8));
    }

    FL_SUBCASE("substr(2, size(-2)) huge non-npos -> clamped, not overflowed") {
        fl::string s("0123456789");
        fl::string result = s.substr(2, static_cast<fl::size>(-2));
        FL_CHECK_EQ(result, fl::string("23456789"));
    }
}

FL_TEST_CASE("fl::string - substring() API") {
    FL_SUBCASE("substring(0, 0) -> empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substring(0, 0).empty());
    }

    FL_SUBCASE("substring(0, size()) -> full") {
        fl::string s("hello");
        FL_CHECK_EQ(s.substring(0, s.size()), s);
    }

    FL_SUBCASE("substring(size(), size()) -> empty") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substring(s.size(), s.size()).empty());
    }

    FL_SUBCASE("substring(size(), size()+1) -> empty (start >= size)") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substring(s.size(), s.size() + 1).empty());
    }

    FL_SUBCASE("substring(3, 2) -> empty (start > end)") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.substring(3, 2).empty());
    }

    FL_SUBCASE("substring(0, size()+10) -> full (end clamped)") {
        fl::string s("hello");
        FL_CHECK_EQ(s.substring(0, s.size() + 10), s);
    }

    FL_SUBCASE("substring mid-string") {
        fl::string s("0123456789");
        FL_CHECK_EQ(s.substring(2, 6), fl::string("2345"));
    }
}

FL_TEST_CASE("fl::string - substr/substring on empty string") {
    fl::string empty;

    FL_SUBCASE("substr(0, 0)") {
        FL_CHECK_TRUE(empty.substr(0, 0).empty());
    }

    FL_SUBCASE("substr(0, npos)") {
        FL_CHECK_TRUE(empty.substr(0, fl::string::npos).empty());
    }

    FL_SUBCASE("substr(0, 100)") {
        FL_CHECK_TRUE(empty.substr(0, 100).empty());
    }

    FL_SUBCASE("substring(0, 0)") {
        FL_CHECK_TRUE(empty.substring(0, 0).empty());
    }

    FL_SUBCASE("substring(0, 1)") {
        FL_CHECK_TRUE(empty.substring(0, 1).empty());
    }
}

FL_TEST_CASE("fl::string - substr/substring on heap-promoted string (>64 chars)") {
    // 26 + 10 + 26 + 4 = 66 chars, which exceeds FASTLED_STR_INLINED_SIZE (64)
    // and forces the string to heap-promote.
    const char* long_prefix = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz_XYZW";
    fl::string s(long_prefix);
    FL_CHECK_GT(s.size(), static_cast<fl::size>(64));

    FL_SUBCASE("substr(0, npos) returns full copy from heap") {
        fl::string result = s.substr(0, fl::string::npos);
        FL_CHECK_EQ(result.size(), s.size());
        FL_CHECK_EQ(result, s);
    }

    FL_SUBCASE("substr(0, size()) returns full copy from heap") {
        fl::string result = s.substr(0, s.size());
        FL_CHECK_EQ(result, s);
    }

    FL_SUBCASE("substr mid-range from heap") {
        fl::string result = s.substr(2, 4);
        FL_CHECK_EQ(result, fl::string("CDEF"));
    }
}

FL_TEST_CASE("fl::string - find with npos position argument") {
    FL_SUBCASE("find(char, npos) -> npos, must not crash") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find('h', fl::string::npos), fl::string::npos);
        FL_CHECK_EQ(s.find('x', fl::string::npos), fl::string::npos);
    }

    FL_SUBCASE("find(char) in empty string -> npos") {
        fl::string s;
        FL_CHECK_EQ(s.find('x'), fl::string::npos);
    }

    FL_SUBCASE("find(char) match at position 0") {
        fl::string s("abc");
        FL_CHECK_EQ(s.find('a'), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find(char) match at last position") {
        fl::string s("abcz");
        FL_CHECK_EQ(s.find('z'), static_cast<fl::size>(3));
    }

    FL_SUBCASE("find(empty string) -> 0 (per std)") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find(""), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find empty string in empty string -> 0") {
        fl::string s;
        FL_CHECK_EQ(s.find(""), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find needle == haystack -> 0") {
        fl::string s("abc");
        FL_CHECK_EQ(s.find("abc"), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find needle longer than haystack -> npos") {
        fl::string s("ab");
        FL_CHECK_EQ(s.find("abcd"), fl::string::npos);
    }

    FL_SUBCASE("find(char, size()) -> npos") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find('h', s.size()), fl::string::npos);
    }
}

FL_TEST_CASE("fl::string - rfind edge cases") {
    FL_SUBCASE("rfind(char) with no match -> npos") {
        fl::string s("hello");
        FL_CHECK_EQ(s.rfind('x'), fl::string::npos);
    }

    FL_SUBCASE("rfind(char) match only at position 0") {
        fl::string s("hbcde");
        FL_CHECK_EQ(s.rfind('h'), static_cast<fl::size>(0));
    }

    FL_SUBCASE("rfind(char) match only at size()-1") {
        fl::string s("abcdz");
        FL_CHECK_EQ(s.rfind('z'), static_cast<fl::size>(4));
    }

    FL_SUBCASE("rfind(char, 0) -> 0 if s[0]==char, else npos") {
        fl::string s("hello");
        FL_CHECK_EQ(s.rfind('h', 0), static_cast<fl::size>(0));
        FL_CHECK_EQ(s.rfind('e', 0), fl::string::npos);
    }

    FL_SUBCASE("rfind(char, npos) -> searches whole string") {
        fl::string s("abcba");
        FL_CHECK_EQ(s.rfind('a', fl::string::npos), static_cast<fl::size>(4));
    }

    FL_SUBCASE("rfind on empty string -> npos for char, 0 for empty needle") {
        fl::string s;
        FL_CHECK_EQ(s.rfind('x'), fl::string::npos);
        FL_CHECK_EQ(s.rfind(""), static_cast<fl::size>(0));
    }
}

FL_TEST_CASE("fl::string - find_first_not_of and find_first_of edges") {
    FL_SUBCASE("find_first_not_of with empty set -> first char (pos 0)") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find_first_not_of(""), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find_first_not_of whitespace trim pattern") {
        fl::string s("   text");
        FL_CHECK_EQ(s.find_first_not_of(" \t"), static_cast<fl::size>(3));
    }

    FL_SUBCASE("find_first_not_of all matching -> npos") {
        fl::string s("aaaa");
        FL_CHECK_EQ(s.find_first_not_of("a"), fl::string::npos);
    }

    FL_SUBCASE("find_first_not_of pos at size() -> npos") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find_first_not_of("x", s.size()), fl::string::npos);
    }

    FL_SUBCASE("find_first_of 'a' or 'b' in 'cccccab' -> 5") {
        fl::string s("cccccab");
        FL_CHECK_EQ(s.find_first_of("ab"), static_cast<fl::size>(5));
    }
}

FL_TEST_CASE("fl::string - find_last_not_of edge cases") {
    FL_SUBCASE("find_last_not_of on empty string -> npos") {
        fl::string s;
        FL_CHECK_EQ(s.find_last_not_of("abc"), fl::string::npos);
        FL_CHECK_EQ(s.find_last_not_of('x'), fl::string::npos);
    }

    FL_SUBCASE("find_last_not_of all chars in set -> npos") {
        fl::string s("aaaa");
        FL_CHECK_EQ(s.find_last_not_of('a'), fl::string::npos);
        FL_CHECK_EQ(s.find_last_not_of("a"), fl::string::npos);
    }

    FL_SUBCASE("find_last_not_of with pos=0 stops after first char") {
        fl::string s("xhello");
        FL_CHECK_EQ(s.find_last_not_of("abc", 0), static_cast<fl::size>(0));
    }

    FL_SUBCASE("find_last_not_of with pos=npos searches whole string") {
        fl::string s("hello!");
        FL_CHECK_EQ(s.find_last_not_of("helo", fl::string::npos), static_cast<fl::size>(5));
    }

    FL_SUBCASE("find_last_of pos=0 only checks first char") {
        fl::string s("hello");
        FL_CHECK_EQ(s.find_last_of("h", 0), static_cast<fl::size>(0));
        FL_CHECK_EQ(s.find_last_of("e", 0), fl::string::npos);
    }
}

FL_TEST_CASE("fl::string - starts_with / ends_with / contains") {
    FL_SUBCASE("starts_with empty prefix -> true") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.starts_with(""));
        FL_CHECK_TRUE(s.starts_with(fl::string("")));
    }

    FL_SUBCASE("starts_with prefix longer than string -> false") {
        fl::string s("hi");
        FL_CHECK_FALSE(s.starts_with("hello"));
    }

    FL_SUBCASE("starts_with exact match -> true") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.starts_with("hello"));
        FL_CHECK_TRUE(s.starts_with(s));
    }

    FL_SUBCASE("starts_with single char") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.starts_with('h'));
        FL_CHECK_FALSE(s.starts_with('e'));
    }

    FL_SUBCASE("starts_with on empty string") {
        fl::string s;
        FL_CHECK_TRUE(s.starts_with(""));
        FL_CHECK_FALSE(s.starts_with("a"));
        FL_CHECK_FALSE(s.starts_with('a'));
    }

    FL_SUBCASE("ends_with empty suffix -> true") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.ends_with(""));
        FL_CHECK_TRUE(s.ends_with(fl::string("")));
    }

    FL_SUBCASE("ends_with suffix longer than string -> false") {
        fl::string s("hi");
        FL_CHECK_FALSE(s.ends_with("hello"));
    }

    FL_SUBCASE("ends_with exact match -> true") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.ends_with("hello"));
    }

    FL_SUBCASE("ends_with single char") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.ends_with('o'));
        FL_CHECK_FALSE(s.ends_with('h'));
    }

    FL_SUBCASE("ends_with on empty string") {
        fl::string s;
        FL_CHECK_TRUE(s.ends_with(""));
        FL_CHECK_FALSE(s.ends_with("a"));
        FL_CHECK_FALSE(s.ends_with('a'));
    }

    FL_SUBCASE("contains char/substring basic") {
        fl::string s("hello world");
        FL_CHECK_TRUE(s.contains('w'));
        FL_CHECK_FALSE(s.contains('z'));
        FL_CHECK_TRUE(s.contains("world"));
        FL_CHECK_FALSE(s.contains("xyz"));
    }

    FL_SUBCASE("contains empty string -> true (find returns 0)") {
        fl::string s("hello");
        FL_CHECK_TRUE(s.contains(""));
    }

    FL_SUBCASE("contains on empty string") {
        fl::string s;
        FL_CHECK_FALSE(s.contains('a'));
        FL_CHECK_FALSE(s.contains("a"));
    }
}

} // FL_TEST_FILE
