#include "fl/stl/type_traits.h"
#include "doctest.h"
#include "fl/int.h"
#include "fl/stl/move.h"

using namespace fl;

// Helper macro to avoid ODR-use of static constexpr enum values
// Uses variadic args to handle commas in template parameters
#define CHECK_TRAIT(...) CHECK(bool(__VA_ARGS__))

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

TEST_CASE("fl::integral_constant") {
    SUBCASE("true_type") {
        CHECK(bool(true_type::value));
        CHECK_TRAIT(integral_constant<bool, true>::value);
    }

    SUBCASE("false_type") {
        CHECK(!bool(false_type::value));
        CHECK_TRAIT(!integral_constant<bool, false>::value);
    }

    SUBCASE("integer constants") {
        CHECK((int(integral_constant<int, 42>::value) == 42));
        CHECK((int(integral_constant<int, -5>::value) == -5));
    }

    SUBCASE("constexpr operators") {
        constexpr bool t = true_type();
        constexpr bool f = false_type();
        CHECK_EQ(t, true);
        CHECK_EQ(f, false);
    }
}

TEST_CASE("fl::identity") {
    SUBCASE("preserves type") {
        static_assert(is_same<identity<int>::type, int>::value, "identity should preserve int");
        static_assert(is_same<identity<float>::type, float>::value, "identity should preserve float");
        static_assert(is_same<identity<Base>::type, Base>::value, "identity should preserve class");
    }
}

TEST_CASE("fl::add_rvalue_reference") {
    SUBCASE("non-reference types") {
        static_assert(is_same<add_rvalue_reference<int>::type, int&&>::value, "should add rvalue ref to int");
        static_assert(is_same<add_rvalue_reference<float>::type, float&&>::value, "should add rvalue ref to float");
    }

    SUBCASE("lvalue reference types") {
        static_assert(is_same<add_rvalue_reference<int&>::type, int&>::value, "should preserve lvalue ref");
        static_assert(is_same<add_rvalue_reference<float&>::type, float&>::value, "should preserve lvalue ref");
    }
}

TEST_CASE("fl::enable_if") {
    SUBCASE("true condition") {
        static_assert(is_same<enable_if<true, int>::type, int>::value, "enable_if true should provide type");
        static_assert(is_same<enable_if_t<true, int>, int>::value, "enable_if_t true should provide type");
    }

    SUBCASE("default type") {
        static_assert(is_same<enable_if<true>::type, void>::value, "enable_if default should be void");
    }
}

TEST_CASE("fl::is_base_of") {
    SUBCASE("inheritance relationship") {
        CHECK_TRAIT(is_base_of<Base, Derived>::value);
        CHECK_TRAIT(is_base_of_v_helper<Base, Derived>::value);
    }

    SUBCASE("no inheritance relationship") {
        CHECK_TRAIT(!is_base_of<Base, Unrelated>::value);
        CHECK_TRAIT(!is_base_of<Derived, Base>::value);
    }

    SUBCASE("same type") {
        CHECK_TRAIT(is_base_of<Base, Base>::value);
        CHECK_TRAIT(is_base_of<int, int>::value);
    }
}

TEST_CASE("fl::is_same") {
    SUBCASE("same types") {
        CHECK_TRAIT(is_same<int, int>::value);
        CHECK_TRAIT(is_same<float, float>::value);
        CHECK_TRAIT(is_same<Base, Base>::value);
        CHECK_TRAIT(is_same_v_helper<int, int>::value);
    }

    SUBCASE("different types") {
        CHECK_TRAIT(!is_same<int, float>::value);
        CHECK_TRAIT(!is_same<int, long>::value);
        CHECK_TRAIT(!is_same<Base, Derived>::value);
        CHECK_TRAIT(!is_same_v_helper<int, float>::value);
    }

    SUBCASE("cv-qualified types") {
        CHECK_TRAIT(!is_same<int, const int>::value);
        CHECK_TRAIT(!is_same<int, volatile int>::value);
        CHECK_TRAIT(is_same<const int, const int>::value);
    }
}

TEST_CASE("fl::conditional") {
    SUBCASE("true condition") {
        static_assert(is_same<conditional<true, int, float>::type, int>::value,
                      "conditional true should choose first type");
        static_assert(is_same<conditional_t<true, int, float>, int>::value,
                      "conditional_t true should choose first type");
    }

    SUBCASE("false condition") {
        static_assert(is_same<conditional<false, int, float>::type, float>::value,
                      "conditional false should choose second type");
        static_assert(is_same<conditional_t<false, int, float>, float>::value,
                      "conditional_t false should choose second type");
    }
}

