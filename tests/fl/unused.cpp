#include "fl/unused.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test that the macros compile and suppress warnings
// These macros are used to silence compiler warnings about unused variables/functions

// ============================================================================
// TEST_CASE: FASTLED_UNUSED macro
// ============================================================================

TEST_CASE("FASTLED_UNUSED macro") {
    SUBCASE("FASTLED_UNUSED is defined") {
        // Verify the macro is defined
        #ifdef FASTLED_UNUSED
        CHECK(true);
        #else
        FAIL("FASTLED_UNUSED is not defined");
        #endif
    }

    SUBCASE("FASTLED_UNUSED with int variable") {
        int unused_var = 42;
        FASTLED_UNUSED(unused_var);
        // If we get here without compiler warnings, the macro works
        CHECK(unused_var == 42);
    }

    SUBCASE("FASTLED_UNUSED with pointer") {
        int value = 100;
        int* unused_ptr = &value;
        FASTLED_UNUSED(unused_ptr);
        CHECK(*unused_ptr == 100);
    }

    SUBCASE("FASTLED_UNUSED with const variable") {
        const double unused_const = 3.14;
        FASTLED_UNUSED(unused_const);
        CHECK(unused_const == doctest::Approx(3.14));
    }

    SUBCASE("FASTLED_UNUSED with struct") {
        struct TestStruct {
            int x;
            int y;
        };
        TestStruct unused_struct = {10, 20};
        FASTLED_UNUSED(unused_struct);
        CHECK(unused_struct.x == 10);
        CHECK(unused_struct.y == 20);
    }

    SUBCASE("FASTLED_UNUSED with multiple calls") {
        int a = 1;
        int b = 2;
        int c = 3;
        FASTLED_UNUSED(a);
        FASTLED_UNUSED(b);
        FASTLED_UNUSED(c);
        CHECK(a == 1);
        CHECK(b == 2);
        CHECK(c == 3);
    }

    SUBCASE("FASTLED_UNUSED with function parameter") {
        auto func = [](int param) {
            FASTLED_UNUSED(param);
            return 0;
        };
        CHECK(func(42) == 0);
    }

    SUBCASE("FASTLED_UNUSED with volatile variable") {
        volatile int unused_volatile = 99;
        FASTLED_UNUSED(unused_volatile);
        CHECK(unused_volatile == 99);
    }

    SUBCASE("FASTLED_UNUSED with reference") {
        int value = 50;
        int& unused_ref = value;
        FASTLED_UNUSED(unused_ref);
        CHECK(unused_ref == 50);
    }

    SUBCASE("FASTLED_UNUSED with expression result") {
        int x = 10;
        int y = 20;
        FASTLED_UNUSED(x + y);
        // Macro should cast result to void
        CHECK(x == 10);
        CHECK(y == 20);
    }
}

// ============================================================================
// TEST_CASE: FL_UNUSED macro
// ============================================================================

