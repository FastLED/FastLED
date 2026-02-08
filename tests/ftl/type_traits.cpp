#include "fl/stl/type_traits.h"
#include "test.h"
#include "fl/int.h"
#include "fl/stl/move.h"

using namespace fl;

// Helper macro to avoid ODR-use of static constexpr enum values
// Uses variadic args to handle commas in template parameters
#define FL_CHECK_TRAIT(...) FL_CHECK(bool(__VA_ARGS__))

// Helper class for testing
struct Base {};
struct Derived : Base {};
struct Unrelated {};

// Test class with swap method
struct SwappableClass {
    int value;
    SwappableClass(int v = 0) : value(v) {}
    void swap(SwappableClass& other) {
        int tmp = value;
        value = other.value;
        other.value = tmp;
    }
};

FL_TEST_CASE("fl::integral_constant") {
    FL_SUBCASE("true_type") {
        FL_CHECK(bool(true_type::value));
        FL_CHECK_TRAIT(integral_constant<bool, true>::value);
    }

    FL_SUBCASE("false_type") {
        FL_CHECK(!bool(false_type::value));
        FL_CHECK_TRAIT(!integral_constant<bool, false>::value);
    }

    FL_SUBCASE("integer constants") {
        FL_CHECK((int(integral_constant<int, 42>::value) == 42));
        FL_CHECK((int(integral_constant<int, -5>::value) == -5));
    }

    FL_SUBCASE("constexpr operators") {
        constexpr bool t = true_type();
        constexpr bool f = false_type();
        FL_CHECK_EQ(t, true);
        FL_CHECK_EQ(f, false);
    }
}

FL_TEST_CASE("fl::identity") {
    FL_SUBCASE("preserves type") {
        static_assert(is_same<identity<int>::type, int>::value, "identity should preserve int");
        static_assert(is_same<identity<float>::type, float>::value, "identity should preserve float");
        static_assert(is_same<identity<Base>::type, Base>::value, "identity should preserve class");
    }
}

FL_TEST_CASE("fl::add_rvalue_reference") {
    FL_SUBCASE("non-reference types") {
        static_assert(is_same<add_rvalue_reference<int>::type, int&&>::value, "should add rvalue ref to int");
        static_assert(is_same<add_rvalue_reference<float>::type, float&&>::value, "should add rvalue ref to float");
    }

    FL_SUBCASE("lvalue reference types") {
        static_assert(is_same<add_rvalue_reference<int&>::type, int&>::value, "should preserve lvalue ref");
        static_assert(is_same<add_rvalue_reference<float&>::type, float&>::value, "should preserve lvalue ref");
    }
}

FL_TEST_CASE("fl::enable_if") {
    FL_SUBCASE("true condition") {
        static_assert(is_same<enable_if<true, int>::type, int>::value, "enable_if true should provide type");
        static_assert(is_same<enable_if_t<true, int>, int>::value, "enable_if_t true should provide type");
    }

    FL_SUBCASE("default type") {
        static_assert(is_same<enable_if<true>::type, void>::value, "enable_if default should be void");
    }
}

FL_TEST_CASE("fl::is_base_of") {
    FL_SUBCASE("inheritance relationship") {
        FL_CHECK_TRAIT(is_base_of<Base, Derived>::value);
        FL_CHECK_TRAIT(is_base_of_v_helper<Base, Derived>::value);
    }

    FL_SUBCASE("no inheritance relationship") {
        FL_CHECK_TRAIT(!is_base_of<Base, Unrelated>::value);
        FL_CHECK_TRAIT(!is_base_of<Derived, Base>::value);
    }

    FL_SUBCASE("same type") {
        FL_CHECK_TRAIT(is_base_of<Base, Base>::value);
        FL_CHECK_TRAIT(is_base_of<int, int>::value);
    }
}