TEST_CASE("fl::is_array") {
    SUBCASE("array types") {
        CHECK_TRAIT(is_array<int[]>::value);
        CHECK_TRAIT(is_array<int[10]>::value);
        CHECK_TRAIT(is_array<float[5]>::value);
    }

    SUBCASE("non-array types") {
        CHECK_TRAIT(!is_array<int>::value);
        CHECK_TRAIT(!is_array<int*>::value);
        CHECK_TRAIT(!is_array<Base>::value);
    }
}

TEST_CASE("fl::remove_extent") {
    SUBCASE("array types") {
        static_assert(is_same<remove_extent<int[]>::type, int>::value,
                      "should remove extent from unbounded array");
        static_assert(is_same<remove_extent<int[10]>::type, int>::value,
                      "should remove extent from bounded array");
        static_assert(is_same<remove_extent<float[5]>::type, float>::value,
                      "should remove extent from array");
    }

    SUBCASE("non-array types") {
        static_assert(is_same<remove_extent<int>::type, int>::value,
                      "should not affect non-array");
        static_assert(is_same<remove_extent<int*>::type, int*>::value,
                      "should not affect pointer");
    }
}

TEST_CASE("fl::is_function") {
    SUBCASE("function types") {
        CHECK_EQ((is_function<int()>::value), true);
        CHECK_EQ((is_function<void(int, float)>::value), true);
        CHECK_EQ((is_function<int() const>::value), true);
        CHECK_EQ((is_function<int() volatile>::value), true);
        CHECK_EQ((is_function<int() const volatile>::value), true);
    }

    SUBCASE("non-function types") {
        CHECK_TRAIT(!is_function<int>::value);
        CHECK_TRAIT(!is_function<int*>::value);
        CHECK_TRAIT(!is_function<Base>::value);
    }
}

TEST_CASE("fl::add_pointer") {
    SUBCASE("non-reference types") {
        static_assert(is_same<add_pointer<int>::type, int*>::value,
                      "should add pointer to int");
        static_assert(is_same<add_pointer_t<float>, float*>::value,
                      "should add pointer to float");
    }

    SUBCASE("reference types") {
        static_assert(is_same<add_pointer<int&>::type, int*>::value,
                      "should convert lvalue ref to pointer");
        static_assert(is_same<add_pointer<int&&>::type, int*>::value,
                      "should convert rvalue ref to pointer");
    }
}

TEST_CASE("fl::remove_const") {
    SUBCASE("const types") {
        static_assert(is_same<remove_const<const int>::type, int>::value,
                      "should remove const from int");
        static_assert(is_same<remove_const<const float>::type, float>::value,
                      "should remove const from float");
    }

    SUBCASE("non-const types") {
        static_assert(is_same<remove_const<int>::type, int>::value,
                      "should not affect non-const");
        static_assert(is_same<remove_const<volatile int>::type, volatile int>::value,
                      "should not affect volatile");
    }
}

TEST_CASE("fl::is_const") {
    SUBCASE("const types") {
        CHECK_TRAIT(is_const<const int>::value);
        CHECK_TRAIT(is_const<const float>::value);
        CHECK_TRAIT(is_const<const Base>::value);
    }

    SUBCASE("non-const types") {
        CHECK_TRAIT(!is_const<int>::value);
        CHECK_TRAIT(!is_const<volatile int>::value);
        CHECK_TRAIT(!is_const<Base>::value);
    }
}

TEST_CASE("fl::is_lvalue_reference") {
    SUBCASE("lvalue references") {
        CHECK_TRAIT(is_lvalue_reference<int&>::value);
        CHECK_TRAIT(is_lvalue_reference<float&>::value);
        CHECK_TRAIT(is_lvalue_reference<Base&>::value);
    }

    SUBCASE("non-lvalue references") {
        CHECK_TRAIT(!is_lvalue_reference<int>::value);
        CHECK_TRAIT(!is_lvalue_reference<int&&>::value);
        CHECK_TRAIT(!is_lvalue_reference<int*>::value);
    }
}

TEST_CASE("fl::is_void") {
    SUBCASE("void type") {
        CHECK_TRAIT(is_void<void>::value);
    }

    SUBCASE("non-void types") {
        CHECK_TRAIT(!is_void<int>::value);
        CHECK_TRAIT(!is_void<float>::value);
        CHECK_TRAIT(!is_void<void*>::value);
    }
}

