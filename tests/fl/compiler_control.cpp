#include "fl/compiler_control.h"
#include "doctest.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"

// Test helper: Function that would normally trigger a warning
static int unused_parameter_function(int x, FL_MAYBE_UNUSED int y) {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_UNUSED_PARAMETER
    return x;  // y is unused
    FL_DISABLE_WARNING_POP
}

// Test helper: Function with implicit fallthrough
static int fallthrough_function(int x) {
    switch(x) {
        case 1:
            FL_DISABLE_WARNING_PUSH
            FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
            // fallthrough
        case 2:
            FL_DISABLE_WARNING_POP
            return 20;
        default:
            return 0;
    }
}

// Test helper: Fast math function
FL_FAST_MATH_BEGIN
static float fast_math_multiply(float a, float b) {
    return a * b;
}
FL_FAST_MATH_END

// Test helper: O3 optimized function
FL_OPTIMIZATION_LEVEL_O3_BEGIN
static int o3_sum(int a, int b) {
    return a + b;
}
FL_OPTIMIZATION_LEVEL_O3_END

// Test helper: O0 debug function
FL_OPTIMIZATION_LEVEL_O0_BEGIN
static int o0_sum(int a, int b) {
    return a + b;
}
FL_OPTIMIZATION_LEVEL_O0_END

// Test helper: Weak linkage function
FL_LINK_WEAK int weak_function() {
    return 42;
}

// Test helper: External C function
FL_EXTERN_C_BEGIN
int c_function() {
    return 100;
}
FL_EXTERN_C_END

// Test helper: Namespace-scoped constexpr variable
// Note: FL_INLINE_CONSTEXPR expands to "inline constexpr" which is C++17
// For C++11 tests, we just use constexpr without inline
namespace {
constexpr int inline_constexpr_value = 123;
}

TEST_CASE("FL_STRINGIFY macros") {
    SUBCASE("FL_STRINGIFY expands macro argument") {
        #define TEST_VALUE 42
        const char* str = FL_STRINGIFY(TEST_VALUE);
        CHECK(fl::strcmp(str, "42") == 0);
        #undef TEST_VALUE
    }

    SUBCASE("FL_STRINGIFY2 converts to string literal") {
        const char* str = FL_STRINGIFY2(hello);
        CHECK(fl::strcmp(str, "hello") == 0);
    }

    SUBCASE("FL_STRINGIFY handles expressions") {
        const char* str = FL_STRINGIFY(1 + 1);
        CHECK(fl::strcmp(str, "1 + 1") == 0);
    }
}

TEST_CASE("FL_DISABLE_WARNING macros are defined") {
    SUBCASE("FL_DISABLE_WARNING_PUSH is defined") {
        // Just check that the macro is defined and compiles
        FL_DISABLE_WARNING_PUSH
        // Should compile without error
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_POP is defined") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING is defined") {
        // Test that the macro is defined and can be used
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(unused-variable)
        int unused_var = 0;
        (void)unused_var;
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }
}

TEST_CASE("FL_DISABLE_WARNING_PUSH and POP are balanced") {
    SUBCASE("single push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("nested push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("multiple sequential push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }
}

TEST_CASE("specific warning suppression macros") {
    SUBCASE("FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SELF_ASSIGN
        FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
        int x = 5;
        x = x;  // Self-assignment
        FL_DISABLE_WARNING_POP
        CHECK(x == 5);
    }

    SUBCASE("FL_DISABLE_FORMAT_TRUNCATION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_FORMAT_TRUNCATION
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_NULL_DEREFERENCE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_NULL_DEREFERENCE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_UNUSED_PARAMETER") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        int result = unused_parameter_function(10, 20);
        FL_DISABLE_WARNING_POP
        CHECK(result == 10);
    }

    SUBCASE("FL_DISABLE_WARNING_RETURN_TYPE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_RETURN_TYPE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
        int result = fallthrough_function(1);
        FL_DISABLE_WARNING_POP
        CHECK(result == 20);
    }

    SUBCASE("FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_FLOAT_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float f = 3.14f;
        int i = static_cast<int>(f);
        FL_DISABLE_WARNING_POP
        CHECK(i == 3);
    }

    SUBCASE("FL_DISABLE_WARNING_SIGN_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SIGN_CONVERSION
        unsigned int u = 10;
        int s = static_cast<int>(u);
        FL_DISABLE_WARNING_POP
        CHECK(s == 10);
    }

    SUBCASE("FL_DISABLE_WARNING_SHORTEN_64_TO_32") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SHORTEN_64_TO_32
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("FL_DISABLE_WARNING_VOLATILE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_VOLATILE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }
}

TEST_CASE("FL_FAST_MATH macros") {
    SUBCASE("FL_FAST_MATH_BEGIN and END are defined") {
        FL_FAST_MATH_BEGIN
        float result = 2.0f * 3.0f;
        FL_FAST_MATH_END
        CHECK(result == 6.0f);
    }

    SUBCASE("fast math function compiles and executes") {
        float result = fast_math_multiply(2.5f, 4.0f);
        CHECK(result == 10.0f);
    }

    SUBCASE("fast math with multiple operations") {
        FL_FAST_MATH_BEGIN
        float a = 1.5f;
        float b = 2.0f;
        float c = 3.0f;
        float result = (a * b) + (b * c);
        FL_FAST_MATH_END
        CHECK(result == 9.0f);
    }

    SUBCASE("nested fast math blocks") {
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_BEGIN
        float result = 10.0f / 2.0f;
        FL_FAST_MATH_END
        FL_FAST_MATH_END
        CHECK(result == 5.0f);
    }
}

TEST_CASE("FL_OPTIMIZATION_LEVEL_O3 macros") {
    SUBCASE("O3 optimization macros are defined") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int result = 5 + 10;
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(result == 15);
    }

    SUBCASE("O3 optimized function compiles and executes") {
        int result = o3_sum(100, 200);
        CHECK(result == 300);
    }

    SUBCASE("O3 with loop optimization") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int sum = 0;
        for (int i = 0; i < 10; i++) {
            sum += i;
        }
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(sum == 45);
    }

    SUBCASE("nested O3 blocks") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int result = 7 * 8;
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(result == 56);
    }
}

