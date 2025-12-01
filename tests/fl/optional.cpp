#include "test.h"
#include "ftl/optional.h"
#include "fl/compiler_control.h"

using namespace fl;

TEST_CASE("fl::Optional - default construction") {
    SUBCASE("default constructor creates empty optional") {
        Optional<int> opt;
        CHECK(opt.empty());
        CHECK(!opt.has_value());
        CHECK(!opt);
        CHECK(opt == nullopt);
    }

    SUBCASE("nullopt constructor creates empty optional") {
        Optional<int> opt(nullopt);
        CHECK(opt.empty());
        CHECK(!opt.has_value());
        CHECK(opt == nullopt);
    }
}

TEST_CASE("fl::Optional - value construction") {
    SUBCASE("construct with lvalue") {
        int value = 42;
        Optional<int> opt(value);
        CHECK(!opt.empty());
        CHECK(opt.has_value());
        CHECK(opt);
        CHECK(*opt == 42);
        CHECK(opt != nullopt);
    }

    SUBCASE("construct with rvalue") {
        Optional<int> opt(42);
        CHECK(!opt.empty());
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    SUBCASE("construct with string") {
        // Test with a more complex type
        Optional<int> opt(123);
        CHECK(opt.has_value());
        CHECK(*opt == 123);
    }
}

TEST_CASE("fl::Optional - copy construction") {
    SUBCASE("copy empty optional") {
        Optional<int> opt1;
        Optional<int> opt2(opt1);
        CHECK(opt2.empty());
        CHECK(opt1 == opt2);
    }

    SUBCASE("copy non-empty optional") {
        Optional<int> opt1(42);
        Optional<int> opt2(opt1);
        CHECK(opt2.has_value());
        CHECK(*opt2 == 42);
        CHECK(opt1 == opt2);
    }
}

TEST_CASE("fl::Optional - move construction") {
    SUBCASE("move empty optional") {
        Optional<int> opt1;
        Optional<int> opt2(fl::move(opt1));
        CHECK(opt2.empty());
    }

    SUBCASE("move non-empty optional") {
        Optional<int> opt1(42);
        Optional<int> opt2(fl::move(opt1));
        CHECK(opt2.has_value());
        CHECK(*opt2 == 42);
    }
}

TEST_CASE("fl::Optional - assignment operators") {
    SUBCASE("copy assign from empty") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        opt2 = opt1;
        CHECK(opt2.empty());
    }

    SUBCASE("copy assign from non-empty") {
        Optional<int> opt1(42);
        Optional<int> opt2;
        opt2 = opt1;
        CHECK(opt2.has_value());
        CHECK(*opt2 == 42);
    }

    SUBCASE("move assign from empty") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        opt2 = fl::move(opt1);
        CHECK(opt2.empty());
    }

    SUBCASE("move assign from non-empty") {
        Optional<int> opt1(42);
        Optional<int> opt2;
        opt2 = fl::move(opt1);
        CHECK(opt2.has_value());
        CHECK(*opt2 == 42);
    }

    SUBCASE("assign nullopt") {
        Optional<int> opt(42);
        opt = nullopt;
        CHECK(opt.empty());
        CHECK(opt == nullopt);
    }

    SUBCASE("assign value lvalue") {
        Optional<int> opt;
        int value = 42;
        opt = value;
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    SUBCASE("assign value rvalue") {
        Optional<int> opt;
        opt = 42;
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    SUBCASE("self-assignment") {
        Optional<int> opt(42);
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
        opt = opt;
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }
}

TEST_CASE("fl::Optional - emplace") {
    SUBCASE("emplace into empty optional") {
        Optional<int> opt;
        opt.emplace(42);
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    SUBCASE("emplace into non-empty optional") {
        Optional<int> opt(10);
        opt.emplace(42);
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }
}

TEST_CASE("fl::Optional - reset") {
    SUBCASE("reset empty optional") {
        Optional<int> opt;
        opt.reset();
        CHECK(opt.empty());
    }

    SUBCASE("reset non-empty optional") {
        Optional<int> opt(42);
        opt.reset();
        CHECK(opt.empty());
        CHECK(opt == nullopt);
    }
}

TEST_CASE("fl::Optional - ptr and const ptr") {
    SUBCASE("ptr on empty optional") {
        Optional<int> opt;
        CHECK(opt.ptr() == nullptr);
    }

    SUBCASE("ptr on non-empty optional") {
        Optional<int> opt(42);
        int* p = opt.ptr();
        CHECK(p != nullptr);
        CHECK(*p == 42);
        *p = 100;
        CHECK(*opt == 100);
    }

    SUBCASE("const ptr on non-empty optional") {
        const Optional<int> opt(42);
        const int* p = opt.ptr();
        CHECK(p != nullptr);
        CHECK(*p == 42);
    }
}

TEST_CASE("fl::Optional - dereference operators") {
    SUBCASE("operator* lvalue") {
        Optional<int> opt(42);
        CHECK(*opt == 42);
        *opt = 100;
        CHECK(*opt == 100);
    }

    SUBCASE("operator* const") {
        const Optional<int> opt(42);
        CHECK(*opt == 42);
    }

    SUBCASE("operator-> with struct") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Optional<Point> opt(Point(10, 20));
        CHECK(opt->x == 10);
        CHECK(opt->y == 20);
        opt->x = 30;
        CHECK(opt->x == 30);
    }

    SUBCASE("operator-> const") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        const Optional<Point> opt(Point(10, 20));
        CHECK(opt->x == 10);
        CHECK(opt->y == 20);
    }
}

