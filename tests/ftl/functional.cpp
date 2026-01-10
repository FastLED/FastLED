#include "fl/stl/functional.h"
#include "fl/stl/scoped_ptr.h"
#include "fl/stl/function.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/move.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/unique_ptr.h"

using namespace fl;

namespace functional_test {

// Test helper structures and functions

// Simple free function
int add(int a, int b) {
    return a + b;
}

// Function with no arguments
int get_42() {
    return 42;
}

} // namespace functional_test

namespace {

// Struct with member functions and member data
struct Calculator {
    int value;

    Calculator(int v = 0) : value(v) {}

    int add(int x) {
        return value + x;
    }

    int add_const(int x) const {
        return value + x;
    }

    int multiply(int x, int y) {
        return x * y;
    }

    static int static_add(int a, int b) {
        return a + b;
    }
};

// Functor (function object)
struct Multiplier {
    int factor;

    Multiplier(int f) : factor(f) {}

    int operator()(int x) const {
        return x * factor;
    }
};

} // anonymous namespace

// Test invoke with regular functions
TEST_CASE("fl::invoke with free functions") {
    SUBCASE("function with arguments") {
        int result = invoke(functional_test::add, 3, 4);
        CHECK_EQ(result, 7);
    }

    SUBCASE("function with no arguments") {
        int result = invoke(functional_test::get_42);
        CHECK_EQ(result, 42);
    }

    SUBCASE("function pointer") {
        int (*fn_ptr)(int, int) = &functional_test::add;
        int result = invoke(fn_ptr, 10, 20);
        CHECK_EQ(result, 30);
    }
}

// Test invoke with member function pointers
TEST_CASE("fl::invoke with member function pointers") {
    SUBCASE("member function with object reference") {
        Calculator calc(5);
        int result = invoke(&Calculator::add, calc, 3);
        CHECK_EQ(result, 8);
    }

    SUBCASE("const member function with object reference") {
        const Calculator calc(10);
        int result = invoke(&Calculator::add_const, calc, 5);
        CHECK_EQ(result, 15);
    }

    SUBCASE("member function with multiple arguments") {
        Calculator calc(0);
        int result = invoke(&Calculator::multiply, calc, 6, 7);
        CHECK_EQ(result, 42);
    }

    SUBCASE("member function with pointer") {
        Calculator calc(20);
        Calculator* ptr = &calc;
        int result = invoke(&Calculator::add, ptr, 10);
        CHECK_EQ(result, 30);
    }

    SUBCASE("member function with const pointer") {
        const Calculator calc(15);
        const Calculator* ptr = &calc;
        int result = invoke(&Calculator::add_const, ptr, 5);
        CHECK_EQ(result, 20);
    }
}

// Test invoke with member data pointers
TEST_CASE("fl::invoke with member data pointers") {
    SUBCASE("member data with object reference") {
        Calculator calc(99);
        int result = invoke(&Calculator::value, calc);
        CHECK_EQ(result, 99);
    }

    SUBCASE("member data with pointer") {
        Calculator calc(42);
        Calculator* ptr = &calc;
        int result = invoke(&Calculator::value, ptr);
        CHECK_EQ(result, 42);
    }

    SUBCASE("member data with const pointer") {
        const Calculator calc(100);
        const Calculator* ptr = &calc;
        int result = invoke(&Calculator::value, ptr);
        CHECK_EQ(result, 100);
    }
}

// Test invoke with functors
TEST_CASE("fl::invoke with functors") {
    SUBCASE("functor object") {
        Multiplier mult(3);
        int result = invoke(mult, 7);
        CHECK_EQ(result, 21);
    }

    SUBCASE("functor with move") {
        Multiplier mult(5);
        int result = invoke(fl::move(mult), 8);
        CHECK_EQ(result, 40);
    }
}

// Test invoke with lambdas
TEST_CASE("fl::invoke with lambdas") {
    SUBCASE("lambda with no capture") {
        auto lambda = [](int x, int y) { return x + y; };
        int result = invoke(lambda, 3, 4);
        CHECK_EQ(result, 7);
    }

    SUBCASE("lambda with capture") {
        int factor = 10;
        auto lambda = [factor](int x) { return x * factor; };
        int result = invoke(lambda, 5);
        CHECK_EQ(result, 50);
    }

    SUBCASE("lambda with mutable capture") {
        int counter = 0;
        auto lambda = [&counter]() { return ++counter; };
        int result1 = invoke(lambda);
        int result2 = invoke(lambda);
        CHECK_EQ(result1, 1);
        CHECK_EQ(result2, 2);
    }

    SUBCASE("lambda returning void") {
        int value = 0;
        auto lambda = [&value](int x) { value = x * 2; };
        invoke(lambda, 21);
        CHECK_EQ(value, 42);
    }
}

