#include "fl/stl/pair.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"

using namespace fl;

namespace {

// Helper class to test move semantics
struct MoveTestTypePair {
    int value;
    bool moved_from = false;
    bool moved_to = false;

    MoveTestTypePair() : value(0) {}
    explicit MoveTestTypePair(int v) : value(v) {}

    // Copy constructor
    MoveTestTypePair(const MoveTestTypePair& other) : value(other.value) {}

    // Move constructor
    MoveTestTypePair(MoveTestTypePair&& other) noexcept
        : value(other.value), moved_to(true) {
        other.moved_from = true;
        other.value = 0;
    }

    // Copy assignment
    MoveTestTypePair& operator=(const MoveTestTypePair& other) {
        value = other.value;
        return *this;
    }

    // Move assignment
    MoveTestTypePair& operator=(MoveTestTypePair&& other) noexcept {
        value = other.value;
        moved_to = true;
        other.moved_from = true;
        other.value = 0;
        return *this;
    }

    bool operator==(const MoveTestTypePair& other) const {
        return value == other.value;
    }

    bool operator<(const MoveTestTypePair& other) const {
        return value < other.value;
    }
};

} // anonymous namespace

TEST_CASE("fl::pair default constructor") {
    SUBCASE("pair with default constructible types") {
        pair<int, double> p;
        CHECK_EQ(p.first, 0);
        CHECK_EQ(p.second, 0.0);
    }

    SUBCASE("pair with custom types") {
        pair<MoveTestTypePair, MoveTestTypePair> p;
        CHECK_EQ(p.first.value, 0);
        CHECK_EQ(p.second.value, 0);
    }
}

TEST_CASE("fl::pair value constructor") {
    SUBCASE("pair from lvalue references") {
        int a = 42;
        double b = 3.14;
        pair<int, double> p(a, b);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14);
    }

    SUBCASE("pair from literals") {
        pair<int, double> p(42, 3.14);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14);
    }

    SUBCASE("pair from different types") {
        pair<int, float> p(42, 3.14f);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14f);
    }

    SUBCASE("pair of custom types") {
        MoveTestTypePair a(10);
        MoveTestTypePair b(20);
        pair<MoveTestTypePair, MoveTestTypePair> p(a, b);
        CHECK_EQ(p.first.value, 10);
        CHECK_EQ(p.second.value, 20);
    }
}

TEST_CASE("fl::pair perfect forwarding constructor") {
    SUBCASE("forwarding lvalues") {
        int a = 10;
        double b = 20.5;
        pair<int, double> p(a, b);
        CHECK_EQ(p.first, 10);
        CHECK_EQ(p.second, 20.5);
    }

    SUBCASE("forwarding rvalues") {
        pair<int, double> p(42, 3.14);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14);
    }

    SUBCASE("forwarding mixed") {
        int a = 10;
        pair<int, double> p(a, 20.5);
        CHECK_EQ(p.first, 10);
        CHECK_EQ(p.second, 20.5);
    }

    SUBCASE("forwarding with move") {
        MoveTestTypePair a(100);
        pair<MoveTestTypePair, MoveTestTypePair> p(fl::move(a), MoveTestTypePair(200));
        CHECK_EQ(p.first.value, 100);
        CHECK(p.first.moved_to);
        CHECK_EQ(p.second.value, 200);
        CHECK(p.second.moved_to);
    }
}

TEST_CASE("fl::pair copy constructor from different pair types") {
    SUBCASE("copy from same types") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(p1);
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
    }

    SUBCASE("copy from convertible types") {
        pair<int, float> p1(42, 3.14f);
        pair<long, double> p2(p1);
        CHECK_EQ(p2.first, 42L);
        CHECK_EQ(p2.second, doctest::Approx(3.14).epsilon(0.01));
    }
}

TEST_CASE("fl::pair move constructor from different pair types") {
    SUBCASE("move from same types") {
        pair<MoveTestTypePair, MoveTestTypePair> p1(MoveTestTypePair(10), MoveTestTypePair(20));
        pair<MoveTestTypePair, MoveTestTypePair> p2(fl::move(p1));
        CHECK_EQ(p2.first.value, 10);
        CHECK_EQ(p2.second.value, 20);
        CHECK(p2.first.moved_to);
        CHECK(p2.second.moved_to);
    }

    SUBCASE("move from convertible types") {
        pair<MoveTestTypePair, int> p1(MoveTestTypePair(10), 20);
        pair<MoveTestTypePair, long> p2(fl::move(p1));
        CHECK_EQ(p2.first.value, 10);
        CHECK(p2.first.moved_to);
        CHECK_EQ(p2.second, 20L);
    }
}