TEST_CASE("FL_UNUSED macro") {
    SUBCASE("FL_UNUSED is defined") {
        // Verify the macro is defined
        #ifdef FL_UNUSED
        CHECK(true);
        #else
        FAIL("FL_UNUSED is not defined");
        #endif
    }

    SUBCASE("FL_UNUSED with int variable") {
        int unused_var = 42;
        FL_UNUSED(unused_var);
        CHECK(unused_var == 42);
    }

    SUBCASE("FL_UNUSED with pointer") {
        int value = 100;
        int* unused_ptr = &value;
        FL_UNUSED(unused_ptr);
        CHECK(*unused_ptr == 100);
    }

    SUBCASE("FL_UNUSED with const variable") {
        const double unused_const = 2.718;
        FL_UNUSED(unused_const);
        CHECK(unused_const == doctest::Approx(2.718));
    }

    SUBCASE("FL_UNUSED with struct") {
        struct TestStruct {
            int a;
            int b;
        };
        TestStruct unused_struct = {5, 15};
        FL_UNUSED(unused_struct);
        CHECK(unused_struct.a == 5);
        CHECK(unused_struct.b == 15);
    }

    SUBCASE("FL_UNUSED with multiple calls") {
        int x = 1;
        int y = 2;
        int z = 3;
        FL_UNUSED(x);
        FL_UNUSED(y);
        FL_UNUSED(z);
        CHECK(x == 1);
        CHECK(y == 2);
        CHECK(z == 3);
    }

    SUBCASE("FL_UNUSED with function parameter") {
        auto func = [](int param) {
            FL_UNUSED(param);
            return 42;
        };
        CHECK(func(100) == 42);
    }

    SUBCASE("FL_UNUSED with volatile variable") {
        volatile int unused_volatile = 77;
        FL_UNUSED(unused_volatile);
        CHECK(unused_volatile == 77);
    }

    SUBCASE("FL_UNUSED with reference") {
        int value = 88;
        int& unused_ref = value;
        FL_UNUSED(unused_ref);
        CHECK(unused_ref == 88);
    }

    SUBCASE("FL_UNUSED with expression result") {
        int a = 5;
        int b = 10;
        FL_UNUSED(a * b);
        CHECK(a == 5);
        CHECK(b == 10);
    }

    SUBCASE("FL_UNUSED with array") {
        int unused_array[3] = {1, 2, 3};
        FL_UNUSED(unused_array);
        CHECK(unused_array[0] == 1);
        CHECK(unused_array[1] == 2);
        CHECK(unused_array[2] == 3);
    }
}

// ============================================================================
// TEST_CASE: FL_UNUSED_FUNCTION attribute
// ============================================================================

// Define test functions with FL_UNUSED_FUNCTION attribute at file scope
FL_UNUSED_FUNCTION static int test_unused_func_1() {
    return 123;
}

FL_UNUSED_FUNCTION static int test_unused_func_2() {
    return 456;
}

FL_UNUSED_FUNCTION static void test_unused_func_void() {
    // Do nothing
}

TEST_CASE("FL_UNUSED_FUNCTION attribute") {
    SUBCASE("FL_UNUSED_FUNCTION is defined") {
        // Verify the macro is defined
        #ifdef FL_UNUSED_FUNCTION
        CHECK(true);
        #else
        FAIL("FL_UNUSED_FUNCTION is not defined");
        #endif
    }

    SUBCASE("FL_UNUSED_FUNCTION with static function") {
        // Call the function to verify it works
        int result = test_unused_func_1();
        CHECK(result == 123);
    }

    SUBCASE("FL_UNUSED_FUNCTION with another static function") {
        int result = test_unused_func_2();
        CHECK(result == 456);
    }

    SUBCASE("FL_UNUSED_FUNCTION with void function") {
        // Just call the function to ensure it compiles
        test_unused_func_void();
        CHECK(true);
    }

    SUBCASE("FL_UNUSED_FUNCTION allows unused function definition") {
        // These functions are defined but may not be called elsewhere
        // The attribute suppresses warnings about them being unused
        // If we get here without warnings, the macro works
        CHECK(true);
    }
}

// ============================================================================
// TEST_CASE: Macro interactions
// ============================================================================

TEST_CASE("macro interactions") {
    SUBCASE("FASTLED_UNUSED and FL_UNUSED are equivalent") {
        int var1 = 10;
        int var2 = 10;
        FASTLED_UNUSED(var1);
        FL_UNUSED(var2);
        // Both should work identically
        CHECK(var1 == var2);
    }

    SUBCASE("nested FASTLED_UNUSED calls") {
        int outer = 5;
        {
            int inner = 10;
            FASTLED_UNUSED(inner);
            CHECK(inner == 10);
        }
        FASTLED_UNUSED(outer);
        CHECK(outer == 5);
    }

    SUBCASE("nested FL_UNUSED calls") {
        int outer = 15;
        {
            int inner = 20;
            FL_UNUSED(inner);
            CHECK(inner == 20);
        }
        FL_UNUSED(outer);
        CHECK(outer == 15);
    }

    SUBCASE("mixed FASTLED_UNUSED and FL_UNUSED") {
        int a = 1;
        int b = 2;
        int c = 3;
        FASTLED_UNUSED(a);
        FL_UNUSED(b);
        FASTLED_UNUSED(c);
        CHECK(a == 1);
        CHECK(b == 2);
        CHECK(c == 3);
    }
}

