#include "test.h"
#include "fl/stl/optional.h"
#include "fl/compiler_control.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/move.h"
#include "fl/stl/string.h"

using namespace fl;

FL_TEST_CASE("fl::Optional - default construction") {
    FL_SUBCASE("default constructor creates empty optional") {
        Optional<int> opt;
        FL_CHECK(opt.empty());
        FL_CHECK(!opt.has_value());
        FL_CHECK(!opt);
        FL_CHECK(opt == nullopt);
    }

    FL_SUBCASE("nullopt constructor creates empty optional") {
        Optional<int> opt(nullopt);
        FL_CHECK(opt.empty());
        FL_CHECK(!opt.has_value());
        FL_CHECK(opt == nullopt);
    }
}

FL_TEST_CASE("fl::Optional - value construction") {
    FL_SUBCASE("construct with lvalue") {
        int value = 42;
        Optional<int> opt(value);
        FL_CHECK(!opt.empty());
        FL_CHECK(opt.has_value());
        FL_CHECK(opt);
        FL_CHECK(*opt == 42);
        FL_CHECK(opt != nullopt);
    }

    FL_SUBCASE("construct with rvalue") {
        Optional<int> opt(42);
        FL_CHECK(!opt.empty());
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("construct with string") {
        // Test with a more complex type
        Optional<int> opt(123);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 123);
    }
}

FL_TEST_CASE("fl::Optional - copy construction") {
    FL_SUBCASE("copy empty optional") {
        Optional<int> opt1;
        Optional<int> opt2(opt1);
        FL_CHECK(opt2.empty());
        FL_CHECK(opt1 == opt2);
    }

    FL_SUBCASE("copy non-empty optional") {
        Optional<int> opt1(42);
        Optional<int> opt2(opt1);
        FL_CHECK(opt2.has_value());
        FL_CHECK(*opt2 == 42);
        FL_CHECK(opt1 == opt2);
    }
}

FL_TEST_CASE("fl::Optional - move construction") {
    FL_SUBCASE("move empty optional") {
        Optional<int> opt1;
        Optional<int> opt2(fl::move(opt1));
        FL_CHECK(opt2.empty());
    }

    FL_SUBCASE("move non-empty optional") {
        Optional<int> opt1(42);
        Optional<int> opt2(fl::move(opt1));
        FL_CHECK(opt2.has_value());
        FL_CHECK(*opt2 == 42);
    }
}

