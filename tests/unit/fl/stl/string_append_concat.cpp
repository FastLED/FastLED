// fl::string append + concatenation + self-referential write tests.
// Extracted from string.cpp (sub-issue of #3131, meta #3127).

#include "fl/stl/compiler_control.h"
#include "fl/stl/string.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

// SECTION: append + concatenation + self-referential write
//=============================================================================

FL_TEST_CASE("fl::string - inline to heap boundary: one-at-a-time") {
    // FASTLED_STR_INLINED_SIZE is 64. Fill to exactly 64 chars one 'a' at a time.
    fl::string s;
    for (int i = 0; i < 64; ++i) {
        s.append('a');
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(i + 1));
    }
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(64));
    // All chars must be 'a'
    for (int i = 0; i < 64; ++i) {
        FL_CHECK_EQ(s[i], 'a');
    }
    // Push past inline into heap
    s.append('b');
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(65));
    FL_CHECK_EQ(s[64], 'b');
}

FL_TEST_CASE("fl::string - inline to heap boundary: single large append") {
    fl::string s;
    fl::string big(200, 'x');
    FL_CHECK_EQ(big.size(), static_cast<fl::size>(200));
    s.append(big);
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(200));
    for (int i = 0; i < 200; ++i) {
        FL_CHECK_EQ(s[i], 'x');
    }
}

FL_TEST_CASE("fl::string - append after prior heap promotion") {
    fl::string s(200, 'z');  // already heap
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(200));
    s.append("end");
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(203));
    FL_CHECK_EQ(s[200], 'e');
    FL_CHECK_EQ(s[201], 'n');
    FL_CHECK_EQ(s[202], 'd');
}

FL_TEST_CASE("fl::string - append integer boundaries") {
    FL_SUBCASE("i8 min and max") {
        fl::string s;
        s.append(static_cast<fl::i8>(-128));
        FL_CHECK_EQ(s, "-128");

        fl::string s2;
        s2.append(static_cast<fl::i8>(127));
        FL_CHECK_EQ(s2, "127");
    }

    FL_SUBCASE("u8 zero and max") {
        fl::string s;
        s.append(static_cast<fl::u8>(0));
        FL_CHECK_EQ(s, "0");

        fl::string s2;
        s2.append(static_cast<fl::u8>(255));
        FL_CHECK_EQ(s2, "255");
    }

    FL_SUBCASE("i32 min and max") {
        fl::string s;
        s.append(static_cast<fl::i32>(-2147483647 - 1));  // INT32_MIN without UB
        FL_CHECK_EQ(s, "-2147483648");

        fl::string s2;
        s2.append(static_cast<fl::i32>(2147483647));
        FL_CHECK_EQ(s2, "2147483647");
    }

    FL_SUBCASE("i64 min and max") {
        fl::string s;
        s.append(static_cast<fl::i64>(-9223372036854775807LL - 1LL));
        FL_CHECK_EQ(s, "-9223372036854775808");

        fl::string s2;
        s2.append(static_cast<fl::i64>(9223372036854775807LL));
        FL_CHECK_EQ(s2, "9223372036854775807");
    }

    FL_SUBCASE("u64 zero and max") {
        fl::string s;
        s.append(static_cast<fl::u64>(0));
        FL_CHECK_EQ(s, "0");

        fl::string s2;
        s2.append(static_cast<fl::u64>(18446744073709551615ULL));
        FL_CHECK_EQ(s2, "18446744073709551615");
    }
}

FL_TEST_CASE("fl::string - append float and double") {
    FL_SUBCASE("float basic values") {
        fl::string s;
        s.append(0.0f);
        FL_CHECK_EQ(s, "0.00");

        fl::string s2;
        s2.append(1.0f);
        FL_CHECK_EQ(s2, "1.00");

        fl::string s3;
        s3.append(-1.5f);
        FL_CHECK_EQ(s3, "-1.50");
    }

    FL_SUBCASE("float precision override") {
        fl::string s;
        s.append(3.14159f, 3);
        // Verify the first 5 chars are "3.14" prefix (precision 3 -> 3 decimal places)
        FL_CHECK_TRUE(s.starts_with("3.14"));
    }

    FL_SUBCASE("double basic values") {
        fl::string s;
        s.append(0.0);
        FL_CHECK_EQ(s, "0.00");

        fl::string s2;
        s2.append(1.0);
        FL_CHECK_EQ(s2, "1.00");

        fl::string s3;
        s3.append(-1.5);
        FL_CHECK_EQ(s3, "-1.50");
    }
}