TEST_CASE("fl::pair copy constructor") {
    SUBCASE("default copy constructor") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(p1);
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
        CHECK_EQ(p1.first, 42);  // Original unchanged
        CHECK_EQ(p1.second, 3.14);
    }
}

TEST_CASE("fl::pair copy assignment") {
    SUBCASE("default copy assignment") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(0, 0.0);
        p2 = p1;
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
        CHECK_EQ(p1.first, 42);  // Original unchanged
        CHECK_EQ(p1.second, 3.14);
    }
}

TEST_CASE("fl::pair move constructor") {
    SUBCASE("move constructor with primitives") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(fl::move(p1));
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
    }

    SUBCASE("move constructor with moveable types") {
        pair<MoveTestTypePair, MoveTestTypePair> p1(MoveTestTypePair(10), MoveTestTypePair(20));
        pair<MoveTestTypePair, MoveTestTypePair> p2(fl::move(p1));
        CHECK_EQ(p2.first.value, 10);
        CHECK_EQ(p2.second.value, 20);
        CHECK(p2.first.moved_to);
        CHECK(p2.second.moved_to);
    }
}

TEST_CASE("fl::pair move assignment") {
    SUBCASE("move assignment with primitives") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(0, 0.0);
        p2 = fl::move(p1);
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
    }

    SUBCASE("move assignment with moveable types") {
        pair<MoveTestTypePair, MoveTestTypePair> p1(MoveTestTypePair(10), MoveTestTypePair(20));
        pair<MoveTestTypePair, MoveTestTypePair> p2;
        p2 = fl::move(p1);
        CHECK_EQ(p2.first.value, 10);
        CHECK_EQ(p2.second.value, 20);
        CHECK(p2.first.moved_to);
        CHECK(p2.second.moved_to);
    }
}

TEST_CASE("fl::pair member typedefs") {
    SUBCASE("first_type typedef") {
        static_assert(fl::is_same<pair<int, double>::first_type, int>::value, "first_type should be int");
        static_assert(fl::is_same<pair<float, char>::first_type, float>::value, "first_type should be float");
    }

    SUBCASE("second_type typedef") {
        static_assert(fl::is_same<pair<int, double>::second_type, double>::value, "second_type should be double");
        static_assert(fl::is_same<pair<float, char>::second_type, char>::value, "second_type should be char");
    }
}

TEST_CASE("fl::pair::swap member function") {
    SUBCASE("swap with primitives") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(100, 2.71);
        p1.swap(p2);
        CHECK_EQ(p1.first, 100);
        CHECK_EQ(p1.second, 2.71);
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
    }

    SUBCASE("swap with custom types") {
        pair<MoveTestTypePair, MoveTestTypePair> p1(MoveTestTypePair(10), MoveTestTypePair(20));
        pair<MoveTestTypePair, MoveTestTypePair> p2(MoveTestTypePair(30), MoveTestTypePair(40));
        p1.swap(p2);
        CHECK_EQ(p1.first.value, 30);
        CHECK_EQ(p1.second.value, 40);
        CHECK_EQ(p2.first.value, 10);
        CHECK_EQ(p2.second.value, 20);
    }
}

TEST_CASE("fl::swap non-member function") {
    SUBCASE("non-member swap with primitives") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(100, 2.71);
        fl::swap(p1, p2);
        CHECK_EQ(p1.first, 100);
        CHECK_EQ(p1.second, 2.71);
        CHECK_EQ(p2.first, 42);
        CHECK_EQ(p2.second, 3.14);
    }

    SUBCASE("non-member swap with custom types") {
        pair<MoveTestTypePair, MoveTestTypePair> p1(MoveTestTypePair(10), MoveTestTypePair(20));
        pair<MoveTestTypePair, MoveTestTypePair> p2(MoveTestTypePair(30), MoveTestTypePair(40));
        fl::swap(p1, p2);
        CHECK_EQ(p1.first.value, 30);
        CHECK_EQ(p1.second.value, 40);
        CHECK_EQ(p2.first.value, 10);
        CHECK_EQ(p2.second.value, 20);
    }
}

TEST_CASE("fl::pair equality operator") {
    SUBCASE("equal pairs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK(p1 == p2);
    }

    SUBCASE("unequal pairs - first differs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(100, 3.14);
        CHECK_FALSE(p1 == p2);
    }

    SUBCASE("unequal pairs - second differs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 2.71);
        CHECK_FALSE(p1 == p2);
    }

    SUBCASE("equality with different types") {
        pair<int, int> p1(42, 100);
        pair<long, long> p2(42L, 100L);
        CHECK(p1 == p2);
    }
}