FL_TEST_CASE("fl::Optional - assignment operators") {
    FL_SUBCASE("copy assign from empty") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        opt2 = opt1;
        FL_CHECK(opt2.empty());
    }

    FL_SUBCASE("copy assign from non-empty") {
        Optional<int> opt1(42);
        Optional<int> opt2;
        opt2 = opt1;
        FL_CHECK(opt2.has_value());
        FL_CHECK(*opt2 == 42);
    }

    FL_SUBCASE("move assign from empty") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        opt2 = fl::move(opt1);
        FL_CHECK(opt2.empty());
    }

    FL_SUBCASE("move assign from non-empty") {
        Optional<int> opt1(42);
        Optional<int> opt2;
        opt2 = fl::move(opt1);
        FL_CHECK(opt2.has_value());
        FL_CHECK(*opt2 == 42);
    }

    FL_SUBCASE("assign nullopt") {
        Optional<int> opt(42);
        opt = nullopt;
        FL_CHECK(opt.empty());
        FL_CHECK(opt == nullopt);
    }

    FL_SUBCASE("assign value lvalue") {
        Optional<int> opt;
        int value = 42;
        opt = value;
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("assign value rvalue") {
        Optional<int> opt;
        opt = 42;
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("self-assignment") {
        Optional<int> opt(42);
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
        opt = opt;
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }
}

FL_TEST_CASE("fl::Optional - emplace") {
    FL_SUBCASE("emplace into empty optional") {
        Optional<int> opt;
        opt.emplace(42);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("emplace into non-empty optional") {
        Optional<int> opt(10);
        opt.emplace(42);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }
}

FL_TEST_CASE("fl::Optional - reset") {
    FL_SUBCASE("reset empty optional") {
        Optional<int> opt;
        opt.reset();
        FL_CHECK(opt.empty());
    }

    FL_SUBCASE("reset non-empty optional") {
        Optional<int> opt(42);
        opt.reset();
        FL_CHECK(opt.empty());
        FL_CHECK(opt == nullopt);
    }
}

FL_TEST_CASE("fl::Optional - ptr and const ptr") {
    FL_SUBCASE("ptr on empty optional") {
        Optional<int> opt;
        FL_CHECK(opt.ptr() == nullptr);
    }

    FL_SUBCASE("ptr on non-empty optional") {
        Optional<int> opt(42);
        int* p = opt.ptr();
        FL_CHECK(p != nullptr);
        FL_CHECK(*p == 42);
        *p = 100;
        FL_CHECK(*opt == 100);
    }

    FL_SUBCASE("const ptr on non-empty optional") {
        const Optional<int> opt(42);
        const int* p = opt.ptr();
        FL_CHECK(p != nullptr);
        FL_CHECK(*p == 42);
    }
}

FL_TEST_CASE("fl::Optional - dereference operators") {
    FL_SUBCASE("operator* lvalue") {
        Optional<int> opt(42);
        FL_CHECK(*opt == 42);
        *opt = 100;
        FL_CHECK(*opt == 100);
    }

    FL_SUBCASE("operator* const") {
        const Optional<int> opt(42);
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("operator-> with struct") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Optional<Point> opt(Point(10, 20));
        FL_CHECK(opt->x == 10);
        FL_CHECK(opt->y == 20);
        opt->x = 30;
        FL_CHECK(opt->x == 30);
    }

    FL_SUBCASE("operator-> const") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        const Optional<Point> opt(Point(10, 20));
        FL_CHECK(opt->x == 10);
        FL_CHECK(opt->y == 20);
    }
}

FL_TEST_CASE("fl::Optional - boolean operators") {
    FL_SUBCASE("operator! on empty") {
        Optional<int> opt;
        FL_CHECK(opt.operator!());
    }

    FL_SUBCASE("operator! on non-empty") {
        Optional<int> opt(42);
        FL_CHECK(!opt.operator!());
    }

    FL_SUBCASE("operator() on empty") {
        Optional<int> opt;
        FL_CHECK(!opt.operator()());
    }

    FL_SUBCASE("operator() on non-empty") {
        Optional<int> opt(42);
        FL_CHECK(opt.operator()());
    }

    FL_SUBCASE("explicit operator bool on empty") {
        Optional<int> opt;
        FL_CHECK(!static_cast<bool>(opt));
        if (opt) {
            FL_CHECK(false); // Should not reach here
        }
    }

    FL_SUBCASE("explicit operator bool on non-empty") {
        Optional<int> opt(42);
        FL_CHECK(static_cast<bool>(opt));
        if (opt) {
            FL_CHECK(true); // Should reach here
        } else {
            FL_CHECK(false); // Should not reach here
        }
    }
}

FL_TEST_CASE("fl::Optional - equality operators") {
    FL_SUBCASE("two empty optionals are equal") {
        Optional<int> opt1;
        Optional<int> opt2;
        FL_CHECK(opt1 == opt2);
        FL_CHECK(!(opt1 != opt2));
    }

    FL_SUBCASE("empty and non-empty are not equal") {
        Optional<int> opt1;
        Optional<int> opt2(42);
        FL_CHECK(opt1 != opt2);
        FL_CHECK(!(opt1 == opt2));
        FL_CHECK(opt2 != opt1);
        FL_CHECK(!(opt2 == opt1));
    }

    FL_SUBCASE("two non-empty with same value are equal") {
        Optional<int> opt1(42);
        Optional<int> opt2(42);
        FL_CHECK(opt1 == opt2);
        FL_CHECK(!(opt1 != opt2));
    }

    FL_SUBCASE("two non-empty with different values are not equal") {
        Optional<int> opt1(42);
        Optional<int> opt2(43);
        FL_CHECK(opt1 != opt2);
        FL_CHECK(!(opt1 == opt2));
    }

    FL_SUBCASE("compare with value - empty") {
        Optional<int> opt;
        FL_CHECK(!(opt == 42));
    }

    FL_SUBCASE("compare with value - matching") {
        Optional<int> opt(42);
        FL_CHECK(opt == 42);
    }

    FL_SUBCASE("compare with value - not matching") {
        Optional<int> opt(42);
        FL_CHECK(!(opt == 43));
    }

    FL_SUBCASE("compare with nullopt - empty") {
        Optional<int> opt;
        FL_CHECK(opt == nullopt);
        FL_CHECK(!(opt != nullopt));
    }

    FL_SUBCASE("compare with nullopt - non-empty") {
        Optional<int> opt(42);
        FL_CHECK(!(opt == nullopt));
        FL_CHECK(opt != nullopt);
    }
}

FL_TEST_CASE("fl::Optional - swap") {
    // Note: swap requires variant to have swap() method
    // If variant doesn't support swap, these tests may not work

    FL_SUBCASE("manual swap via move") {
        Optional<int> opt1(10);
        Optional<int> opt2(20);
        Optional<int> temp(fl::move(opt1));
        opt1 = fl::move(opt2);
        opt2 = fl::move(temp);
        FL_CHECK(*opt1 == 20);
        FL_CHECK(*opt2 == 10);
    }
}

FL_TEST_CASE("fl::make_optional helper functions") {
    FL_SUBCASE("make_optional with rvalue") {
        auto opt = make_optional(42);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }

    FL_SUBCASE("make_optional type deduction") {
        auto opt_int = make_optional(42);
        auto opt_double = make_optional(3.14);

        FL_CHECK(opt_int.has_value());
        FL_CHECK(opt_double.has_value());
        FL_CHECK(*opt_int == 42);
        FL_CHECK_CLOSE(*opt_double, 3.14, 0.001);
    }

    FL_SUBCASE("make_optional with explicit copy") {
        // To avoid reference issues, we construct from the value directly
        int value = 42;
        Optional<int> opt(value);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);
    }
}

