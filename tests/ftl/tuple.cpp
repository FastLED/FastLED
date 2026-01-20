#include "test.h"
#include "fl/stl/tuple.h"
#include "fl/stl/string.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"

using namespace fl;

TEST_CASE("fl::tuple - empty tuple") {
    tuple<> t;
    (void)t; // Suppress unused variable warning
    CHECK_EQ(0u, tuple_size<tuple<>>::value);
}

TEST_CASE("fl::tuple - basic construction") {
    SUBCASE("single element") {
        tuple<int> t(42);
        CHECK_EQ(42, t.head);
    }

    SUBCASE("two elements") {
        tuple<int, float> t(42, 3.14f);
        CHECK_EQ(42, t.head);
        CHECK_EQ(3.14f, t.tail.head);
    }

    SUBCASE("three elements") {
        tuple<int, float, double> t(42, 3.14f, 2.718);
        CHECK_EQ(42, get<0>(t));
        CHECK_EQ(3.14f, get<1>(t));
        CHECK_EQ(2.718, get<2>(t));
    }

    SUBCASE("default construction") {
        tuple<int, float> t;
        (void)t; // Suppress unused variable warning
        // Default constructed, values are uninitialized
        // Just verify it compiles
    }
}

TEST_CASE("fl::tuple - make_tuple") {
    SUBCASE("empty tuple") {
        auto t = make_tuple();
        CHECK_EQ(0u, tuple_size<decltype(t)>::value);
    }

    SUBCASE("single element") {
        auto t = make_tuple(42);
        CHECK_EQ(42, get<0>(t));
    }

    SUBCASE("multiple types") {
        auto t = make_tuple(42, "hello", 3.14f);
        CHECK_EQ(42, get<0>(t));
        CHECK_EQ(string("hello"), string(get<1>(t)));
        CHECK_CLOSE(3.14f, get<2>(t), 0.0001f);
    }

    SUBCASE("type decay") {
        const int x = 42;
        int& ref = const_cast<int&>(x);
        auto t = make_tuple(ref);
        // make_tuple should decay references to values
        CHECK((is_same<tuple_element<0, decltype(t)>::type, int>::value));
    }
}

TEST_CASE("fl::tuple - get by index") {
    SUBCASE("lvalue reference") {
        tuple<int, float, double> t(1, 2.0f, 3.0);
        CHECK_EQ(1, get<0>(t));
        CHECK_EQ(2.0f, get<1>(t));
        CHECK_EQ(3.0, get<2>(t));

        // Modify through reference
        get<0>(t) = 42;
        CHECK_EQ(42, get<0>(t));
    }

    SUBCASE("const lvalue reference") {
        const tuple<int, float, double> t(1, 2.0f, 3.0);
        CHECK_EQ(1, get<0>(t));
        CHECK_EQ(2.0f, get<1>(t));
        CHECK_EQ(3.0, get<2>(t));
    }

    SUBCASE("rvalue reference") {
        auto t = make_tuple(1, 2.0f, 3.0);
        auto value = get<0>(move(t));
        CHECK_EQ(1, value);
    }
}

TEST_CASE("fl::tuple - tuple_size") {
    SUBCASE("empty") {
        CHECK_EQ(0u, tuple_size<tuple<>>::value);
    }

    SUBCASE("one element") {
        CHECK_EQ(1u, tuple_size<tuple<int>>::value);
    }

    SUBCASE("multiple elements") {
        CHECK_EQ(2u, tuple_size<tuple<int, float>>::value);
        CHECK_EQ(3u, tuple_size<tuple<int, float, double>>::value);
        CHECK_EQ(5u, tuple_size<tuple<int, float, double, char, bool>>::value);
    }

    SUBCASE("with make_tuple") {
        auto t1 = make_tuple(1, 2, 3);
        auto t2 = make_tuple();
        auto t3 = make_tuple(1, "test");

        CHECK_EQ(3u, tuple_size<decltype(t1)>::value);
        CHECK_EQ(0u, tuple_size<decltype(t2)>::value);
        CHECK_EQ(2u, tuple_size<decltype(t3)>::value);
    }
}

TEST_CASE("fl::tuple - tuple_element") {
    using tuple_type = tuple<int, float, double, char>;

    SUBCASE("type extraction") {
        CHECK((is_same<tuple_element<0, tuple_type>::type, int>::value));
        CHECK((is_same<tuple_element<1, tuple_type>::type, float>::value));
        CHECK((is_same<tuple_element<2, tuple_type>::type, double>::value));
        CHECK((is_same<tuple_element<3, tuple_type>::type, char>::value));
    }

    SUBCASE("with string") {
        using tuple_with_string = tuple<int, string, float>;
        CHECK((is_same<tuple_element<0, tuple_with_string>::type, int>::value));
        CHECK((is_same<tuple_element<1, tuple_with_string>::type, string>::value));
        CHECK((is_same<tuple_element<2, tuple_with_string>::type, float>::value));
    }
}

