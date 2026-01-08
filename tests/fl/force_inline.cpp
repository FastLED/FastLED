#include "fl/force_inline.h"
#include "doctest.h"

// Test that FASTLED_FORCE_INLINE macro is defined and can be used

// Define test functions using the macro
FASTLED_FORCE_INLINE int add_force_inline(int a, int b) {
    return a + b;
}

FASTLED_FORCE_INLINE float multiply_force_inline(float a, float b) {
    return a * b;
}

FASTLED_FORCE_INLINE bool is_positive_force_inline(int x) {
    return x > 0;
}

// Test with template functions
template<typename T>
FASTLED_FORCE_INLINE T max_force_inline(T a, T b) {
    return (a > b) ? a : b;
}

// Test with constexpr
FASTLED_FORCE_INLINE constexpr int square_force_inline(int x) {
    return x * x;
}

TEST_CASE("FASTLED_FORCE_INLINE macro") {
    SUBCASE("basic integer functions") {
        // Test that functions marked with FASTLED_FORCE_INLINE work correctly
        CHECK_EQ(add_force_inline(2, 3), 5);
        CHECK_EQ(add_force_inline(-5, 10), 5);
        CHECK_EQ(add_force_inline(0, 0), 0);
        CHECK_EQ(add_force_inline(100, -50), 50);
    }

    SUBCASE("floating point functions") {
        CHECK_EQ(multiply_force_inline(2.0f, 3.0f), 6.0f);
        CHECK_EQ(multiply_force_inline(0.5f, 4.0f), 2.0f);
        CHECK_EQ(multiply_force_inline(-2.0f, 3.0f), -6.0f);
        CHECK_EQ(multiply_force_inline(0.0f, 100.0f), 0.0f);
    }

    SUBCASE("boolean functions") {
        CHECK(is_positive_force_inline(1));
        CHECK(is_positive_force_inline(100));
        CHECK_FALSE(is_positive_force_inline(0));
        CHECK_FALSE(is_positive_force_inline(-1));
        CHECK_FALSE(is_positive_force_inline(-100));
    }

    SUBCASE("template functions") {
        // Test with int
        CHECK_EQ(max_force_inline(5, 10), 10);
        CHECK_EQ(max_force_inline(10, 5), 10);
        CHECK_EQ(max_force_inline(-5, -10), -5);

        // Test with float
        CHECK_EQ(max_force_inline(5.5f, 10.5f), 10.5f);
        CHECK_EQ(max_force_inline(10.5f, 5.5f), 10.5f);

        // Test with double
        CHECK_EQ(max_force_inline(3.14, 2.71), 3.14);
        CHECK_EQ(max_force_inline(2.71, 3.14), 3.14);
    }

    SUBCASE("constexpr functions") {
        // Test that FASTLED_FORCE_INLINE works with constexpr
        CHECK_EQ(square_force_inline(0), 0);
        CHECK_EQ(square_force_inline(1), 1);
        CHECK_EQ(square_force_inline(5), 25);
        CHECK_EQ(square_force_inline(-3), 9);
        CHECK_EQ(square_force_inline(10), 100);

        // Compile-time evaluation test
        constexpr int result = square_force_inline(7);
        static_assert(result == 49, "constexpr function should work at compile time");
    }

    SUBCASE("macro is defined") {
        // Just verify the macro exists and expands to something
        // This test just checks compilation succeeds with the macro
#ifdef FASTLED_NO_FORCE_INLINE
        // If FASTLED_NO_FORCE_INLINE is defined, macro should expand to just "inline"
        // We can't directly test the expansion, but we can verify functions work
        CHECK(true); // Placeholder to verify this branch compiles
#else
        // Default case: macro should expand to "__attribute__((always_inline)) inline"
        // Again, we verify functions work correctly
        CHECK(true); // Placeholder to verify this branch compiles
#endif
    }

    SUBCASE("multiple calls") {
        // Test that force-inlined functions can be called multiple times
        int sum = 0;
        for (int i = 0; i < 10; i++) {
            sum = add_force_inline(sum, i);
        }
        CHECK_EQ(sum, 45); // 0+1+2+3+4+5+6+7+8+9 = 45
    }

    SUBCASE("nested calls") {
        // Test that force-inlined functions can call each other
        int a = add_force_inline(2, 3);        // 5
        int b = add_force_inline(a, 4);        // 9
        int c = square_force_inline(b);         // 81
        CHECK_EQ(c, 81);
    }
}

// Additional test to verify the macro doesn't break with various function signatures
namespace test_signatures {
    // Return void
    FASTLED_FORCE_INLINE void do_nothing() {}

    // Multiple parameters
    FASTLED_FORCE_INLINE int add_three(int a, int b, int c) {
        return a + b + c;
    }

    // Reference parameters
    FASTLED_FORCE_INLINE void increment_ref(int& x) {
        x++;
    }

    // Const reference parameters
    FASTLED_FORCE_INLINE int get_value(const int& x) {
        return x;
    }

    // Pointer parameters
    FASTLED_FORCE_INLINE void set_value(int* ptr, int value) {
        if (ptr) *ptr = value;
    }
}

TEST_CASE("FASTLED_FORCE_INLINE with various signatures") {
    using namespace test_signatures;

    SUBCASE("void return") {
        // Just verify it compiles and runs
        do_nothing();
        CHECK(true);
    }

    SUBCASE("multiple parameters") {
        CHECK_EQ(add_three(1, 2, 3), 6);
        CHECK_EQ(add_three(10, 20, 30), 60);
    }

    SUBCASE("reference parameters") {
        int x = 5;
        increment_ref(x);
        CHECK_EQ(x, 6);
        increment_ref(x);
        CHECK_EQ(x, 7);
    }

    SUBCASE("const reference parameters") {
        int x = 42;
        CHECK_EQ(get_value(x), 42);
    }

    SUBCASE("pointer parameters") {
        int x = 0;
        set_value(&x, 100);
        CHECK_EQ(x, 100);
        set_value(&x, -50);
        CHECK_EQ(x, -50);
    }
}