FL_TEST_CASE("fl::Optional - value() method") {
    FL_SUBCASE("value() on non-empty optional") {
        Optional<int> opt(42);
        FL_CHECK(opt.value() == 42);
        opt.value() = 100;
        FL_CHECK(opt.value() == 100);
    }

    FL_SUBCASE("value() const on non-empty optional") {
        const Optional<int> opt(42);
        FL_CHECK(opt.value() == 42);
    }

    FL_SUBCASE("value() with struct") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Optional<Point> opt(Point(10, 20));
        FL_CHECK(opt.value().x == 10);
        FL_CHECK(opt.value().y == 20);
        opt.value().x = 30;
        FL_CHECK(opt.value().x == 30);
    }

    FL_SUBCASE("value() is compatible with operator*") {
        Optional<int> opt(42);
        FL_CHECK(opt.value() == *opt);
        FL_CHECK(&opt.value() == &(*opt));
    }
}

FL_TEST_CASE("fl::Optional - edge cases") {
    FL_SUBCASE("optional of bool") {
        Optional<bool> opt_false(false);
        Optional<bool> opt_true(true);
        Optional<bool> opt_empty;

        // Explicit bool conversion vs contained bool value
        FL_CHECK(opt_false.has_value());
        FL_CHECK(*opt_false == false);
        FL_CHECK(static_cast<bool>(opt_false) == true); // has value

        FL_CHECK(opt_true.has_value());
        FL_CHECK(*opt_true == true);

        FL_CHECK(!opt_empty.has_value());
        FL_CHECK(static_cast<bool>(opt_empty) == false);
    }

    FL_SUBCASE("optional of pointer") {
        int x = 42;
        int* ptr = &x;
        Optional<int*> opt(ptr);

        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == ptr);
        FL_CHECK(**opt == 42);
    }

    FL_SUBCASE("multiple reset calls") {
        Optional<int> opt(42);
        opt.reset();
        opt.reset();
        opt.reset();
        FL_CHECK(opt.empty());
    }

    FL_SUBCASE("assign after reset") {
        Optional<int> opt(42);
        opt.reset();
        FL_CHECK(opt.empty());
        opt = 100;
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 100);
    }
}

FL_TEST_CASE("fl::Optional - constexpr support") {
    FL_SUBCASE("constexpr nullopt_t") {
        constexpr nullopt_t n = nullopt;
        (void)n;
    }

    // Note: Full constexpr Optional operations would require constexpr variant
    // which may not be supported. Testing what we can.
}

FL_TEST_CASE("fl::Optional - type alias") {
    FL_SUBCASE("optional is alias for Optional") {
        // Test that the lowercase alias works
        optional<int> opt(42);
        FL_CHECK(opt.has_value());
        FL_CHECK(*opt == 42);

        Optional<int> opt2(42);
        FL_CHECK(opt == opt2);
    }
}