FL_TEST_CASE("fl::string - append composite types") {
    FL_SUBCASE("vec2<int>") {
        fl::string s;
        fl::vec2<int> pt{3, 4};
        s.append(pt);
        FL_CHECK_EQ(s, "vec2(3,4)");
    }

    FL_SUBCASE("vector<int>") {
        fl::vector<int> v;
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        fl::string s;
        s.append(v);
        FL_CHECK_EQ(s, "[1, 2, 3]");
    }

    FL_SUBCASE("span<int>") {
        int arr[3] = {10, 20, 30};
        fl::span<int> sp(arr, 3);
        fl::string s;
        s.append(sp);
        FL_CHECK_EQ(s, "[10, 20, 30]");
    }

    FL_SUBCASE("optional with value") {
        fl::optional<int> opt(42);
        fl::string s;
        s.append(opt);
        FL_CHECK_EQ(s, "42");
    }

    FL_SUBCASE("optional empty") {
        fl::optional<int> opt;
        fl::string s;
        s.append(opt);
        FL_CHECK_EQ(s, "nullopt");
    }
}

FL_TEST_CASE("fl::string - self-referential append") {
    FL_SUBCASE("append self inline") {
        fl::string s("abc");
        s.append(s);
        FL_CHECK_EQ(s, "abcabc");
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(6));
    }

    FL_SUBCASE("operator+= self inline") {
        fl::string s("abc");
        s += s;
        FL_CHECK_EQ(s, "abcabc");
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(6));
    }

    FL_SUBCASE("triple self append grows through heap") {
        fl::string s("abc");
        s += s;        // "abcabc" (6)
        s += s;        // "abcabcabcabc" (12)
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(12));
        FL_CHECK_EQ(s, "abcabcabcabc");
    }

    FL_SUBCASE("self-append across heap promotion") {
        // Start with 40 chars so doubling crosses 64-byte inline boundary
        fl::string s(40, 'x');
        s.append(s);  // 80 chars, heap-promoted
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(80));
        for (int i = 0; i < 80; ++i) {
            FL_CHECK_EQ(s[i], 'x');
        }
    }
}

FL_TEST_CASE("fl::string - operator+= chains") {
    fl::string s;
    s += "a";
    s += "b";
    s += static_cast<fl::i32>(42);
    fl::vec2<int> pt{1, 2};
    s += pt;
    FL_CHECK_EQ(s, "ab42vec2(1,2)");
}

FL_TEST_CASE("fl::string - operator+ free function") {
    FL_SUBCASE("string + cstr") {
        fl::string r = fl::string("a") + "b";
        FL_CHECK_EQ(r, "ab");
    }

    FL_SUBCASE("cstr + string") {
        fl::string r = "a" + fl::string("b");
        FL_CHECK_EQ(r, "ab");
    }

    FL_SUBCASE("string + string") {
        fl::string r = fl::string("a") + fl::string("b");
        FL_CHECK_EQ(r, "ab");
    }

    FL_SUBCASE("string + integer") {
        fl::string r = fl::string("a") + static_cast<fl::i32>(42);
        FL_CHECK_EQ(r, "a42");
    }

    FL_SUBCASE("integer + string") {
        // operator+(const T& lhs, const string& rhs) via template
        fl::string r = static_cast<fl::i32>(42) + fl::string("a");
        FL_CHECK_EQ(r, "42a");
    }

    FL_SUBCASE("no aliasing: operands unmodified") {
        fl::string a("hello");
        fl::string b(" world");
        fl::string r = a + b;
        FL_CHECK_EQ(r, "hello world");
        FL_CHECK_EQ(a, "hello");
        FL_CHECK_EQ(b, " world");
    }
}

FL_TEST_CASE("fl::string - append empty / null inputs") {
    FL_SUBCASE("append empty cstr") {
        fl::string s("abc");
        s.append("");
        FL_CHECK_EQ(s, "abc");
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(3));
    }

    FL_SUBCASE("append empty string object") {
        fl::string s("abc");
        s.append(fl::string());
        FL_CHECK_EQ(s, "abc");
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(3));
    }

    FL_SUBCASE("append cstr with explicit zero length") {
        fl::string s("abc");
        s.append("ignored", 0);
        FL_CHECK_EQ(s, "abc");
        FL_CHECK_EQ(s.size(), static_cast<fl::size>(3));
    }
}

FL_TEST_CASE("fl::string - exact inline capacity boundary") {
    // FASTLED_STR_INLINED_SIZE == 64; inline buffer holds 64 chars + NUL.
    // Build 63-char string, verify fits inline, then push to 64 (still inline),
    // then 65 (heap-promoted).
    fl::string s(63, 'a');
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(63));
    FL_CHECK_TRUE(s.is_owning());

    s.append('b');  // size == 64, still within inline capacity
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(64));
    FL_CHECK_EQ(s[63], 'b');

    s.append('c');  // size == 65, heap-promoted
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(65));
    FL_CHECK_EQ(s[64], 'c');
    FL_CHECK_TRUE(s.is_owning());
}

FL_TEST_CASE("fl::string - large volume append smoke") {
    // 100 small-string appends; verify final size is correct.
    fl::string s;
    for (int i = 0; i < 100; ++i) {
        s.append("ab");
    }
    FL_CHECK_EQ(s.size(), static_cast<fl::size>(200));
    // Spot-check first and last chars
    FL_CHECK_EQ(s[0], 'a');
    FL_CHECK_EQ(s[199], 'b');
}

// =============================================================================

} // FL_TEST_FILE