FL_TEST_CASE("fl::is_same") {
    FL_SUBCASE("same types") {
        FL_CHECK_TRAIT(is_same<int, int>::value);
        FL_CHECK_TRAIT(is_same<float, float>::value);
        FL_CHECK_TRAIT(is_same<Base, Base>::value);
        FL_CHECK_TRAIT(is_same_v_helper<int, int>::value);
    }

    FL_SUBCASE("different types") {
        FL_CHECK_TRAIT(!is_same<int, float>::value);
        FL_CHECK_TRAIT(!is_same<int, long>::value);
        FL_CHECK_TRAIT(!is_same<Base, Derived>::value);
        FL_CHECK_TRAIT(!is_same_v_helper<int, float>::value);
    }

    FL_SUBCASE("cv-qualified types") {
        FL_CHECK_TRAIT(!is_same<int, const int>::value);
        FL_CHECK_TRAIT(!is_same<int, volatile int>::value);
        FL_CHECK_TRAIT(is_same<const int, const int>::value);
    }
}

FL_TEST_CASE("fl::conditional") {
    FL_SUBCASE("true condition") {
        static_assert(is_same<conditional<true, int, float>::type, int>::value,
                      "conditional true should choose first type");
        static_assert(is_same<conditional_t<true, int, float>, int>::value,
                      "conditional_t true should choose first type");
    }

    FL_SUBCASE("false condition") {
        static_assert(is_same<conditional<false, int, float>::type, float>::value,
                      "conditional false should choose second type");
        static_assert(is_same<conditional_t<false, int, float>, float>::value,
                      "conditional_t false should choose second type");
    }
}

FL_TEST_CASE("fl::is_array") {
    FL_SUBCASE("array types") {
        FL_CHECK_TRAIT(is_array<int[]>::value);
        FL_CHECK_TRAIT(is_array<int[10]>::value);
        FL_CHECK_TRAIT(is_array<float[5]>::value);
    }

    FL_SUBCASE("non-array types") {
        FL_CHECK_TRAIT(!is_array<int>::value);
        FL_CHECK_TRAIT(!is_array<int*>::value);
        FL_CHECK_TRAIT(!is_array<Base>::value);
    }
}

FL_TEST_CASE("fl::remove_extent") {
    FL_SUBCASE("array types") {
        static_assert(is_same<remove_extent<int[]>::type, int>::value,
                      "should remove extent from unbounded array");
        static_assert(is_same<remove_extent<int[10]>::type, int>::value,
                      "should remove extent from bounded array");
        static_assert(is_same<remove_extent<float[5]>::type, float>::value,
                      "should remove extent from array");
    }

    FL_SUBCASE("non-array types") {
        static_assert(is_same<remove_extent<int>::type, int>::value,
                      "should not affect non-array");
        static_assert(is_same<remove_extent<int*>::type, int*>::value,
                      "should not affect pointer");
    }
}

FL_TEST_CASE("fl::is_function") {
    FL_SUBCASE("function types") {
        FL_CHECK_EQ((is_function<int()>::value), true);
        FL_CHECK_EQ((is_function<void(int, float)>::value), true);
        FL_CHECK_EQ((is_function<int() const>::value), true);
        FL_CHECK_EQ((is_function<int() volatile>::value), true);
        FL_CHECK_EQ((is_function<int() const volatile>::value), true);
    }

    FL_SUBCASE("non-function types") {
        FL_CHECK_TRAIT(!is_function<int>::value);
        FL_CHECK_TRAIT(!is_function<int*>::value);
        FL_CHECK_TRAIT(!is_function<Base>::value);
    }
}

FL_TEST_CASE("fl::add_pointer") {
    FL_SUBCASE("non-reference types") {
        static_assert(is_same<add_pointer<int>::type, int*>::value,
                      "should add pointer to int");
        static_assert(is_same<add_pointer_t<float>, float*>::value,
                      "should add pointer to float");
    }

    FL_SUBCASE("reference types") {
        static_assert(is_same<add_pointer<int&>::type, int*>::value,
                      "should convert lvalue ref to pointer");
        static_assert(is_same<add_pointer<int&&>::type, int*>::value,
                      "should convert rvalue ref to pointer");
    }
}

FL_TEST_CASE("fl::remove_const") {
    FL_SUBCASE("const types") {
        static_assert(is_same<remove_const<const int>::type, int>::value,
                      "should remove const from int");
        static_assert(is_same<remove_const<const float>::type, float>::value,
                      "should remove const from float");
    }

    FL_SUBCASE("non-const types") {
        static_assert(is_same<remove_const<int>::type, int>::value,
                      "should not affect non-const");
        static_assert(is_same<remove_const<volatile int>::type, volatile int>::value,
                      "should not affect volatile");
    }
}