FL_TEST_CASE("fl::Optional<T&&> - rvalue reference specialization") {
    FL_SUBCASE("default construction creates empty optional") {
        Optional<int&&> opt;
        FL_CHECK(opt.empty());
        FL_CHECK(!opt.has_value());
        FL_CHECK(!opt);
        FL_CHECK(opt == nullopt);
    }

    FL_SUBCASE("nullopt constructor creates empty optional") {
        Optional<int&&> opt(nullopt);
        FL_CHECK(opt.empty());
        FL_CHECK(!opt.has_value());
        FL_CHECK(opt == nullopt);
    }

    FL_SUBCASE("construct with rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        FL_CHECK(!opt.empty());
        FL_CHECK(opt.has_value());
        FL_CHECK(opt);
        FL_CHECK(opt != nullopt);
        // Verify the reference points to the original value
        FL_CHECK(opt.ptr() == &value);
    }

    FL_SUBCASE("move construction transfers reference") {
        int value = 100;
        Optional<int&&> opt1(fl::move(value));
        FL_CHECK(opt1.has_value());
        FL_CHECK(opt1.ptr() == &value);

        Optional<int&&> opt2(fl::move(opt1));
        FL_CHECK(opt2.has_value());
        FL_CHECK(opt2.ptr() == &value);
        FL_CHECK(opt1.empty());  // Original is now empty
    }

    FL_SUBCASE("move assignment transfers reference") {
        int value1 = 42;
        int value2 = 100;
        Optional<int&&> opt1(fl::move(value1));
        Optional<int&&> opt2(fl::move(value2));

        FL_CHECK(opt1.ptr() == &value1);
        FL_CHECK(opt2.ptr() == &value2);

        opt1 = fl::move(opt2);
        FL_CHECK(opt1.ptr() == &value2);
        FL_CHECK(opt2.empty());
    }

    FL_SUBCASE("assign nullopt clears reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        FL_CHECK(opt.has_value());

        opt = nullopt;
        FL_CHECK(opt.empty());
        FL_CHECK(opt == nullopt);
    }

    FL_SUBCASE("reset clears reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));
        FL_CHECK(opt.has_value());

        opt.reset();
        FL_CHECK(opt.empty());
    }

    FL_SUBCASE("dereference operator forwards rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        // Get rvalue reference and verify it can be moved
        int&& ref = *opt;
        int moved_value = fl::move(ref);
        FL_CHECK(moved_value == 42);
    }

    FL_SUBCASE("get method forwards rvalue reference") {
        int value = 99;
        Optional<int&&> opt(fl::move(value));

        int&& ref = opt.get();
        FL_CHECK(ref == 99);
    }

    FL_SUBCASE("arrow operator provides member access") {
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        Point p(10, 20);
        Optional<Point&&> opt(fl::move(p));

        FL_CHECK(opt->x == 10);
        FL_CHECK(opt->y == 20);
    }

    FL_SUBCASE("boolean operators work correctly") {
        int value = 42;
        Optional<int&&> opt_empty;
        Optional<int&&> opt_full(fl::move(value));

        FL_CHECK(opt_empty.operator!());
        FL_CHECK(!opt_empty.operator()());
        FL_CHECK(!static_cast<bool>(opt_empty));

        FL_CHECK(!opt_full.operator!());
        FL_CHECK(opt_full.operator()());
        FL_CHECK(static_cast<bool>(opt_full));
    }

    FL_SUBCASE("equality operators") {
        int value1 = 42;
        int value2 = 42;
        int value3 = 99;

        Optional<int&&> opt_empty1;
        Optional<int&&> opt_empty2;
        Optional<int&&> opt1(fl::move(value1));
        Optional<int&&> opt2(fl::move(value2));
        Optional<int&&> opt3(fl::move(value3));

        // Two empty optionals are equal
        FL_CHECK(opt_empty1 == opt_empty2);

        // Empty and non-empty are not equal
        FL_CHECK(opt_empty1 != opt1);

        // Two optionals with same value are equal
        FL_CHECK(opt1 == opt2);

        // Two optionals with different values are not equal
        FL_CHECK(opt1 != opt3);
    }

    FL_SUBCASE("ptr method returns correct pointer") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        int* p = opt.ptr();
        FL_CHECK(p == &value);
        FL_CHECK(*p == 42);
    }

    FL_SUBCASE("forwarding to function consuming rvalue reference") {
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
        FL_CHECK(result == 42);
    }

    FL_SUBCASE("lifetime semantics - reference validity") {
        // This test verifies that the optional correctly holds a reference
        // to an existing object and doesn't try to manage its lifetime
        int value = 100;
        Optional<int&&> opt(fl::move(value));

        // Modify through the optional
        *opt.ptr() = 200;

        // Verify the original value changed
        FL_CHECK(value == 200);
    }

    FL_SUBCASE("move from optional into new variable") {
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
        FL_CHECK(new_obj.value == 42);
        FL_CHECK(obj.value == -1);  // Original was moved from
    }

    FL_SUBCASE("value() method forwards rvalue reference") {
        int value = 42;
        Optional<int&&> opt(fl::move(value));

        // Get rvalue reference using value() and verify it can be moved
        int&& ref = opt.value();
        int moved_value = fl::move(ref);
        FL_CHECK(moved_value == 42);
    }

    FL_SUBCASE("value() is compatible with operator* and get()") {
        int value = 99;
        Optional<int&&> opt(fl::move(value));

        // All three should return the same reference
        int&& ref1 = opt.value();
        int&& ref2 = *opt;
        int&& ref3 = opt.get();

        FL_CHECK(&ref1 == &value);
        FL_CHECK(&ref2 == &value);
        FL_CHECK(&ref3 == &value);
    }
}