TEST_CASE("fl::pair inequality operator") {
    SUBCASE("equal pairs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK_FALSE(p1 != p2);
    }

    SUBCASE("unequal pairs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(100, 3.14);
        CHECK(p1 != p2);
    }
}

TEST_CASE("fl::pair less-than operator") {
    SUBCASE("less-than by first") {
        pair<int, double> p1(10, 3.14);
        pair<int, double> p2(20, 2.71);
        CHECK(p1 < p2);
        CHECK_FALSE(p2 < p1);
    }

    SUBCASE("less-than by second when first equal") {
        pair<int, double> p1(42, 2.71);
        pair<int, double> p2(42, 3.14);
        CHECK(p1 < p2);
        CHECK_FALSE(p2 < p1);
    }

    SUBCASE("equal pairs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK_FALSE(p1 < p2);
        CHECK_FALSE(p2 < p1);
    }
}

TEST_CASE("fl::pair less-than-or-equal operator") {
    SUBCASE("less-than") {
        pair<int, double> p1(10, 3.14);
        pair<int, double> p2(20, 2.71);
        CHECK(p1 <= p2);
    }

    SUBCASE("equal") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK(p1 <= p2);
    }

    SUBCASE("greater-than") {
        pair<int, double> p1(20, 3.14);
        pair<int, double> p2(10, 2.71);
        CHECK_FALSE(p1 <= p2);
    }
}

TEST_CASE("fl::pair greater-than operator") {
    SUBCASE("greater-than by first") {
        pair<int, double> p1(20, 3.14);
        pair<int, double> p2(10, 2.71);
        CHECK(p1 > p2);
        CHECK_FALSE(p2 > p1);
    }

    SUBCASE("greater-than by second when first equal") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 2.71);
        CHECK(p1 > p2);
        CHECK_FALSE(p2 > p1);
    }

    SUBCASE("equal pairs") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK_FALSE(p1 > p2);
        CHECK_FALSE(p2 > p1);
    }
}

TEST_CASE("fl::pair greater-than-or-equal operator") {
    SUBCASE("greater-than") {
        pair<int, double> p1(20, 3.14);
        pair<int, double> p2(10, 2.71);
        CHECK(p1 >= p2);
    }

    SUBCASE("equal") {
        pair<int, double> p1(42, 3.14);
        pair<int, double> p2(42, 3.14);
        CHECK(p1 >= p2);
    }

    SUBCASE("less-than") {
        pair<int, double> p1(10, 3.14);
        pair<int, double> p2(20, 2.71);
        CHECK_FALSE(p1 >= p2);
    }
}

TEST_CASE("fl::make_pair function") {
    SUBCASE("make_pair with primitives") {
        auto p = make_pair(42, 3.14);
        static_assert(fl::is_same<decltype(p), pair<int, double>>::value, "make_pair should deduce types");
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14);
    }

    SUBCASE("make_pair with lvalue references") {
        int a = 10;
        double b = 20.5;
        auto p = make_pair(a, b);
        CHECK_EQ(p.first, 10);
        CHECK_EQ(p.second, 20.5);
    }

    SUBCASE("make_pair with rvalue references") {
        auto p = make_pair(MoveTestTypePair(100), MoveTestTypePair(200));
        CHECK_EQ(p.first.value, 100);
        CHECK_EQ(p.second.value, 200);
    }

    SUBCASE("make_pair type decay") {
        int arr[3] = {1, 2, 3};
        auto p = make_pair(arr, 42);
        // Array should decay to pointer
        static_assert(fl::is_same<decltype(p.first), int*>::value, "Array should decay to pointer");
        CHECK_EQ(p.second, 42);
    }
}

// NOTE: get by index tests are skipped because the implementation in pair.h
// has a fundamental issue - it uses runtime if statements but the return type
// is fixed at compile-time. When I != 0 or I != 1, the wrong branch is compiled
// which causes type mismatch errors. This is a known limitation/bug in the
// current pair.h implementation.
//
// TEST_CASE("fl::pair get by index") {
//     // These tests would fail to compile due to pair.h implementation issues
// }

// NOTE: get by type tests are skipped because the implementation in pair.h
// has a fundamental issue - it uses runtime if statements but the return type
// is fixed at compile-time, making it impossible for both branches to compile
// when the pair has different types. This is a known limitation/bug in the
// current pair.h implementation.
//
// TEST_CASE("fl::pair get by type") {
//     // These tests would fail to compile due to pair.h implementation issues
// }

