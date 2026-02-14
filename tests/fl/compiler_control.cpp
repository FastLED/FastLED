#include "fl/compiler_control.h"
#include "test.h"
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

FL_TEST_CASE("FL_STRINGIFY macros") {
    FL_SUBCASE("FL_STRINGIFY expands macro argument") {
        #define TEST_VALUE 42
        const char* str = FL_STRINGIFY(TEST_VALUE);
        FL_CHECK(fl::strcmp(str, "42") == 0);
        #undef TEST_VALUE
    }

    FL_SUBCASE("FL_STRINGIFY2 converts to string literal") {
        const char* str = FL_STRINGIFY2(hello);
        FL_CHECK(fl::strcmp(str, "hello") == 0);
    }

    FL_SUBCASE("FL_STRINGIFY handles expressions") {
        const char* str = FL_STRINGIFY(1 + 1);
        FL_CHECK(fl::strcmp(str, "1 + 1") == 0);
    }
}

FL_TEST_CASE("FL_DISABLE_WARNING macros are defined") {
    FL_SUBCASE("FL_DISABLE_WARNING_PUSH is defined") {
        // Just check that the macro is defined and compiles
        FL_DISABLE_WARNING_PUSH
        // Should compile without error
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_POP is defined") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING is defined") {
        // Test that the macro is defined and can be used
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(unused-variable)
        int unused_var = 0;
        (void)unused_var;
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FL_DISABLE_WARNING_PUSH and POP are balanced") {
    FL_SUBCASE("single push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("nested push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("multiple sequential push-pop") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }
}

FL_TEST_CASE("specific warning suppression macros") {
    FL_SUBCASE("FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_GLOBAL_CONSTRUCTORS
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SELF_ASSIGN
        FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
        int x = 5;
        x = x;  // Self-assignment
        FL_DISABLE_WARNING_POP
        FL_CHECK(x == 5);
    }

    FL_SUBCASE("FL_DISABLE_FORMAT_TRUNCATION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_FORMAT_TRUNCATION
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_NULL_DEREFERENCE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_NULL_DEREFERENCE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_UNUSED_PARAMETER") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        int result = unused_parameter_function(10, 20);
        FL_DISABLE_WARNING_POP
        FL_CHECK(result == 10);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_RETURN_TYPE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_RETURN_TYPE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_IMPLICIT_FALLTHROUGH
        int result = fallthrough_function(1);
        FL_DISABLE_WARNING_POP
        FL_CHECK(result == 20);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_IMPLICIT_INT_CONVERSION
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_FLOAT_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float f = 3.14f;
        int i = static_cast<int>(f);
        FL_DISABLE_WARNING_POP
        FL_CHECK(i == 3);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_SIGN_CONVERSION") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SIGN_CONVERSION
        unsigned int u = 10;
        int s = static_cast<int>(u);
        FL_DISABLE_WARNING_POP
        FL_CHECK(s == 10);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_SHORTEN_64_TO_32") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_SHORTEN_64_TO_32
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_DISABLE_WARNING_VOLATILE") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_VOLATILE
        // Should compile without warning
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }
}

FL_TEST_CASE("FL_FAST_MATH macros") {
    FL_SUBCASE("FL_FAST_MATH_BEGIN and END are defined") {
        FL_FAST_MATH_BEGIN
        float result = 2.0f * 3.0f;
        FL_FAST_MATH_END
        FL_CHECK(result == 6.0f);
    }

    FL_SUBCASE("fast math function compiles and executes") {
        float result = fast_math_multiply(2.5f, 4.0f);
        FL_CHECK(result == 10.0f);
    }

    FL_SUBCASE("fast math with multiple operations") {
        FL_FAST_MATH_BEGIN
        float a = 1.5f;
        float b = 2.0f;
        float c = 3.0f;
        float result = (a * b) + (b * c);
        FL_FAST_MATH_END
        FL_CHECK(result == 9.0f);
    }

    FL_SUBCASE("nested fast math blocks") {
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_BEGIN
        float result = 10.0f / 2.0f;
        FL_FAST_MATH_END
        FL_FAST_MATH_END
        FL_CHECK(result == 5.0f);
    }
}

FL_TEST_CASE("FL_OPTIMIZATION_LEVEL_O3 macros") {
    FL_SUBCASE("O3 optimization macros are defined") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int result = 5 + 10;
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(result == 15);
    }

    FL_SUBCASE("O3 optimized function compiles and executes") {
        int result = o3_sum(100, 200);
        FL_CHECK(result == 300);
    }

    FL_SUBCASE("O3 with loop optimization") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int sum = 0;
        for (int i = 0; i < 10; i++) {
            sum += i;
        }
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(sum == 45);
    }

    FL_SUBCASE("nested O3 blocks") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int result = 7 * 8;
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(result == 56);
    }
}