FL_TEST_CASE("fl::is_const") {
    FL_SUBCASE("const types") {
        FL_CHECK_TRAIT(is_const<const int>::value);
        FL_CHECK_TRAIT(is_const<const float>::value);
        FL_CHECK_TRAIT(is_const<const Base>::value);
    }

    FL_SUBCASE("non-const types") {
        FL_CHECK_TRAIT(!is_const<int>::value);
        FL_CHECK_TRAIT(!is_const<volatile int>::value);
        FL_CHECK_TRAIT(!is_const<Base>::value);
    }
}

FL_TEST_CASE("fl::is_lvalue_reference") {
    FL_SUBCASE("lvalue references") {
        FL_CHECK_TRAIT(is_lvalue_reference<int&>::value);
        FL_CHECK_TRAIT(is_lvalue_reference<float&>::value);
        FL_CHECK_TRAIT(is_lvalue_reference<Base&>::value);
    }

    FL_SUBCASE("non-lvalue references") {
        FL_CHECK_TRAIT(!is_lvalue_reference<int>::value);
        FL_CHECK_TRAIT(!is_lvalue_reference<int&&>::value);
        FL_CHECK_TRAIT(!is_lvalue_reference<int*>::value);
    }
}

FL_TEST_CASE("fl::is_void") {
    FL_SUBCASE("void type") {
        FL_CHECK_TRAIT(is_void<void>::value);
    }

    FL_SUBCASE("non-void types") {
        FL_CHECK_TRAIT(!is_void<int>::value);
        FL_CHECK_TRAIT(!is_void<float>::value);
        FL_CHECK_TRAIT(!is_void<void*>::value);
    }
}

FL_TEST_CASE("fl::forward") {
    FL_SUBCASE("lvalue forward") {
        int x = 42;
        int& ref = forward<int&>(x);
        FL_CHECK_EQ(ref, 42);
    }

    FL_SUBCASE("rvalue forward") {
        int temp = 42;
        int&& rref = forward<int&&>(fl::move(temp));
        FL_CHECK_EQ(rref, 42);
    }
}

FL_TEST_CASE("fl::remove_cv") {
    FL_SUBCASE("const") {
        static_assert(is_same<remove_cv<const int>::type, int>::value,
                      "should remove const");
        static_assert(is_same<remove_cv_t<const int>, int>::value,
                      "remove_cv_t should remove const");
    }

    FL_SUBCASE("volatile") {
        static_assert(is_same<remove_cv<volatile int>::type, int>::value,
                      "should remove volatile");
    }

    FL_SUBCASE("const volatile") {
        static_assert(is_same<remove_cv<const volatile int>::type, int>::value,
                      "should remove const volatile");
    }

    FL_SUBCASE("neither") {
        static_assert(is_same<remove_cv<int>::type, int>::value,
                      "should not affect non-cv type");
    }
}

FL_TEST_CASE("fl::decay") {
    FL_SUBCASE("array decay") {
        static_assert(is_same<decay<int[10]>::type, int*>::value,
                      "array should decay to pointer");
        static_assert(is_same<decay_t<int[]>, int*>::value,
                      "unbounded array should decay to pointer");
    }

    FL_SUBCASE("function decay") {
        static_assert(is_same<decay<int()>::type, int(*)()>::value,
                      "function should decay to function pointer");
    }

    FL_SUBCASE("reference and cv decay") {
        static_assert(is_same<decay<const int&>::type, int>::value,
                      "should remove ref and const");
        static_assert(is_same<decay<volatile int&&>::type, int>::value,
                      "should remove ref and volatile");
    }

    FL_SUBCASE("no decay") {
        static_assert(is_same<decay<int>::type, int>::value,
                      "should not affect plain int");
    }
}

FL_TEST_CASE("fl::is_pod") {
    FL_SUBCASE("POD types") {
        FL_CHECK_TRAIT(is_pod<bool>::value);
        FL_CHECK_TRAIT(is_pod<char>::value);
        FL_CHECK_TRAIT(is_pod<int>::value);
        FL_CHECK_TRAIT(is_pod<unsigned int>::value);
        FL_CHECK_TRAIT(is_pod<long>::value);
        FL_CHECK_TRAIT(is_pod<float>::value);
        FL_CHECK_TRAIT(is_pod<double>::value);
        FL_CHECK_TRAIT(is_pod_v_helper<int>::value);
    }

    FL_SUBCASE("non-POD types by default") {
        // Custom classes default to non-POD for safety
        FL_CHECK_TRAIT(!is_pod<Base>::value);
        FL_CHECK_TRAIT(!is_pod<Derived>::value);
    }
}