// Test invoke with static member functions
TEST_CASE("fl::invoke with static member functions") {
    SUBCASE("static member function") {
        int result = invoke(&Calculator::static_add, 15, 25);
        CHECK_EQ(result, 40);
    }
}

// Test type traits used by invoke
TEST_CASE("fl::invoke helper traits") {
    // Test is_member_data_pointer
    SUBCASE("is_member_data_pointer trait") {
        using namespace detail;
        static_assert(!is_member_data_pointer<int>::value, "int is not a member data pointer");
        static_assert(is_member_data_pointer<int Calculator::*>::value, "int Calculator::* is a member data pointer");
        static_assert(!is_member_data_pointer<int (Calculator::*)(int)>::value, "member function pointer is not a member data pointer");
    }

    // Test is_pointer_like
    SUBCASE("is_pointer_like trait") {
        using namespace detail;
        static_assert(!is_pointer_like<int>::value, "int is not pointer-like");
        static_assert(!is_pointer_like<Calculator>::value, "Calculator is not pointer-like");
        static_assert(is_pointer_like<int*>::value, "int* is pointer-like");
        static_assert(is_pointer_like<Calculator*>::value, "Calculator* is pointer-like");
    }
}

// Test edge cases
TEST_CASE("fl::invoke edge cases") {
    SUBCASE("invoke with temporary object") {
        int result = invoke(&Calculator::add, Calculator(100), 50);
        CHECK_EQ(result, 150);
    }

    SUBCASE("invoke with temporary functor") {
        int result = invoke(Multiplier(4), 10);
        CHECK_EQ(result, 40);
    }

    SUBCASE("invoke with temporary lambda") {
        int result = invoke([](int x) { return x * x; }, 9);
        CHECK_EQ(result, 81);
    }

    SUBCASE("nested invoke calls") {
        auto add_lambda = [](int a, int b) { return a + b; };
        auto mult_lambda = [](int x, int y) { return x * y; };

        int sum = invoke(add_lambda, 3, 4);
        int product = invoke(mult_lambda, sum, 2);
        CHECK_EQ(product, 14);
    }
}

// Test invoke with different return types
TEST_CASE("fl::invoke with various return types") {
    SUBCASE("return bool") {
        auto is_positive = [](int x) { return x > 0; };
        bool result = invoke(is_positive, 5);
        CHECK(result);
    }

    SUBCASE("return float") {
        auto divide = [](float a, float b) { return a / b; };
        float result = invoke(divide, 10.0f, 2.0f);
        CHECK_EQ(result, 5.0f);
    }

    SUBCASE("return void") {
        int side_effect = 0;
        auto set_value = [&side_effect](int x) { side_effect = x; };
        invoke(set_value, 99);
        CHECK_EQ(side_effect, 99);
    }
}

// Test invoke forwarding behavior
TEST_CASE("fl::invoke forwarding") {
    SUBCASE("forwards arguments correctly") {
        struct MoveOnly {
            int value;
            bool moved_from;
            MoveOnly(int v) : value(v), moved_from(false) {}
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly(MoveOnly&& other) : value(other.value), moved_from(false) {
                other.moved_from = true;
            }
        };

        // Lambda that actually moves from the parameter
        auto extract = [](MoveOnly obj) { return obj.value; };
        MoveOnly obj(42);
        int result = invoke(extract, fl::move(obj));
        CHECK_EQ(result, 42);
        CHECK(obj.moved_from); // Verify object was moved from
    }
}