TEST_CASE("fl::forward") {
    SUBCASE("lvalue forward") {
        int x = 42;
        int& ref = forward<int&>(x);
        CHECK_EQ(ref, 42);
    }

    SUBCASE("rvalue forward") {
        int temp = 42;
        int&& rref = forward<int&&>(fl::move(temp));
        CHECK_EQ(rref, 42);
    }
}

TEST_CASE("fl::remove_cv") {
    SUBCASE("const") {
        static_assert(is_same<remove_cv<const int>::type, int>::value,
                      "should remove const");
        static_assert(is_same<remove_cv_t<const int>, int>::value,
                      "remove_cv_t should remove const");
    }

    SUBCASE("volatile") {
        static_assert(is_same<remove_cv<volatile int>::type, int>::value,
                      "should remove volatile");
    }

    SUBCASE("const volatile") {
        static_assert(is_same<remove_cv<const volatile int>::type, int>::value,
                      "should remove const volatile");
    }

    SUBCASE("neither") {
        static_assert(is_same<remove_cv<int>::type, int>::value,
                      "should not affect non-cv type");
    }
}

TEST_CASE("fl::decay") {
    SUBCASE("array decay") {
        static_assert(is_same<decay<int[10]>::type, int*>::value,
                      "array should decay to pointer");
        static_assert(is_same<decay_t<int[]>, int*>::value,
                      "unbounded array should decay to pointer");
    }

    SUBCASE("function decay") {
        static_assert(is_same<decay<int()>::type, int(*)()>::value,
                      "function should decay to function pointer");
    }

    SUBCASE("reference and cv decay") {
        static_assert(is_same<decay<const int&>::type, int>::value,
                      "should remove ref and const");
        static_assert(is_same<decay<volatile int&&>::type, int>::value,
                      "should remove ref and volatile");
    }

    SUBCASE("no decay") {
        static_assert(is_same<decay<int>::type, int>::value,
                      "should not affect plain int");
    }
}

TEST_CASE("fl::is_pod") {
    SUBCASE("POD types") {
        CHECK_TRAIT(is_pod<bool>::value);
        CHECK_TRAIT(is_pod<char>::value);
        CHECK_TRAIT(is_pod<int>::value);
        CHECK_TRAIT(is_pod<unsigned int>::value);
        CHECK_TRAIT(is_pod<long>::value);
        CHECK_TRAIT(is_pod<float>::value);
        CHECK_TRAIT(is_pod<double>::value);
        CHECK_TRAIT(is_pod_v_helper<int>::value);
    }

    SUBCASE("non-POD types by default") {
        // Custom classes default to non-POD for safety
        CHECK_TRAIT(!is_pod<Base>::value);
        CHECK_TRAIT(!is_pod<Derived>::value);
    }
}

TEST_CASE("fl::is_member_function_pointer") {
    SUBCASE("member function pointers") {
        CHECK_EQ((is_member_function_pointer<int (Base::*)()>::value), true);
        CHECK_EQ((is_member_function_pointer<void (Base::*)(int)>::value), true);
        CHECK_EQ((is_member_function_pointer<int (Base::*)() const>::value), true);
    }

    SUBCASE("non-member function pointers") {
        CHECK_TRAIT(!is_member_function_pointer<int>::value);
        CHECK_TRAIT(!is_member_function_pointer<int*>::value);
        CHECK_EQ((is_member_function_pointer<int(*)()>::value), false);
    }
}

TEST_CASE("fl::is_integral") {
    SUBCASE("integral types") {
        CHECK_TRAIT(is_integral<bool>::value);
        CHECK_TRAIT(is_integral<char>::value);
        CHECK_TRAIT(is_integral<signed char>::value);
        CHECK_TRAIT(is_integral<unsigned char>::value);
        CHECK_TRAIT(is_integral<short>::value);
        CHECK_TRAIT(is_integral<unsigned short>::value);
        CHECK_TRAIT(is_integral<int>::value);
        CHECK_TRAIT(is_integral<unsigned int>::value);
        CHECK_TRAIT(is_integral<long>::value);
        CHECK_TRAIT(is_integral<unsigned long>::value);
        CHECK_TRAIT(is_integral<long long>::value);
        CHECK_TRAIT(is_integral<unsigned long long>::value);
    }

    SUBCASE("cv-qualified integrals") {
        CHECK_TRAIT(is_integral<const int>::value);
        CHECK_TRAIT(is_integral<volatile int>::value);
        // Note: const volatile not tested due to ambiguous partial specialization
    }

    SUBCASE("reference to integrals") {
        CHECK_TRAIT(is_integral<int&>::value);
        CHECK_TRAIT(is_integral<const int&>::value);
    }

    SUBCASE("non-integral types") {
        CHECK_TRAIT(!is_integral<float>::value);
        CHECK_TRAIT(!is_integral<double>::value);
        CHECK_TRAIT(!is_integral<int*>::value);
        CHECK_TRAIT(!is_integral<void>::value);
    }

    SUBCASE("fl fixed-width types") {
        CHECK_TRAIT(is_integral<i8>::value);
        CHECK_TRAIT(is_integral<u8>::value);
        CHECK_TRAIT(is_integral<i16>::value);
        CHECK_TRAIT(is_integral<u16>::value);
        CHECK_TRAIT(is_integral<i32>::value);
        CHECK_TRAIT(is_integral<u32>::value);
        CHECK_TRAIT(is_integral<i64>::value);
        CHECK_TRAIT(is_integral<u64>::value);
    }
}