FL_TEST_CASE("fl::is_member_function_pointer") {
    FL_SUBCASE("member function pointers") {
        FL_CHECK_EQ((is_member_function_pointer<int (Base::*)()>::value), true);
        FL_CHECK_EQ((is_member_function_pointer<void (Base::*)(int)>::value), true);
        FL_CHECK_EQ((is_member_function_pointer<int (Base::*)() const>::value), true);
    }

    FL_SUBCASE("non-member function pointers") {
        FL_CHECK_TRAIT(!is_member_function_pointer<int>::value);
        FL_CHECK_TRAIT(!is_member_function_pointer<int*>::value);
        FL_CHECK_EQ((is_member_function_pointer<int(*)()>::value), false);
    }
}

FL_TEST_CASE("fl::is_integral") {
    FL_SUBCASE("integral types") {
        FL_CHECK_TRAIT(is_integral<bool>::value);
        FL_CHECK_TRAIT(is_integral<char>::value);
        FL_CHECK_TRAIT(is_integral<signed char>::value);
        FL_CHECK_TRAIT(is_integral<unsigned char>::value);
        FL_CHECK_TRAIT(is_integral<short>::value);
        FL_CHECK_TRAIT(is_integral<unsigned short>::value);
        FL_CHECK_TRAIT(is_integral<int>::value);
        FL_CHECK_TRAIT(is_integral<unsigned int>::value);
        FL_CHECK_TRAIT(is_integral<long>::value);
        FL_CHECK_TRAIT(is_integral<unsigned long>::value);
        FL_CHECK_TRAIT(is_integral<long long>::value);
        FL_CHECK_TRAIT(is_integral<unsigned long long>::value);
    }

    FL_SUBCASE("cv-qualified integrals") {
        FL_CHECK_TRAIT(is_integral<const int>::value);
        FL_CHECK_TRAIT(is_integral<volatile int>::value);
        // Note: const volatile not tested due to ambiguous partial specialization
    }

    FL_SUBCASE("reference to integrals") {
        FL_CHECK_TRAIT(is_integral<int&>::value);
        FL_CHECK_TRAIT(is_integral<const int&>::value);
    }

    FL_SUBCASE("non-integral types") {
        FL_CHECK_TRAIT(!is_integral<float>::value);
        FL_CHECK_TRAIT(!is_integral<double>::value);
        FL_CHECK_TRAIT(!is_integral<int*>::value);
        FL_CHECK_TRAIT(!is_integral<void>::value);
    }

    FL_SUBCASE("fl fixed-width types") {
        FL_CHECK_TRAIT(is_integral<i8>::value);
        FL_CHECK_TRAIT(is_integral<u8>::value);
        FL_CHECK_TRAIT(is_integral<i16>::value);
        FL_CHECK_TRAIT(is_integral<u16>::value);
        FL_CHECK_TRAIT(is_integral<i32>::value);
        FL_CHECK_TRAIT(is_integral<u32>::value);
        FL_CHECK_TRAIT(is_integral<i64>::value);
        FL_CHECK_TRAIT(is_integral<u64>::value);
    }
}

FL_TEST_CASE("fl::is_floating_point") {
    FL_SUBCASE("floating point types") {
        FL_CHECK_TRAIT(is_floating_point<float>::value);
        FL_CHECK_TRAIT(is_floating_point<double>::value);
        FL_CHECK_TRAIT(is_floating_point<long double>::value);
    }

    FL_SUBCASE("cv-qualified floating point") {
        FL_CHECK_TRAIT(is_floating_point<const float>::value);
        FL_CHECK_TRAIT(is_floating_point<volatile double>::value);
    }

    FL_SUBCASE("reference to floating point") {
        FL_CHECK_TRAIT(is_floating_point<float&>::value);
        FL_CHECK_TRAIT(is_floating_point<const double&>::value);
    }

    FL_SUBCASE("non-floating point types") {
        FL_CHECK_TRAIT(!is_floating_point<int>::value);
        FL_CHECK_TRAIT(!is_floating_point<bool>::value);
        FL_CHECK_TRAIT(!is_floating_point<float*>::value);
    }
}