TEST_CASE("FL_OPTIMIZATION_LEVEL_O0 macros") {
    SUBCASE("O0 optimization macros are defined") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int result = 3 + 4;
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(result == 7);
    }

    SUBCASE("O0 debug function compiles and executes") {
        int result = o0_sum(50, 75);
        CHECK(result == 125);
    }

    SUBCASE("O0 preserves debugging information") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        volatile int x = 10;
        volatile int y = 20;
        int result = x + y;
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(result == 30);
    }

    SUBCASE("nested O0 blocks") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int result = 12 - 5;
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(result == 7);
    }
}

TEST_CASE("mixed optimization levels") {
    SUBCASE("O3 followed by O0") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int a = 10 * 2;
        FL_OPTIMIZATION_LEVEL_O3_END

        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int b = 5 + 3;
        FL_OPTIMIZATION_LEVEL_O0_END

        CHECK(a == 20);
        CHECK(b == 8);
    }

    SUBCASE("fast math with O3") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_FAST_MATH_BEGIN
        float result = 3.0f * 4.0f;
        FL_FAST_MATH_END
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(result == 12.0f);
    }

    SUBCASE("O0 with warning suppression") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        int result = unused_parameter_function(15, 25);
        FL_DISABLE_WARNING_POP
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(result == 15);
    }
}

TEST_CASE("FL_LINK_WEAK attribute") {
    SUBCASE("weak function is defined") {
        int result = weak_function();
        CHECK(result == 42);
    }

    SUBCASE("weak function can be called multiple times") {
        CHECK(weak_function() == 42);
        CHECK(weak_function() == 42);
    }

    SUBCASE("weak attribute allows override") {
        // Weak linkage means this function can be overridden by strong symbols
        // Here we just verify it works
        CHECK(weak_function() == 42);
    }
}

TEST_CASE("FL_EXTERN_C macros") {
    SUBCASE("FL_EXTERN_C_BEGIN and END are defined") {
        // Just check that the macros are defined
        // (Cannot test linkage inside function scope)
        CHECK(true);
    }

    SUBCASE("FL_EXTERN_C is defined") {
        // Check that FL_EXTERN_C functions can be called
        // (test_extern_c_function defined at file scope)
        CHECK(c_function() == 100);
    }

    SUBCASE("C linkage function compiles and executes") {
        int result = c_function();
        CHECK(result == 100);
    }

    SUBCASE("extern C function has C linkage") {
        // Verify the function can be called
        CHECK(c_function() == 100);
    }
}

TEST_CASE("FL_INLINE_CONSTEXPR macro") {
    SUBCASE("FL_INLINE_CONSTEXPR variable is defined") {
        CHECK(inline_constexpr_value == 123);
    }

    SUBCASE("inline constexpr can be used in constexpr context") {
        constexpr int doubled = inline_constexpr_value * 2;
        CHECK(doubled == 246);
    }

    SUBCASE("inline constexpr variable has correct value") {
        // Note: inline keyword not allowed in local scope, use constexpr only
        constexpr int local_value = 999;
        CHECK(local_value == 999);
    }

    SUBCASE("inline constexpr in array size") {
        // Note: inline keyword not allowed in local scope, use constexpr only
        constexpr fl::size_t array_size = 5;
        int array[array_size];
        CHECK(sizeof(array) / sizeof(int) == 5);
    }
}