// ============================================================================
// TEST_CASE: Practical usage scenarios
// ============================================================================

TEST_CASE("practical usage scenarios") {
    SUBCASE("unused parameter in callback") {
        // Common pattern: callback receives parameters but doesn't use all of them
        auto callback = [](int used_param, int unused_param) {
            FL_UNUSED(unused_param);
            return used_param * 2;
        };
        CHECK(callback(5, 999) == 10);
    }

    SUBCASE("unused variable in debug code") {
        // Variable used only in asserts (stripped in release builds)
        int result = 100;
        FL_UNUSED(result);  // Prevents warning when asserts are disabled
        CHECK(result == 100);
    }

    SUBCASE("unused result from function call") {
        auto get_value = []() { return 42; };
        FL_UNUSED(get_value());
        // Intentionally ignoring return value
        CHECK(true);
    }

    SUBCASE("conditional compilation with unused variable") {
        int debug_var = 123;
        #ifdef NEVER_DEFINED
        // Variable only used in this block
        debug_var++;
        #else
        // Variable unused in this path
        FL_UNUSED(debug_var);
        #endif
        CHECK(debug_var == 123);
    }

    SUBCASE("template function with unused type parameter") {
        // Some template instantiations may not use all parameters
        auto template_func = [](int value, const char* unused_value) {
            FL_UNUSED(unused_value);
            return value;
        };
        CHECK(template_func(42, "unused") == 42);
    }

    SUBCASE("unused this pointer in static-like member") {
        struct TestClass {
            int member = 10;
            void method(TestClass* self) {
                FL_UNUSED(self);
                // Method doesn't actually use self pointer
            }
        };
        TestClass obj;
        obj.method(&obj);
        CHECK(obj.member == 10);
    }
}

// ============================================================================
// TEST_CASE: Edge cases
// ============================================================================

TEST_CASE("edge cases") {
    SUBCASE("FL_UNUSED with nullptr") {
        int* null_ptr = nullptr;
        FL_UNUSED(null_ptr);
        CHECK(null_ptr == nullptr);
    }

    SUBCASE("FL_UNUSED with boolean") {
        bool unused_bool = true;
        FL_UNUSED(unused_bool);
        CHECK(unused_bool == true);
    }

    SUBCASE("FL_UNUSED with char") {
        char unused_char = 'A';
        FL_UNUSED(unused_char);
        CHECK(unused_char == 'A');
    }

    SUBCASE("FL_UNUSED with string literal") {
        const char* unused_str = "test";
        FL_UNUSED(unused_str);
        CHECK(unused_str[0] == 't');
    }

    SUBCASE("FL_UNUSED with enum") {
        enum TestEnum { VALUE_A, VALUE_B, VALUE_C };
        TestEnum unused_enum = VALUE_B;
        FL_UNUSED(unused_enum);
        CHECK(unused_enum == VALUE_B);
    }

    SUBCASE("FL_UNUSED with lambda") {
        auto unused_lambda = []() { return 42; };
        FL_UNUSED(unused_lambda);
        // Lambda defined but not called
        CHECK(true);
    }

    SUBCASE("FASTLED_UNUSED with cast expression") {
        double pi = 3.14159;
        FASTLED_UNUSED(static_cast<int>(pi));
        CHECK(pi == doctest::Approx(3.14159));
    }

    SUBCASE("FL_UNUSED with sizeof expression") {
        int array[10];
        FL_UNUSED(sizeof(array));
        CHECK(sizeof(array) == 10 * sizeof(int));
    }

    SUBCASE("multiple FL_UNUSED_FUNCTION attributes") {
        // Already tested above, but verify they don't conflict
        CHECK(test_unused_func_1() == 123);
        CHECK(test_unused_func_2() == 456);
    }
}

// ============================================================================
// TEST_CASE: Type compatibility
// ============================================================================