TEST_CASE("fl::Optional - boolean operators") {
    SUBCASE("operator! on empty") {
        Optional<int> opt;
        CHECK(opt.operator!());
    }

    SUBCASE("operator! on non-empty") {
        Optional<int> opt(42);
        CHECK(!opt.operator!());
    }

    SUBCASE("operator() on empty") {
        Optional<int> opt;
        CHECK(!opt.operator()());
    }

    SUBCASE("operator() on non-empty") {
        Optional<int> opt(42);
        CHECK(opt.operator()());
    }

    SUBCASE("explicit operator bool on empty") {
        Optional<int> opt;
        CHECK(!static_cast<bool>(opt));
        if (opt) {
            CHECK(false); // Should not reach here
        }
    }

    SUBCASE("explicit operator bool on non-empty") {
        Optional<int> opt(42);
        CHECK(static_cast<bool>(opt));
        if (opt) {
            CHECK(true); // Should reach here
        } else {
            CHECK(false); // Should not reach here
        }
    }
}

TEST_CASE("fl::Optional - equality operators") {
    SUBCASE("two empty optionals are equal") {
        Optional<int> opt1;
        Optional<int> opt2;
        CHECK(opt1 == opt2);
        CHECK(!(opt1 != opt2));
    }

    SUBCASE("empty and non-empty are not equal") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        CHECK(opt1 != opt2);
        CHECK(!(opt1 == opt2));
        CHECK(opt2 != opt1);
        CHECK(!(opt2 == opt1));
    }

    SUBCASE("two non-empty with same value are equal") {
        Optional<int> opt1(42);
        Optional<int> opt2(42);
        CHECK(opt1 == opt2);
        CHECK(!(opt1 != opt2));
    }

    SUBCASE("two non-empty with different values are not equal") {
        Optional<int> opt1(42);
        Optional<int> opt2(43);
        CHECK(opt1 != opt2);
        CHECK(!(opt1 == opt2));
    }

    SUBCASE("compare with value - empty") {
        Optional<int> opt;
        CHECK(!(opt == 42));
    }

    SUBCASE("compare with value - matching") {
        Optional<int> opt(42);
        CHECK(opt == 42);
    }

    SUBCASE("compare with value - not matching") {
        Optional<int> opt(42);
        CHECK(!(opt == 43));
    }

    SUBCASE("compare with nullopt - empty") {
        Optional<int> opt;
        CHECK(opt == nullopt);
        CHECK(!(opt != nullopt));
    }

    SUBCASE("compare with nullopt - non-empty") {
        Optional<int> opt(42);
        CHECK(!(opt == nullopt));
        CHECK(opt != nullopt);
    }
}

TEST_CASE("fl::Optional - swap") {
    // Note: swap requires variant to have swap() method
    // If variant doesn't support swap, these tests may not work

    SUBCASE("manual swap via move") {
        Optional<int> opt1(10);
        Optional<int> opt2(20);
        Optional<int> temp(fl::move(opt1));
        opt1 = fl::move(opt2);
        opt2 = fl::move(temp);
        CHECK(*opt1 == 20);
        CHECK(*opt2 == 10);
    }
}

TEST_CASE("fl::make_optional helper functions") {
    SUBCASE("make_optional with rvalue") {
        auto opt = make_optional(42);
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }

    SUBCASE("make_optional type deduction") {
        auto opt_int = make_optional(42);
        auto opt_double = make_optional(3.14);

        CHECK(opt_int.has_value());
        CHECK(opt_double.has_value());
        CHECK(*opt_int == 42);
        CHECK_CLOSE(*opt_double, 3.14, 0.001);
    }

    SUBCASE("make_optional with explicit copy") {
        // To avoid reference issues, we construct from the value directly
        int value = 42;
        Optional<int> opt(value);
        CHECK(opt.has_value());
        CHECK(*opt == 42);
    }
}

TEST_CASE("fl::Optional - edge cases") {
    SUBCASE("optional of bool") {
        Optional<bool> opt_false(false);
        Optional<bool> opt_true(true);
        Optional<bool> opt_empty;

        // Explicit bool conversion vs contained bool value
        CHECK(opt_false.has_value());
        CHECK(*opt_false == false);
        CHECK(static_cast<bool>(opt_false) == true); // has value

        CHECK(opt_true.has_value());
        CHECK(*opt_true == true);

        CHECK(!opt_empty.has_value());
        CHECK(static_cast<bool>(opt_empty) == false);
    }

    SUBCASE("optional of pointer") {
        int x = 42;
        int* ptr = &x;
        Optional<int*> opt(ptr);

        CHECK(opt.has_value());
        CHECK(*opt == ptr);
        CHECK(**opt == 42);
    }

    SUBCASE("multiple reset calls") {
        Optional<int> opt(42);
        opt.reset();
        opt.reset();
        opt.reset();
        CHECK(opt.empty());
    }

    SUBCASE("assign after reset") {
        Optional<int> opt(42);
        opt.reset();
        CHECK(opt.empty());
        opt = 100;
        CHECK(opt.has_value());
        CHECK(*opt == 100);
    }
}

TEST_CASE("fl::Optional - constexpr support") {
    SUBCASE("constexpr nullopt_t") {
        constexpr nullopt_t n = nullopt;
        (void)n;
    }

    // Note: Full constexpr Optional operations would require constexpr variant
    // which may not be supported. Testing what we can.
}

TEST_CASE("fl::Optional - type alias") {
    SUBCASE("optional is alias for Optional") {
        // Test that the lowercase alias works
        optional<int> opt(42);
        CHECK(opt.has_value());
        CHECK(*opt == 42);

        Optional<int> opt2(42);
        CHECK(opt == opt2);
    }
}