FL_TEST_CASE("FL_OPTIMIZATION_LEVEL_O0 macros") {
    FL_SUBCASE("O0 optimization macros are defined") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int result = 3 + 4;
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(result == 7);
    }

    FL_SUBCASE("O0 debug function compiles and executes") {
        int result = o0_sum(50, 75);
        FL_CHECK(result == 125);
    }

    FL_SUBCASE("O0 preserves debugging information") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        volatile int x = 10;
        volatile int y = 20;
        int result = x + y;
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(result == 30);
    }

    FL_SUBCASE("nested O0 blocks") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int result = 12 - 5;
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(result == 7);
    }
}

FL_TEST_CASE("mixed optimization levels") {
    FL_SUBCASE("O3 followed by O0") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int a = 10 * 2;
        FL_OPTIMIZATION_LEVEL_O3_END

        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        int b = 5 + 3;
        FL_OPTIMIZATION_LEVEL_O0_END

        FL_CHECK(a == 20);
        FL_CHECK(b == 8);
    }

    FL_SUBCASE("fast math with O3") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_FAST_MATH_BEGIN
        float result = 3.0f * 4.0f;
        FL_FAST_MATH_END
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(result == 12.0f);
    }

    FL_SUBCASE("O0 with warning suppression") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        int result = unused_parameter_function(15, 25);
        FL_DISABLE_WARNING_POP
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(result == 15);
    }
}

FL_TEST_CASE("FL_LINK_WEAK attribute") {
    FL_SUBCASE("weak function is defined") {
        int result = weak_function();
        FL_CHECK(result == 42);
    }

    FL_SUBCASE("weak function can be called multiple times") {
        FL_CHECK(weak_function() == 42);
        FL_CHECK(weak_function() == 42);
    }

    FL_SUBCASE("weak attribute allows override") {
        // Weak linkage means this function can be overridden by strong symbols
        // Here we just verify it works
        FL_CHECK(weak_function() == 42);
    }
}

FL_TEST_CASE("FL_EXTERN_C macros") {
    FL_SUBCASE("FL_EXTERN_C_BEGIN and END are defined") {
        // Just check that the macros are defined
        // (Cannot test linkage inside function scope)
        FL_CHECK(true);
    }

    FL_SUBCASE("FL_EXTERN_C is defined") {
        // Check that FL_EXTERN_C functions can be called
        // (test_extern_c_function defined at file scope)
        FL_CHECK(c_function() == 100);
    }

    FL_SUBCASE("C linkage function compiles and executes") {
        int result = c_function();
        FL_CHECK(result == 100);
    }

    FL_SUBCASE("extern C function has C linkage") {
        // Verify the function can be called
        FL_CHECK(c_function() == 100);
    }
}

FL_TEST_CASE("FL_INLINE_CONSTEXPR macro") {
    FL_SUBCASE("FL_INLINE_CONSTEXPR variable is defined") {
        FL_CHECK(inline_constexpr_value == 123);
    }

    FL_SUBCASE("inline constexpr can be used in constexpr context") {
        constexpr int doubled = inline_constexpr_value * 2;
        FL_CHECK(doubled == 246);
    }

    FL_SUBCASE("inline constexpr variable has correct value") {
        // Note: inline keyword not allowed in local scope, use constexpr only
        constexpr int local_value = 999;
        FL_CHECK(local_value == 999);
    }

    FL_SUBCASE("inline constexpr in array size") {
        // Note: inline keyword not allowed in local scope, use constexpr only
        constexpr fl::size_t array_size = 5;
        int array[array_size];
        FL_CHECK(sizeof(array) / sizeof(int) == 5);
    }
}

FL_TEST_CASE("macro combinations and interactions") {
    FL_SUBCASE("warning suppression inside fast math") {
        FL_FAST_MATH_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float f = 5.7f;
        int i = static_cast<int>(f);
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_END
        FL_CHECK(i == 5);
    }

    FL_SUBCASE("multiple warning suppressions") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        FL_DISABLE_WARNING_SIGN_CONVERSION
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        int result = unused_parameter_function(30, 40);
        FL_DISABLE_WARNING_POP
        FL_CHECK(result == 30);
    }

    FL_SUBCASE("all optimization macros together") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_FAST_MATH_BEGIN
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_FLOAT_CONVERSION
        float result = 2.5f * 4.0f;
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_END
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(result == 10.0f);
    }

    FL_SUBCASE("extern C with inline constexpr") {
        // Cannot test linkage inside function scope
        // Just verify macros are defined
        FL_CHECK(true);
    }
}

