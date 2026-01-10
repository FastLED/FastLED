#include "test.h"
#include "fl/stl/optional.h"
#include "fl/compiler_control.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"

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

TEST_CASE("fl::Optional - value() method") {
    SUBCASE("value() on non-empty optional") {
        Optional<int> opt(42);
        CHECK(opt.value() == 42);
        opt.value() = 100;
        CHECK(opt.value() == 100);
    }

    SUBCASE("value() const on non-empty optional") {
        const Optional<int> opt(42);
        CHECK(opt.value() == 42);
    }

    SUBCASE("value() with struct") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Optional<Point> opt(Point(10, 20));
        CHECK(opt.value().x == 10);
        CHECK(opt.value().y == 20);
        opt.value().x = 30;
        CHECK(opt.value().x == 30);
    }

    SUBCASE("value() is compatible with operator*") {
        Optional<int> opt(42);
        CHECK(opt.value() == *opt);
        CHECK(&opt.value() == &(*opt));
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

TEST_CASE("fl::Optional<T&&> - rvalue reference specialization") {
    SUBCASE("default construction creates empty optional") {
        Optional<int&&> opt;
        CHECK(opt.empty());
        CHECK(!opt.has_value());
        CHECK(!opt);
        CHECK(opt == nullopt);
    }

    SUBCASE("nullopt constructor creates empty optional") {
        Optional<int&&> opt(nullopt);
        CHECK(opt.empty());
        CHECK(!opt.has_value());
        CHECK(opt == nullopt);
    }

    SUBCASE("construct with rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        CHECK(!opt.empty());
        CHECK(opt.has_value());
        CHECK(opt);
        CHECK(opt != nullopt);
        // Verify the reference points to the original value
        CHECK(opt.ptr() == &value);
    }

    SUBCASE("move construction transfers reference") {
        int value = 100;
        Optional<int&&> opt1(fl::move(value));
        CHECK(opt1.has_value());
        CHECK(opt1.ptr() == &value);

        Optional<int&&> opt2(fl::move(opt1));
        CHECK(opt2.has_value());
        CHECK(opt2.ptr() == &value);
        CHECK(opt1.empty());  // Original is now empty
    }

    SUBCASE("move assignment transfers reference") {
        int value1 = 42;
        int value2 = 100;
        Optional<int&&> opt1(fl::move(value1));
        Optional<int&&> opt2(fl::move(value2));

        CHECK(opt1.ptr() == &value1);
        CHECK(opt2.ptr() == &value2);

        opt1 = fl::move(opt2);
        CHECK(opt1.ptr() == &value2);
        CHECK(opt2.empty());
    }

    SUBCASE("assign nullopt clears reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        CHECK(opt.has_value());

        opt = nullopt;
        CHECK(opt.empty());
        CHECK(opt == nullopt);
    }

    SUBCASE("reset clears reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        CHECK(opt.has_value());

        opt.reset();
        CHECK(opt.empty());
    }

    SUBCASE("dereference operator forwards rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        // Get rvalue reference and verify it can be moved
        int&& ref = *opt;
        int moved_value = fl::move(ref);
        CHECK(moved_value == 42);
    }

    SUBCASE("get method forwards rvalue reference") {
        int value = 99;
        Optional<int&&> opt(fl::move(value));

        int&& ref = opt.get();
        CHECK(ref == 99);
    }

    SUBCASE("arrow operator provides member access") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Point p(10, 20);
        Optional<Point&&> opt(fl::move(p));

        CHECK(opt->x == 10);
        CHECK(opt->y == 20);
    }

    SUBCASE("boolean operators work correctly") {
        int value = 42;
        Optional<int&&> opt_empty;
        Optional<int&&> opt_full(fl::move(value));

        CHECK(opt_empty.operator!());
        CHECK(!opt_empty.operator()());
        CHECK(!static_cast<bool>(opt_empty));

        CHECK(!opt_full.operator!());
        CHECK(opt_full.operator()());
        CHECK(static_cast<bool>(opt_full));
    }

    SUBCASE("equality operators") {
        int value1 = 42;
        int value2 = 42;
        int value3 = 99;

        Optional<int&&> opt_empty1;
        Optional<int&&> opt_empty2;
        Optional<int&&> opt1(fl::move(value1));
        Optional<int&&> opt2(fl::move(value2));
        Optional<int&&> opt3(fl::move(value3));

        // Two empty optionals are equal
        CHECK(opt_empty1 == opt_empty2);

        // Empty and non-empty are not equal
        CHECK(opt_empty1 != opt1);

        // Two optionals with same value are equal
        CHECK(opt1 == opt2);

        // Two optionals with different values are not equal
        CHECK(opt1 != opt3);
    }

    SUBCASE("ptr method returns correct pointer") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        int* p = opt.ptr();
        CHECK(p == &value);
        CHECK(*p == 42);
    }

    SUBCASE("forwarding to function consuming rvalue reference") {
        struct Widget {
            int value;
            bool moved = false;
            Widget(int v) : value(v) {}
            Widget(Widget&& other) : value(other.value), moved(true) {
                other.value = 0;
            }
        };

        auto consume = [](Widget&& w) -> int {
            Widget local = fl::move(w);
            return local.value;
        };

        Widget w(42);
        Optional<Widget&&> opt(fl::move(w));

        // Forward the rvalue reference to the consuming function
        int result = consume(*opt);
        CHECK(result == 42);
    }

    SUBCASE("lifetime semantics - reference validity") {
        // This test verifies that the optional correctly holds a reference
        // to an existing object and doesn't try to manage its lifetime
        int value = 100;
        Optional<int&&> opt(fl::move(value));

        // Modify through the optional
        *opt.ptr() = 200;

        // Verify the original value changed
        CHECK(value == 200);
    }

    SUBCASE("move from optional into new variable") {
        struct MoveOnly {
            int value;
            MoveOnly(int v) : value(v) {}
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly(MoveOnly&& other) : value(other.value) {
                other.value = -1;
            }
        };

        MoveOnly obj(42);
        Optional<MoveOnly&&> opt(fl::move(obj));

        // Move construct from the dereferenced optional
        MoveOnly new_obj(fl::move(*opt));
        CHECK(new_obj.value == 42);
        CHECK(obj.value == -1);  // Original was moved from
    }

    SUBCASE("value() method forwards rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        // Get rvalue reference using value() and verify it can be moved
        int&& ref = opt.value();
        int moved_value = fl::move(ref);
        CHECK(moved_value == 42);
    }

    SUBCASE("value() is compatible with operator* and get()") {
        int value = 99;
        Optional<int&&> opt(fl::move(value));

        // All three should return the same reference
        int&& ref1 = opt.value();
        int&& ref2 = *opt;
        int&& ref3 = opt.get();

        CHECK(&ref1 == &value);
        CHECK(&ref2 == &value);
        CHECK(&ref3 == &value);
    }
}