FL_TEST_CASE("fl::is_signed") {
    FL_SUBCASE("signed types") {
        FL_CHECK_TRAIT(is_signed<signed char>::value);
        FL_CHECK_TRAIT(is_signed<short>::value);
        FL_CHECK_TRAIT(is_signed<int>::value);
        FL_CHECK_TRAIT(is_signed<long>::value);
        FL_CHECK_TRAIT(is_signed<long long>::value);
        FL_CHECK_TRAIT(is_signed<float>::value);
        FL_CHECK_TRAIT(is_signed<double>::value);
    }

    FL_SUBCASE("unsigned types") {
        FL_CHECK_TRAIT(!is_signed<unsigned char>::value);
        FL_CHECK_TRAIT(!is_signed<unsigned short>::value);
        FL_CHECK_TRAIT(!is_signed<unsigned int>::value);
        FL_CHECK_TRAIT(!is_signed<unsigned long>::value);
        FL_CHECK_TRAIT(!is_signed<unsigned long long>::value);
        FL_CHECK_TRAIT(!is_signed<bool>::value);
    }
}

FL_TEST_CASE("fl::type_rank") {
    FL_SUBCASE("rank ordering") {
        FL_CHECK((type_rank<bool>::value < type_rank<char>::value));
        FL_CHECK((type_rank<char>::value < type_rank<short>::value));
        FL_CHECK((type_rank<short>::value < type_rank<int>::value));
        FL_CHECK((type_rank<int>::value < type_rank<long>::value));
        FL_CHECK((type_rank<long>::value < type_rank<long long>::value));
        FL_CHECK((type_rank<long long>::value < type_rank<float>::value));
        FL_CHECK((type_rank<float>::value < type_rank<double>::value));
        FL_CHECK((type_rank<double>::value < type_rank<long double>::value));
    }

    FL_SUBCASE("same rank for signed/unsigned") {
        FL_CHECK_EQ(type_rank<int>::value, type_rank<unsigned int>::value);
        FL_CHECK_EQ(type_rank<long>::value, type_rank<unsigned long>::value);
    }
}

FL_TEST_CASE("fl::common_type") {
    FL_SUBCASE("same type") {
        static_assert(is_same<common_type<int, int>::type, int>::value,
                      "common type of same types should be that type");
        static_assert(is_same<common_type_t<float, float>, float>::value,
                      "common_type_t of same types should be that type");
    }

    FL_SUBCASE("integer promotion") {
        static_assert(is_same<common_type<int, long>::type, long>::value,
                      "larger integer type should win");
        static_assert(is_same<common_type<short, int>::type, int>::value,
                      "int should win over short");
    }

    FL_SUBCASE("floating point promotion") {
        static_assert(is_same<common_type<int, float>::type, float>::value,
                      "float should win over int");
        static_assert(is_same<common_type<float, double>::type, double>::value,
                      "double should win over float");
        static_assert(is_same<common_type<double, long double>::type, long double>::value,
                      "long double should win");
    }

    FL_SUBCASE("symmetric") {
        static_assert(is_same<common_type<int, float>::type, common_type<float, int>::type>::value,
                      "common_type should be symmetric");
        static_assert(is_same<common_type<int, long>::type, common_type<long, int>::type>::value,
                      "common_type should be symmetric for integers");
    }
}

FL_TEST_CASE("fl::swap") {
    FL_SUBCASE("POD types") {
        int a = 5, b = 10;
        swap(a, b);
        FL_CHECK_EQ(a, 10);
        FL_CHECK_EQ(b, 5);
    }

    FL_SUBCASE("custom swap method") {
        SwappableClass x(5), y(10);
        swap(x, y);
        FL_CHECK_EQ(x.value, 10);
        FL_CHECK_EQ(y.value, 5);
    }

    FL_SUBCASE("float types") {
        float a = 1.5f, b = 2.5f;
        swap(a, b);
        FL_CHECK_EQ(a, 2.5f);
        FL_CHECK_EQ(b, 1.5f);
    }
}