FL_TEST_CASE("macro edge cases") {
    FL_SUBCASE("empty warning suppression block") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_CHECK(true);
    }

    FL_SUBCASE("empty fast math block") {
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_END
        FL_CHECK(true);
    }

    FL_SUBCASE("empty O3 block") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(true);
    }

    FL_SUBCASE("empty O0 block") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(true);
    }

    FL_SUBCASE("empty extern C block") {
        // Cannot test extern C linkage inside function scope
        FL_CHECK(true);
    }

    FL_SUBCASE("deeply nested macros") {
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
        FL_CHECK(result == 6);
    }
}

FL_TEST_CASE("compiler portability") {
    FL_SUBCASE("macros work on current compiler") {
        // Clang
        #if defined(__clang__)
        FL_CHECK(true);
        #endif

        // GCC
        #if defined(__GNUC__)
        FL_CHECK(true);
        #endif

        // MSVC
        #if defined(_MSC_VER)
        FL_CHECK(true);
        #endif

        // At least one should be true
        FL_CHECK(true);
    }

    FL_SUBCASE("fallback macros are safe") {
        // Even on unknown compilers, the macros should be defined
        // and should do nothing (no-op) rather than causing errors
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_POP
        FL_FAST_MATH_BEGIN
        FL_FAST_MATH_END
        FL_CHECK(true);
    }
}

FL_TEST_CASE("practical usage scenarios") {
    FL_SUBCASE("suppress warning in template code") {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING_UNUSED_PARAMETER
        auto lambda = [](int x, FL_MAYBE_UNUSED int y) { return x; };
        FL_DISABLE_WARNING_POP
        FL_CHECK(lambda(10, 20) == 10);
    }

    FL_SUBCASE("fast math for performance-critical loop") {
        FL_FAST_MATH_BEGIN
        float sum = 0.0f;
        for (int i = 0; i < 100; i++) {
            sum += static_cast<float>(i) * 0.5f;
        }
        FL_FAST_MATH_END
        // Sum of i*0.5 from 0 to 99 = 0.5 * (0+1+...+99) = 0.5 * 4950 = 2475
        FL_CHECK(sum == 2475.0f);
    }

    FL_SUBCASE("debug code with O0") {
        FL_OPTIMIZATION_LEVEL_O0_BEGIN
        volatile int debug_value = 42;
        int result = debug_value + 1;
        FL_OPTIMIZATION_LEVEL_O0_END
        FL_CHECK(result == 43);
    }

    FL_SUBCASE("optimized hot path with O3") {
        FL_OPTIMIZATION_LEVEL_O3_BEGIN
        int product = 1;
        for (int i = 1; i <= 5; i++) {
            product *= i;
        }
        FL_OPTIMIZATION_LEVEL_O3_END
        FL_CHECK(product == 120);  // 5! = 120
    }

    FL_SUBCASE("C API wrapper") {
        // Test existing extern C function
        FL_CHECK(c_function() == 100);
    }

    FL_SUBCASE("weak symbol for optional override") {
        // Weak symbol can be overridden by user code
        FL_CHECK(weak_function() == 42);
    }

    FL_SUBCASE("constexpr configuration constant") {
        // Note: inline keyword not allowed in local scope
        constexpr int buffer_size = 256;
        char buffer[buffer_size];
        FL_CHECK(sizeof(buffer) == 256);
    }
}

FL_TEST_CASE("stringify in practical scenarios") {
    FL_SUBCASE("version string") {
        #define VERSION_MAJOR 1
        #define VERSION_MINOR 2
        const char* version = FL_STRINGIFY(VERSION_MAJOR) "." FL_STRINGIFY(VERSION_MINOR);
        FL_CHECK(fl::strcmp(version, "1.2") == 0);
        #undef VERSION_MAJOR
        #undef VERSION_MINOR
    }

    FL_SUBCASE("debug macro with stringify") {
        #define DEBUG_VALUE 0xDEADBEEF
        const char* debug_str = FL_STRINGIFY(DEBUG_VALUE);
        FL_CHECK(fl::strcmp(debug_str, "0xDEADBEEF") == 0);
        #undef DEBUG_VALUE
    }

    FL_SUBCASE("pragma construction") {
        // Test that stringify can be used in pragma construction
        #define WARNING_NAME unused-variable
        // This would normally be used like: _Pragma(FL_STRINGIFY(GCC diagnostic ignored "-W" #WARNING_NAME))
        const char* pragma_str = FL_STRINGIFY(WARNING_NAME);
        FL_CHECK(fl::strcmp(pragma_str, "unused-variable") == 0);
        #undef WARNING_NAME
    }
}

// ============================================================================
// DEPRECATION MACROS
// ============================================================================