TEST_CASE("fl::is_floating_point") {
    SUBCASE("floating point types") {
        CHECK_TRAIT(is_floating_point<float>::value);
        CHECK_TRAIT(is_floating_point<double>::value);
        CHECK_TRAIT(is_floating_point<long double>::value);
    }

    SUBCASE("cv-qualified floating point") {
        CHECK_TRAIT(is_floating_point<const float>::value);
        CHECK_TRAIT(is_floating_point<volatile double>::value);
    }

    SUBCASE("reference to floating point") {
        CHECK_TRAIT(is_floating_point<float&>::value);
        CHECK_TRAIT(is_floating_point<const double&>::value);
    }

    SUBCASE("non-floating point types") {
        CHECK_TRAIT(!is_floating_point<int>::value);
        CHECK_TRAIT(!is_floating_point<bool>::value);
        CHECK_TRAIT(!is_floating_point<float*>::value);
    }
}

TEST_CASE("fl::is_signed") {
    SUBCASE("signed types") {
        CHECK_TRAIT(is_signed<signed char>::value);
        CHECK_TRAIT(is_signed<short>::value);
        CHECK_TRAIT(is_signed<int>::value);
        CHECK_TRAIT(is_signed<long>::value);
        CHECK_TRAIT(is_signed<long long>::value);
        CHECK_TRAIT(is_signed<float>::value);
        CHECK_TRAIT(is_signed<double>::value);
    }

    SUBCASE("unsigned types") {
        CHECK_TRAIT(!is_signed<unsigned char>::value);
        CHECK_TRAIT(!is_signed<unsigned short>::value);
        CHECK_TRAIT(!is_signed<unsigned int>::value);
        CHECK_TRAIT(!is_signed<unsigned long>::value);
        CHECK_TRAIT(!is_signed<unsigned long long>::value);
        CHECK_TRAIT(!is_signed<bool>::value);
    }
}

TEST_CASE("fl::type_rank") {
    SUBCASE("rank ordering") {
        CHECK((type_rank<bool>::value < type_rank<char>::value));
        CHECK((type_rank<char>::value < type_rank<short>::value));
        CHECK((type_rank<short>::value < type_rank<int>::value));
        CHECK((type_rank<int>::value < type_rank<long>::value));
        CHECK((type_rank<long>::value < type_rank<long long>::value));
        CHECK((type_rank<long long>::value < type_rank<float>::value));
        CHECK((type_rank<float>::value < type_rank<double>::value));
        CHECK((type_rank<double>::value < type_rank<long double>::value));
    }

    SUBCASE("same rank for signed/unsigned") {
        CHECK_EQ(type_rank<int>::value, type_rank<unsigned int>::value);
        CHECK_EQ(type_rank<long>::value, type_rank<unsigned long>::value);
    }
}

TEST_CASE("fl::common_type") {
    SUBCASE("same type") {
        static_assert(is_same<common_type<int, int>::type, int>::value,
                      "common type of same types should be that type");
        static_assert(is_same<common_type_t<float, float>, float>::value,
                      "common_type_t of same types should be that type");
    }

    SUBCASE("integer promotion") {
        static_assert(is_same<common_type<int, long>::type, long>::value,
                      "larger integer type should win");
        static_assert(is_same<common_type<short, int>::type, int>::value,
                      "int should win over short");
    }

    SUBCASE("floating point promotion") {
        static_assert(is_same<common_type<int, float>::type, float>::value,
                      "float should win over int");
        static_assert(is_same<common_type<float, double>::type, double>::value,
                      "double should win over float");
        static_assert(is_same<common_type<double, long double>::type, long double>::value,
                      "long double should win");
    }

    SUBCASE("symmetric") {
        static_assert(is_same<common_type<int, float>::type, common_type<float, int>::type>::value,
                      "common_type should be symmetric");
        static_assert(is_same<common_type<int, long>::type, common_type<long, int>::type>::value,
                      "common_type should be symmetric for integers");
    }
}