TEST_CASE("fl::tuple - move semantics") {
    SUBCASE("move construction") {
        auto t1 = make_tuple(42, string("test"));
        auto t2 = move(t1);

        CHECK_EQ(42, get<0>(t2));
        CHECK_EQ(string("test"), get<1>(t2));
    }

    SUBCASE("get with move") {
        auto t = make_tuple(string("hello"), string("world"));
        auto s1 = get<0>(move(t));
        CHECK_EQ(string("hello"), s1);
    }
}

TEST_CASE("fl::tuple - copy semantics") {
    SUBCASE("copy construction") {
        tuple<int, float> t1(42, 3.14f);
        tuple<int, float> t2(t1);

        CHECK_EQ(42, get<0>(t2));
        CHECK_EQ(3.14f, get<1>(t2));
    }

    SUBCASE("copy with string") {
        auto t1 = make_tuple(42, string("test"), 3.14);
        auto t2 = t1;

        CHECK_EQ(42, get<0>(t2));
        CHECK_EQ(string("test"), get<1>(t2));
        CHECK_EQ(3.14, get<2>(t2));
    }
}

TEST_CASE("fl::tuple - nested tuples") {
    SUBCASE("tuple of tuples") {
        auto inner1 = make_tuple(1, 2);
        auto inner2 = make_tuple(3, 4);
        auto outer = make_tuple(inner1, inner2);

        CHECK_EQ(1, get<0>(get<0>(outer)));
        CHECK_EQ(2, get<1>(get<0>(outer)));
        CHECK_EQ(3, get<0>(get<1>(outer)));
        CHECK_EQ(4, get<1>(get<1>(outer)));
    }
}

TEST_CASE("fl::tuple - various types") {
    SUBCASE("numeric types") {
        auto t = make_tuple(
            static_cast<int8_t>(1),
            static_cast<uint16_t>(2),
            static_cast<int32_t>(3),
            static_cast<uint64_t>(4)
        );

        CHECK_EQ(1, get<0>(t));
        CHECK_EQ(2, get<1>(t));
        CHECK_EQ(3, get<2>(t));
        CHECK_EQ(4u, get<3>(t));
    }

    SUBCASE("mixed numeric types") {
        auto t = make_tuple(42, 3.14f, 2.718, 'a', true);

        CHECK_EQ(42, get<0>(t));
        CHECK_CLOSE(3.14f, get<1>(t), 0.0001f);
        CHECK_CLOSE(2.718, get<2>(t), 0.0001);
        CHECK_EQ('a', get<3>(t));
        CHECK_EQ(true, get<4>(t));
    }

    SUBCASE("pointers") {
        int x = 42;
        float y = 3.14f;
        auto t = make_tuple(&x, &y);

        CHECK_EQ(&x, get<0>(t));
        CHECK_EQ(&y, get<1>(t));
        CHECK_EQ(42, *get<0>(t));
        CHECK_CLOSE(3.14f, *get<1>(t), 0.0001f);
    }
}

TEST_CASE("fl::tuple - constexpr operations") {
    SUBCASE("tuple_size is constexpr") {
        constexpr size_t size = tuple_size<tuple<int, float, double>>::value;
        CHECK_EQ(3u, size);
    }

    SUBCASE("tuple_size operator()") {
        tuple_size<tuple<int, float>> ts;
        CHECK_EQ(2u, ts());
    }

    SUBCASE("tuple_size conversion operator") {
        tuple_size<tuple<int, float, double>> ts;
        size_t size = ts;
        CHECK_EQ(3u, size);
    }
}

TEST_CASE("fl::tuple - edge cases") {
    SUBCASE("single element tuple") {
        auto t = make_tuple(42);
        CHECK_EQ(42, get<0>(t));
        CHECK_EQ(1u, tuple_size<decltype(t)>::value);
    }

    SUBCASE("large tuple") {
        auto t = make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        CHECK_EQ(1, get<0>(t));
        CHECK_EQ(5, get<4>(t));
        CHECK_EQ(10, get<9>(t));
        CHECK_EQ(10u, tuple_size<decltype(t)>::value);
    }

    SUBCASE("tuple with same types") {
        auto t = make_tuple(1, 2, 3, 4, 5);
        CHECK_EQ(1, get<0>(t));
        CHECK_EQ(2, get<1>(t));
        CHECK_EQ(3, get<2>(t));
        CHECK_EQ(4, get<3>(t));
        CHECK_EQ(5, get<4>(t));
    }
}