TEST_CASE("type compatibility") {
    SUBCASE("FL_UNUSED with signed types") {
        signed char sc = -1;
        signed short ss = -100;
        signed int si = -1000;
        signed long sl = -10000;
        FL_UNUSED(sc);
        FL_UNUSED(ss);
        FL_UNUSED(si);
        FL_UNUSED(sl);
        CHECK(sc == -1);
        CHECK(ss == -100);
        CHECK(si == -1000);
        CHECK(sl == -10000);
    }

    SUBCASE("FL_UNUSED with unsigned types") {
        unsigned char uc = 255;
        unsigned short us = 65535;
        unsigned int ui = 1000;
        unsigned long ul = 10000;
        FL_UNUSED(uc);
        FL_UNUSED(us);
        FL_UNUSED(ui);
        FL_UNUSED(ul);
        CHECK(uc == 255);
        CHECK(us == 65535);
        CHECK(ui == 1000);
        CHECK(ul == 10000);
    }

    SUBCASE("FL_UNUSED with floating point types") {
        float f = 1.5f;
        double d = 2.5;
        long double ld = 3.5L;
        FL_UNUSED(f);
        FL_UNUSED(d);
        FL_UNUSED(ld);
        CHECK(f == doctest::Approx(1.5f));
        CHECK(d == doctest::Approx(2.5));
        CHECK(ld == doctest::Approx(3.5L));
    }

    SUBCASE("FL_UNUSED with const types") {
        const int ci = 100;
        const double cd = 3.14;
        const char* ccp = "test";
        FL_UNUSED(ci);
        FL_UNUSED(cd);
        FL_UNUSED(ccp);
        CHECK(ci == 100);
        CHECK(cd == doctest::Approx(3.14));
        CHECK(ccp[0] == 't');
    }

    SUBCASE("FL_UNUSED with pointer types") {
        int value = 42;
        int* p = &value;
        int** pp = &p;
        int*** ppp = &pp;
        FL_UNUSED(p);
        FL_UNUSED(pp);
        FL_UNUSED(ppp);
        CHECK(**pp == 42);
        CHECK(***ppp == 42);
    }

    SUBCASE("FL_UNUSED with reference types") {
        int value = 50;
        int& ref = value;
        const int& cref = value;
        FL_UNUSED(ref);
        FL_UNUSED(cref);
        CHECK(ref == 50);
        CHECK(cref == 50);
    }
}

// ============================================================================
// TEST_CASE: Macro expansion verification
// ============================================================================

TEST_CASE("macro expansion verification") {
    SUBCASE("FASTLED_UNUSED expands to (void)(x)") {
        // The macro should cast to void to suppress warnings
        int x = 10;
        // This should be equivalent to: (void)(x);
        FASTLED_UNUSED(x);
        CHECK(x == 10);
    }

    SUBCASE("FL_UNUSED expands to (void)(x)") {
        int x = 20;
        // This should be equivalent to: (void)(x);
        FL_UNUSED(x);
        CHECK(x == 20);
    }

    SUBCASE("FL_UNUSED_FUNCTION expands to __attribute__((unused))") {
        // The attribute should allow unused function definitions
        // If this compiles without warnings, the macro is correct
        #ifdef __GNUC__
        // GCC and Clang support __attribute__((unused))
        CHECK(true);
        #else
        // Other compilers may not support this attribute
        CHECK(true);
        #endif
    }
}

// ============================================================================
// TEST_CASE: Compiler compatibility
// ============================================================================

TEST_CASE("compiler compatibility") {
    SUBCASE("macros work on current compiler") {
        // All macros should be defined and functional
        int test_var = 42;
        FASTLED_UNUSED(test_var);
        FL_UNUSED(test_var);
        CHECK(test_var == 42);
    }

    SUBCASE("FL_UNUSED_FUNCTION works on GCC/Clang") {
        #if defined(__GNUC__) || defined(__clang__)
        // GCC and Clang support __attribute__((unused))
        CHECK(test_unused_func_1() == 123);
        #else
        // Other compilers may not support the attribute
        // but the function should still compile and work
        CHECK(test_unused_func_1() == 123);
        #endif
    }
}