// Test fl::invoke with fl::scoped_ptr smart pointers
// (Merged from test_invoke.cpp)
TEST_CASE("fl::invoke with scoped_ptr smart pointers") {
    struct TestScopedPtrClass {
        int value = 42;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int add(int x) const { return value + x; }
        int multiply(int x) { return value * x; }
    };

    // Test with scoped_ptr
    fl::scoped_ptr<TestScopedPtrClass> scopedPtr(new TestScopedPtrClass);

    // Member function: const getter
    CHECK_EQ(42, invoke(&TestScopedPtrClass::getValue, scopedPtr));

    // Member function: setter
    invoke(&TestScopedPtrClass::setValue, scopedPtr, 123);
    CHECK_EQ(123, scopedPtr->value);

    // Member function with additional arg, const
    CHECK_EQ(133, invoke(&TestScopedPtrClass::add, scopedPtr, 10));

    // Member function with additional arg, non-const
    CHECK_EQ(246, invoke(&TestScopedPtrClass::multiply, scopedPtr, 2));

    // Member data pointer access and modification
    CHECK_EQ(123, invoke(&TestScopedPtrClass::value, scopedPtr));
    invoke(&TestScopedPtrClass::value, scopedPtr) = 999;
    CHECK_EQ(999, scopedPtr->value);

    // Test with custom deleter
    struct CustomDeleter {
        void operator()(TestScopedPtrClass* ptr) {
            delete ptr;
        }
    };

    fl::scoped_ptr<TestScopedPtrClass, CustomDeleter> customScopedPtr(new TestScopedPtrClass, CustomDeleter{});

    // Member function with custom deleter scoped_ptr
    CHECK_EQ(42, invoke(&TestScopedPtrClass::getValue, customScopedPtr));
    invoke(&TestScopedPtrClass::setValue, customScopedPtr, 555);
    CHECK_EQ(555, customScopedPtr->value);
    CHECK_EQ(565, invoke(&TestScopedPtrClass::add, customScopedPtr, 10));
}

// Test fl::invoke with fl::function objects
// (Merged from test_invoke.cpp)
TEST_CASE("fl::invoke with fl::function objects") {
    struct TestFunctionClass {
        int value = 100;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int add(int x) const { return value + x; }
        int multiply(int x) { return value * x; }
    };

    // 1. Test fl::function with free function
    fl::function<int(int, int)> free_func = functional_test::add;
    auto result1 = invoke(free_func, 10, 20);
    CHECK_EQ(30, result1);

    // 2. Test fl::function with lambda
    fl::function<int(int, int)> lambda_func = [](int a, int b) { return a * b; };
    auto result2 = invoke(lambda_func, 6, 7);
    CHECK_EQ(42, result2);

    // 3. Test fl::function with member function bound to object
    TestFunctionClass obj;
    fl::function<int()> member_func = fl::function<int()>(&TestFunctionClass::getValue, &obj);
    auto result3 = invoke(member_func);
    CHECK_EQ(100, result3);

    // 4. Test fl::function with member function bound to raw pointer
    TestFunctionClass* raw_ptr = &obj;
    fl::function<void(int)> setter_func = fl::function<void(int)>(&TestFunctionClass::setValue, raw_ptr);
    invoke(setter_func, 200);
    CHECK_EQ(200, obj.value);

    // 5. Test fl::function with member function bound to scoped_ptr
    fl::scoped_ptr<TestFunctionClass> scoped_ptr(new TestFunctionClass);
    scoped_ptr->setValue(300);

    // Create function bound to scoped_ptr
    fl::function<int()> scoped_getter = fl::function<int()>(&TestFunctionClass::getValue, scoped_ptr.get());
    auto result4 = invoke(scoped_getter);
    CHECK_EQ(300, result4);

    // 6. Test fl::function with member function with args bound to scoped_ptr
    fl::function<int(int)> scoped_adder = fl::function<int(int)>(&TestFunctionClass::add, scoped_ptr.get());
    auto result5 = invoke(scoped_adder, 50);
    CHECK_EQ(350, result5);

    // 7. Test fl::function with complex lambda capturing scoped_ptr
    auto complex_lambda = [&scoped_ptr](int multiplier) {
        return invoke(&TestFunctionClass::multiply, scoped_ptr, multiplier);
    };
    fl::function<int(int)> complex_func = complex_lambda;
    auto result6 = invoke(complex_func, 3);
    CHECK_EQ(900, result6);  // 300 * 3 = 900

    // 8. Test nested fl::invoke calls
    fl::function<int(int)> nested_func = [&scoped_ptr](int x) {
        // Use fl::invoke inside a function that's also invoked with fl::invoke
        return invoke(&TestFunctionClass::add, scoped_ptr, x) * 2;
    };
    auto result7 = invoke(nested_func, 25);
    CHECK_EQ(650, result7);  // (300 + 25) * 2 = 650
}