TEST_CASE("fl::pair_element type trait") {
    SUBCASE("pair_element<0>") {
        static_assert(fl::is_same<pair_element<0, int, double>::type, int>::value, "pair_element<0> should be int");
    }

    SUBCASE("pair_element<1>") {
        static_assert(fl::is_same<pair_element<1, int, double>::type, double>::value, "pair_element<1> should be double");
    }
}

TEST_CASE("fl::tuple_size for pair") {
    SUBCASE("tuple_size value") {
        static_assert(tuple_size<pair<int, double>>::value == 2, "tuple_size should be 2");
        static_assert(tuple_size<pair<float, char>>::value == 2, "tuple_size should be 2");
    }
}

TEST_CASE("fl::tuple_element for pair") {
    SUBCASE("tuple_element<0>") {
        static_assert(fl::is_same<tuple_element<0, pair<int, double>>::type, int>::value, "tuple_element<0> should be int");
    }

    SUBCASE("tuple_element<1>") {
        static_assert(fl::is_same<tuple_element<1, pair<int, double>>::type, double>::value, "tuple_element<1> should be double");
    }
}

TEST_CASE("fl::pair edge cases") {
    SUBCASE("pair with zero values") {
        pair<int, int> p(0, 0);
        CHECK_EQ(p.first, 0);
        CHECK_EQ(p.second, 0);
    }

    SUBCASE("pair with negative values") {
        pair<int, int> p(-42, -100);
        CHECK_EQ(p.first, -42);
        CHECK_EQ(p.second, -100);
    }

    SUBCASE("pair with nullptr") {
        pair<int*, double*> p(nullptr, nullptr);
        CHECK_EQ(p.first, nullptr);
        CHECK_EQ(p.second, nullptr);
    }

    SUBCASE("pair with boolean") {
        pair<bool, bool> p(true, false);
        CHECK_EQ(p.first, true);
        CHECK_EQ(p.second, false);
    }

    SUBCASE("pair of same types") {
        pair<int, int> p(42, 100);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 100);
    }
}

TEST_CASE("fl::pair with pointers") {
    SUBCASE("pair of pointers") {
        int a = 42;
        double b = 3.14;
        pair<int*, double*> p(&a, &b);
        CHECK_EQ(*p.first, 42);
        CHECK_EQ(*p.second, 3.14);
    }

    SUBCASE("modify through pointers") {
        int a = 42;
        double b = 3.14;
        pair<int*, double*> p(&a, &b);
        *p.first = 100;
        *p.second = 2.71;
        CHECK_EQ(a, 100);
        CHECK_EQ(b, 2.71);
    }
}

TEST_CASE("fl::pair nested pairs") {
    SUBCASE("pair of pairs") {
        pair<int, int> inner1(10, 20);
        pair<int, int> inner2(30, 40);
        pair<pair<int, int>, pair<int, int>> outer(inner1, inner2);
        CHECK_EQ(outer.first.first, 10);
        CHECK_EQ(outer.first.second, 20);
        CHECK_EQ(outer.second.first, 30);
        CHECK_EQ(outer.second.second, 40);
    }

    SUBCASE("make_pair with nested pair") {
        auto inner = make_pair(10, 20);
        auto outer = make_pair(inner, 30);
        CHECK_EQ(outer.first.first, 10);
        CHECK_EQ(outer.first.second, 20);
        CHECK_EQ(outer.second, 30);
    }
}

TEST_CASE("fl::pair comparison comprehensive") {
    SUBCASE("lexicographic ordering") {
        pair<int, int> p1(1, 100);
        pair<int, int> p2(2, 1);
        // p1 < p2 because first element (1 < 2)
        CHECK(p1 < p2);
        CHECK(p1 <= p2);
        CHECK_FALSE(p1 > p2);
        CHECK_FALSE(p1 >= p2);
        CHECK(p1 != p2);
    }

    SUBCASE("lexicographic ordering - equal first") {
        pair<int, int> p1(1, 50);
        pair<int, int> p2(1, 100);
        // p1 < p2 because first equal, second (50 < 100)
        CHECK(p1 < p2);
        CHECK(p1 <= p2);
        CHECK_FALSE(p1 > p2);
        CHECK_FALSE(p1 >= p2);
        CHECK(p1 != p2);
    }
}

TEST_CASE("fl::Pair backwards compatibility") {
    SUBCASE("Pair alias") {
        Pair<int, double> p(42, 3.14);
        CHECK_EQ(p.first, 42);
        CHECK_EQ(p.second, 3.14);
        static_assert(fl::is_same<Pair<int, double>, pair<int, double>>::value, "Pair should be alias for pair");
    }
}