TEST_CASE("fl::swap") {
    SUBCASE("POD types") {
        int a = 5, b = 10;
        swap(a, b);
        CHECK_EQ(a, 10);
        CHECK_EQ(b, 5);
    }

    SUBCASE("custom swap method") {
        SwappableClass x(5), y(10);
        swap(x, y);
        CHECK_EQ(x.value, 10);
        CHECK_EQ(y.value, 5);
    }

    SUBCASE("float types") {
        float a = 1.5f, b = 2.5f;
        swap(a, b);
        CHECK_EQ(a, 2.5f);
        CHECK_EQ(b, 1.5f);
    }
}

TEST_CASE("fl::swap_by_copy") {
    SUBCASE("forces copy semantics") {
        int a = 5, b = 10;
        swap_by_copy(a, b);
        CHECK_EQ(a, 10);
        CHECK_EQ(b, 5);
    }
}

TEST_CASE("fl::has_member_swap") {
    SUBCASE("class with swap") {
        CHECK_TRAIT(has_member_swap<SwappableClass>::value);
    }

    SUBCASE("POD without swap") {
        CHECK_TRAIT(!has_member_swap<int>::value);
        CHECK_TRAIT(!has_member_swap<float>::value);
    }

    SUBCASE("class without swap") {
        CHECK_TRAIT(!has_member_swap<Base>::value);
    }
}

TEST_CASE("fl::contains_type") {
    SUBCASE("type is present") {
        CHECK_TRAIT(contains_type<int, int, float, double>::value);
        CHECK_TRAIT(contains_type<float, int, float, double>::value);
        CHECK_TRAIT(contains_type<double, int, float, double>::value);
    }

    SUBCASE("type is not present") {
        CHECK_TRAIT(!contains_type<char, int, float, double>::value);
        CHECK_TRAIT(!contains_type<long, int, float, double>::value);
    }

    SUBCASE("single type") {
        CHECK_TRAIT(contains_type<int, int>::value);
        CHECK_TRAIT(!contains_type<float, int>::value);
    }

    SUBCASE("empty list") {
        CHECK_TRAIT(!contains_type<int>::value);
    }
}

TEST_CASE("fl::max_size") {
    SUBCASE("multiple types") {
        constexpr fl::size max1 = max_size<char, short, int, long>::value;
        CHECK((max1 >= sizeof(long)));
        CHECK((max1 >= sizeof(int)));
        CHECK((max1 >= sizeof(short)));
        CHECK((max1 >= sizeof(char)));
    }

    SUBCASE("single type") {
        CHECK((fl::size(max_size<int>::value) == sizeof(int)));
    }

    SUBCASE("empty") {
        CHECK((fl::size(max_size<>::value) == 0));
    }
}

TEST_CASE("fl::max_align") {
    SUBCASE("multiple types") {
        constexpr fl::size align = max_align<char, short, int, long>::value;
        CHECK((align >= alignof(long)));
        CHECK((align >= alignof(int)));
        CHECK((align >= alignof(short)));
        CHECK((align >= alignof(char)));
    }

    SUBCASE("single type") {
        CHECK((fl::size(max_align<int>::value) == alignof(int)));
    }

    SUBCASE("empty") {
        CHECK((fl::size(max_align<>::value) == 1));
    }
}

TEST_CASE("fl::alignment_of") {
    SUBCASE("basic types") {
        CHECK((fl::size(alignment_of<char>::value) == alignof(char)));
        CHECK((fl::size(alignment_of<short>::value) == alignof(short)));
        CHECK((fl::size(alignment_of<int>::value) == alignof(int)));
        CHECK((fl::size(alignment_of<long>::value) == alignof(long)));
        CHECK((fl::size(alignment_of<float>::value) == alignof(float)));
        CHECK((fl::size(alignment_of<double>::value) == alignof(double)));
    }

    SUBCASE("class types") {
        CHECK((fl::size(alignment_of<Base>::value) == alignof(Base)));
        CHECK((fl::size(alignment_of<Derived>::value) == alignof(Derived)));
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