FL_TEST_CASE("fl::swap_by_copy") {
    FL_SUBCASE("forces copy semantics") {
        int a = 5, b = 10;
        swap_by_copy(a, b);
        FL_CHECK_EQ(a, 10);
        FL_CHECK_EQ(b, 5);
    }
}

FL_TEST_CASE("fl::has_member_swap") {
    FL_SUBCASE("class with swap") {
        FL_CHECK_TRAIT(has_member_swap<SwappableClass>::value);
    }

    FL_SUBCASE("POD without swap") {
        FL_CHECK_TRAIT(!has_member_swap<int>::value);
        FL_CHECK_TRAIT(!has_member_swap<float>::value);
    }

    FL_SUBCASE("class without swap") {
        FL_CHECK_TRAIT(!has_member_swap<Base>::value);
    }
}

FL_TEST_CASE("fl::contains_type") {
    FL_SUBCASE("type is present") {
        FL_CHECK_TRAIT(contains_type<int, int, float, double>::value);
        FL_CHECK_TRAIT(contains_type<float, int, float, double>::value);
        FL_CHECK_TRAIT(contains_type<double, int, float, double>::value);
    }

    FL_SUBCASE("type is not present") {
        FL_CHECK_TRAIT(!contains_type<char, int, float, double>::value);
        FL_CHECK_TRAIT(!contains_type<long, int, float, double>::value);
    }

    FL_SUBCASE("single type") {
        FL_CHECK_TRAIT(contains_type<int, int>::value);
        FL_CHECK_TRAIT(!contains_type<float, int>::value);
    }

    FL_SUBCASE("empty list") {
        FL_CHECK_TRAIT(!contains_type<int>::value);
    }
}

FL_TEST_CASE("fl::max_size") {
    FL_SUBCASE("multiple types") {
        constexpr fl::size max1 = max_size<char, short, int, long>::value;
        FL_CHECK((max1 >= sizeof(long)));
        FL_CHECK((max1 >= sizeof(int)));
        FL_CHECK((max1 >= sizeof(short)));
        FL_CHECK((max1 >= sizeof(char)));
    }

    FL_SUBCASE("single type") {
        FL_CHECK((fl::size(max_size<int>::value) == sizeof(int)));
    }

    FL_SUBCASE("empty") {
        FL_CHECK((fl::size(max_size<>::value) == 0));
    }
}

FL_TEST_CASE("fl::max_align") {
    FL_SUBCASE("multiple types") {
        constexpr fl::size align = max_align<char, short, int, long>::value;
        FL_CHECK((align >= alignof(long)));
        FL_CHECK((align >= alignof(int)));
        FL_CHECK((align >= alignof(short)));
        FL_CHECK((align >= alignof(char)));
    }

    FL_SUBCASE("single type") {
        FL_CHECK((fl::size(max_align<int>::value) == alignof(int)));
    }

    FL_SUBCASE("empty") {
        FL_CHECK((fl::size(max_align<>::value) == 1));
    }
}

FL_TEST_CASE("fl::alignment_of") {
    FL_SUBCASE("basic types") {
        FL_CHECK((fl::size(alignment_of<char>::value) == alignof(char)));
        FL_CHECK((fl::size(alignment_of<short>::value) == alignof(short)));
        FL_CHECK((fl::size(alignment_of<int>::value) == alignof(int)));
        FL_CHECK((fl::size(alignment_of<long>::value) == alignof(long)));
        FL_CHECK((fl::size(alignment_of<float>::value) == alignof(float)));
        FL_CHECK((fl::size(alignment_of<double>::value) == alignof(double)));
    }

    FL_SUBCASE("class types") {
        FL_CHECK((fl::size(alignment_of<Base>::value) == alignof(Base)));
        FL_CHECK((fl::size(alignment_of<Derived>::value) == alignof(Derived)));
    }
}

// Compile-time tests
namespace compile_time_tests {
    // Test declval
    template<typename T>
    void test_declval() {
        // declval should be usable in unevaluated contexts
        using result = decltype(declval<T>());
        static_assert(is_same<result, typename add_rvalue_reference<T>::type>::value,
                      "declval should return rvalue reference");
    }

    void run_tests() {
        test_declval<int>();
        test_declval<float>();
        test_declval<Base>();
    }
}