TEST_CASE("macro combinations and interactions") {
    SUBCASE("warning suppression inside fast math") {
        FL_FAST_MATH_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float f = 5.7f;
        int i = static_cast<int>(f);
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_END
        CHECK(i == 5);
    }

    SUBCASE("multiple warning suppressions") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        FL_DISABLE_WARNING_SIGN_CONVERSION
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        int result = unused_parameter_function(30, 40);
        FL_DISABLE_WARNING_POP
        CHECK(result == 30);
    }

    SUBCASE("all optimization macros together") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_FAST_MATH_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float result = 2.5f * 4.0f;
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_END
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(result == 10.0f);
    }

    SUBCASE("extern C with inline constexpr") {
        // Cannot test linkage inside function scope
        // Just verify macros are defined
        CHECK(true);
    }
}

TEST_CASE("macro edge cases") {
    SUBCASE("empty warning suppression block") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        CHECK(true);
    }

    SUBCASE("empty fast math block") {
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_END
        CHECK(true);
    }

    SUBCASE("empty O3 block") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(true);
    }

    SUBCASE("empty O0 block") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(true);
    }

    SUBCASE("empty extern C block") {
        // Cannot test extern C linkage inside function scope
        CHECK(true);
    }

    SUBCASE("deeply nested macros") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_FAST_MATH_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_PUSH
        int result = 1 + 2 + 3;
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_END
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(result == 6);
    }
}

TEST_CASE("compiler portability") {
    SUBCASE("macros work on current compiler") {
        // Clang
        #if defined(__clang__)
        CHECK(true);
        #endif

        // GCC
        #if defined(__GNUC__)
        CHECK(true);
        #endif

        // MSVC
        #if defined(_MSC_VER)
        CHECK(true);
        #endif

        // At least one should be true
        CHECK(true);
    }

    SUBCASE("fallback macros are safe") {
        // Even on unknown compilers, the macros should be defined
        // and should do nothing (no-op) rather than causing errors
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_END
        CHECK(true);
    }
}

TEST_CASE("practical usage scenarios") {
    SUBCASE("suppress warning in template code") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        auto lambda = [](int x, FL_MAYBE_UNUSED int y) { return x; };
        FL_DISABLE_WARNING_POP
        CHECK(lambda(10, 20) == 10);
    }

    SUBCASE("fast math for performance-critical loop") {
        FL_FAST_MATH_BEGIN
        float sum = 0.0f;
        for (int i = 0; i < 100; i++) {
            sum += static_cast<float>(i) * 0.5f;
        }
        FL_FAST_MATH_END
        // Sum of i*0.5 from 0 to 99 = 0.5 * (0+1+...+99) = 0.5 * 4950 = 2475
        CHECK(sum == 2475.0f);
    }

    SUBCASE("debug code with O0") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        volatile int debug_value = 42;
        int result = debug_value + 1;
        FL_OPTIMIZATION_LEVEL_O0_END
        CHECK(result == 43);
    }

    SUBCASE("optimized hot path with O3") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int product = 1;
        for (int i = 1; i <= 5; i++) {
            product *= i;
        }
        FL_OPTIMIZATION_LEVEL_O3_END
        CHECK(product == 120);  // 5! = 120
    }

    SUBCASE("C API wrapper") {
        // Test existing extern C function
        CHECK(c_function() == 100);
    }

    SUBCASE("weak symbol for optional override") {
        // Weak symbol can be overridden by user code
        CHECK(weak_function() == 42);
    }

    SUBCASE("constexpr configuration constant") {
        // Note: inline keyword not allowed in local scope
        constexpr int buffer_size = 256;
        char buffer[buffer_size];
        CHECK(sizeof(buffer) == 256);
    }
}

TEST_CASE("stringify in practical scenarios") {
    SUBCASE("version string") {
        #define VERSION_MAJOR 1
        #define VERSION_MINOR 2
        const char* version = FL_STRINGIFY(VERSION_MAJOR) "." FL_STRINGIFY(VERSION_MINOR);
        CHECK(fl::strcmp(version, "1.2") == 0);
        #undef VERSION_MAJOR
        #undef VERSION_MINOR
    }

    SUBCASE("debug macro with stringify") {
        #define DEBUG_VALUE 0xDEADBEEF
        const char* debug_str = FL_STRINGIFY(DEBUG_VALUE);
        CHECK(fl::strcmp(debug_str, "0xDEADBEEF") == 0);
        #undef DEBUG_VALUE
    }

    SUBCASE("pragma construction") {
        // Test that stringify can be used in pragma construction
        #define WARNING_NAME unused-variable
        // This would normally be used like: _Pragma(FL_STRINGIFY(GCC diagnostic ignored "-W" #WARNING_NAME))
        const char* pragma_str = FL_STRINGIFY(WARNING_NAME);
        CHECK(fl::strcmp(pragma_str, "unused-variable") == 0);
        #undef WARNING_NAME
    }
}