// Test that deprecated macros are properly defined
FL_TEST_CASE("fl::deprecated_macros_defined") {
    // These macros should be defined on all platforms
    #ifndef FASTLED_DEPRECATED
    FL_FAIL("FASTLED_DEPRECATED is not defined");
    #endif

    #ifndef FASTLED_DEPRECATED_CLASS
    FL_FAIL("FASTLED_DEPRECATED_CLASS is not defined");
    #endif

    #ifndef FL_DEPRECATED
    FL_FAIL("FL_DEPRECATED is not defined");
    #endif

    #ifndef FL_DEPRECATED_CLASS
    FL_FAIL("FL_DEPRECATED_CLASS is not defined");
    #endif

    // If we get here, all macros are defined
    FL_CHECK(true);
}

// Test that deprecated function macro can be applied
FL_DEPRECATED("This is a test deprecated function")
inline int deprecated_test_function() {
    return 42;
}

FL_TEST_CASE("fl::deprecated_function_usage") {
    // Function should still work even if deprecated
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    int result = deprecated_test_function();
    FL_CHECK_EQ(result, 42);
    FL_DISABLE_WARNING_POP
}

// Test that deprecated class macro can be applied
class FL_DEPRECATED_CLASS("This is a test deprecated class")
 DeprecatedTestClass {
public:
    int value;
    DeprecatedTestClass() : value(100) {}
    int getValue() const { return value; }
};

FL_TEST_CASE("fl::deprecated_class_usage") {
    // Class should still work even if deprecated
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    DeprecatedTestClass obj;
    FL_CHECK_EQ(obj.getValue(), 100);

    obj.value = 200;
    FL_CHECK_EQ(obj.getValue(), 200);
}

// Test deprecated method in non-deprecated class
class TestClassWithDeprecatedMethod {
public:
    FL_DEPRECATED("Use newMethod() instead")
    int oldMethod() {
        return 1;
    }

    int newMethod() {
        return 2;
    }
};

FL_TEST_CASE("fl::deprecated_method_usage") {
    TestClassWithDeprecatedMethod obj;
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS

    // Old method should still work
    FL_CHECK_EQ(obj.oldMethod(), 1);

    FL_DISABLE_WARNING_POP
    // New method should work
    FL_CHECK_EQ(obj.newMethod(), 2);
}

// Test that FASTLED_DEPRECATED and FL_DEPRECATED are equivalent
FL_DEPRECATED("FL_DEPRECATED version")
inline int deprecated_fl() {
    return 1;
}

FASTLED_DEPRECATED("FASTLED_DEPRECATED version")
inline int deprecated_fastled() {
    return 2;
}

FL_TEST_CASE("fl::deprecated_macro_equivalence") {
    // Both versions should work
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    FL_CHECK_EQ(deprecated_fl(), 1);
    FL_CHECK_EQ(deprecated_fastled(), 2);
}

// Test struct deprecation
struct FL_DEPRECATED_CLASS("Deprecated struct")
DeprecatedTestStruct {
    int x;
    int y;
};

FL_TEST_CASE("fl::deprecated_struct_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    DeprecatedTestStruct s;
    s.x = 10;
    s.y = 20;
    FL_CHECK_EQ(s.x, 10);
    FL_CHECK_EQ(s.y, 20);
    FL_DISABLE_WARNING_POP
}

// Test deprecated typedef
FL_DEPRECATED("Use int instead")
typedef int deprecated_int_type;

FL_TEST_CASE("fl::deprecated_typedef_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    deprecated_int_type value = 42;
    FL_CHECK_EQ(value, 42);
    FL_DISABLE_WARNING_POP
}

// Test deprecated variable
FL_DEPRECATED("Use new_constant instead")
static const int OLD_CONSTANT = 100;

static const int NEW_CONSTANT = 200;

FL_TEST_CASE("fl::deprecated_variable_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    FL_CHECK_EQ(OLD_CONSTANT, 100);
    FL_DISABLE_WARNING_POP
    FL_CHECK_EQ(NEW_CONSTANT, 200);
}

// Test deprecated template function
template<typename T>
FL_DEPRECATED("Use newTemplateFunction instead")
T oldTemplateFunction(T value) {
    return value * 2;
}

template<typename T>
T newTemplateFunction(T value) {
    return value * 3;
}

FL_TEST_CASE("fl::deprecated_template_function") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    FL_CHECK_EQ(oldTemplateFunction(5), 10);
    FL_CHECK_EQ(oldTemplateFunction(3.0), 6.0);
    FL_DISABLE_WARNING_POP

    FL_CHECK_EQ(newTemplateFunction(5), 15);
    FL_CHECK_EQ(newTemplateFunction(3.0), 9.0);
}
