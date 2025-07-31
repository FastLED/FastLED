
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/type_traits.h"
#include "fl/unused.h"

using namespace fl;

// Forward declarations
class TestClass;
void takeLValue(TestClass &obj);
void takeRValue(TestClass &&obj);
template <typename T> void forwardValue(T &&obj);

TEST_CASE("is_base_of") {
    class Base {};
    class Derived : public Base {};

    CHECK(fl::is_base_of<Base, Derived>::value);
    CHECK_FALSE(fl::is_base_of<Derived, Base>::value);
}

TEST_CASE("is_integral<T> value") {
    // Test with integral types
    REQUIRE(is_integral<bool>::value);
    REQUIRE(is_integral<char>::value);
    REQUIRE(is_integral<signed char>::value);
    REQUIRE(is_integral<unsigned char>::value);
    REQUIRE(is_integral<int>::value);
    REQUIRE(is_integral<unsigned int>::value);
    REQUIRE(is_integral<short>::value);
    REQUIRE(is_integral<long>::value);
    REQUIRE(is_integral<long long>::value);

    // Test with sized types
    REQUIRE(is_integral<int8_t>::value);
    REQUIRE(is_integral<uint8_t>::value);
    REQUIRE(is_integral<int16_t>::value);
    REQUIRE(is_integral<uint16_t>::value);
    REQUIRE(is_integral<int32_t>::value);
    REQUIRE(is_integral<uint32_t>::value);
    REQUIRE(is_integral<int64_t>::value);
    REQUIRE(is_integral<uint64_t>::value);

    // Test with non-integral types
    REQUIRE_FALSE(is_integral<float>::value);
    REQUIRE_FALSE(is_integral<double>::value);
    REQUIRE_FALSE(is_integral<char *>::value);
}

TEST_CASE("Test fl::move") {
    // Test with a simple class that tracks move operations
    class MoveTracker {
      public:
        MoveTracker() : moved_from(false) {}

        // Copy constructor
        MoveTracker(const MoveTracker &other) : moved_from(false) {
            // Regular copy
            FL_UNUSED(other);
        }

        // Move constructor
        MoveTracker(MoveTracker &&other) : moved_from(false) {
            other.moved_from = true;
            FL_UNUSED(other);
        }

        bool was_moved_from() const { return moved_from; }

      private:
        bool moved_from;
    };

    // Test 1: Basic move operation
    {
        MoveTracker original;
        REQUIRE_FALSE(original.was_moved_from());

        // Use fl::move to trigger move constructor
        MoveTracker moved(fl::move(original));

        // Original should be marked as moved from
        REQUIRE(original.was_moved_from());
        REQUIRE_FALSE(moved.was_moved_from());
    }

    // Test 2: Move vs copy behavior
    {
        MoveTracker original;

        // Regular copy - shouldn't mark original as moved
        MoveTracker copied(original);
        REQUIRE_FALSE(original.was_moved_from());

        // Move should mark as moved
        MoveTracker moved(fl::move(original));
        REQUIRE(original.was_moved_from());
    }
}

// A simple class to test with
class TestClass {
  public:
    int value;
    TestClass() : value(0) {}
    TestClass(int v) : value(v) {}
};

// Function that takes an lvalue reference
void takeLValue(TestClass &obj) { obj.value = 42; }

// Function that takes an rvalue reference
void takeRValue(TestClass &&obj) { obj.value = 100; }

// Function template that forwards its argument to an lvalue reference function
template <typename T> void forwardToLValue(T &&obj) {
    takeLValue(fl::forward<T>(obj));
}

// Function template that forwards its argument to an rvalue reference function
template <typename T> void forwardValue(T &&obj) {
    takeRValue(fl::forward<T>(obj));
}

TEST_CASE("fl::forward preserves value categories") {

    SUBCASE("Forwarding lvalues") {
        TestClass obj(10);

        // Should call takeLValue
        forwardToLValue(obj);
        REQUIRE(obj.value == 42);

        // This would fail to compile if we tried to forward an lvalue to an
        // rvalue reference Uncomment to see the error: forwardValue(obj);  //
        // This should fail at compile time
    }

    SUBCASE("Forwarding rvalues") {
        // Should call takeRValue
        forwardValue(TestClass(20));

        // We can also test with a temporary
        TestClass temp(30);
        forwardValue(fl::move(temp));
        // temp.value should now be 100, but we can't check it here since it was
        // moved
    }

    SUBCASE("Move and forward") {
        TestClass obj(50);

        // Move creates an rvalue, forward preserves that
        forwardValue(fl::move(obj));

        // obj was moved from, so we don't make assertions about its state
    }
}

TEST_CASE("common_type_impl behavior") {
    SUBCASE("same types return same type") {
        static_assert(fl::is_same<fl::common_type_t<int, int>, int>::value,
                      "int + int should return int");
        static_assert(
            fl::is_same<fl::common_type_t<short, short>, short>::value,
            "short + short should return short");
        static_assert(fl::is_same<fl::common_type_t<long, long>, long>::value,
                      "long + long should return long");
        static_assert(
            fl::is_same<fl::common_type_t<float, float>, float>::value,
            "float + float should return float");
    }

    SUBCASE("different size promotions with generic types") {
        // Smaller to larger promotions
        static_assert(fl::is_same<fl::common_type_t<short, int>, int>::value,
                      "short + int should return int");
        static_assert(fl::is_same<fl::common_type_t<int, short>, int>::value,
                      "int + short should return int");
        static_assert(fl::is_same<fl::common_type_t<int, long>, long>::value,
                      "int + long should return long");
        static_assert(fl::is_same<fl::common_type_t<long, int>, long>::value,
                      "long + int should return long");
        static_assert(
            fl::is_same<fl::common_type_t<long, long long>, long long>::value,
            "long + long long should return long long");
    }

    SUBCASE("mixed signedness same size with generic types") {
        // These should use the partial specialization to choose signed version
        static_assert(
            fl::is_same<fl::common_type_t<short, unsigned short>, short>::value,
            "short + unsigned short should return short");
        static_assert(
            fl::is_same<fl::common_type_t<unsigned short, short>, short>::value,
            "unsigned short + short should return short");
        static_assert(
            fl::is_same<fl::common_type_t<int, unsigned int>, int>::value,
            "int + unsigned int should return int");
        static_assert(
            fl::is_same<fl::common_type_t<unsigned int, int>, int>::value,
            "unsigned int + int should return int");
        static_assert(
            fl::is_same<fl::common_type_t<long, unsigned long>, long>::value,
            "long + unsigned long should return long");
    }

    SUBCASE("float/double promotions with generic types") {
        // Float always wins over integers
        static_assert(fl::is_same<fl::common_type_t<int, float>, float>::value,
                      "int + float should return float");
        static_assert(fl::is_same<fl::common_type_t<float, int>, float>::value,
                      "float + int should return float");
        static_assert(
            fl::is_same<fl::common_type_t<short, float>, float>::value,
            "short + float should return float");
        static_assert(fl::is_same<fl::common_type_t<long, float>, float>::value,
                      "long + float should return float");

        // Double wins over float
        static_assert(
            fl::is_same<fl::common_type_t<float, double>, double>::value,
            "float + double should return double");
        static_assert(
            fl::is_same<fl::common_type_t<double, float>, double>::value,
            "double + float should return double");
        static_assert(
            fl::is_same<fl::common_type_t<int, double>, double>::value,
            "int + double should return double");
    }

    SUBCASE("sized types with generic types mixed") {
        // Verify sized types work with generic types
        static_assert(fl::is_same<fl::common_type_t<int8_t, int>, int>::value,
                      "int8_t + int should return int");
        static_assert(fl::is_same<fl::common_type_t<int, int8_t>, int>::value,
                      "int + int8_t should return int");
        static_assert(fl::is_same<fl::common_type_t<uint16_t, int>, int>::value,
                      "uint16_t + int should return int");
        static_assert(
            fl::is_same<fl::common_type_t<short, int32_t>, int32_t>::value,
            "short + int32_t should return int32_t");
    }

    SUBCASE("cross signedness different sizes with generic types") {
        // Larger type wins when different sizes
        static_assert(fl::is_same<fl::common_type_t<char, unsigned int>,
                                  unsigned int>::value,
                      "char + unsigned int should return unsigned int");
        static_assert(
            fl::is_same<fl::common_type_t<unsigned char, int>, int>::value,
            "unsigned char + int should return int");
        static_assert(fl::is_same<fl::common_type_t<short, unsigned long>,
                                  unsigned long>::value,
                      "short + unsigned long should return unsigned long");
    }

    SUBCASE("explicit sized type combinations") {
        // Test the explicit sized type specializations we added
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, int16_t>, int16_t>::value,
            "int8_t + int16_t should return int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, uint32_t>, uint32_t>::value,
            "uint8_t + uint32_t should return uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, uint32_t>, uint32_t>::value,
            "int16_t + uint32_t should return uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, int32_t>, int32_t>::value,
            "uint16_t + int32_t should return int32_t");
    }

    SUBCASE("mixed signedness same size with sized types") {
        // Test partial specialization with sized types
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, uint16_t>, int16_t>::value,
            "int16_t + uint16_t should return int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, int16_t>, int16_t>::value,
            "uint16_t + int16_t should return int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<int32_t, uint32_t>, int32_t>::value,
            "int32_t + uint32_t should return int32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int64_t, uint64_t>, int64_t>::value,
            "int64_t + uint64_t should return int64_t");
    }


}

TEST_CASE("type promotion helper templates") {
    SUBCASE("choose_by_size helper tests") {
        // Test size-based selection
        static_assert(fl::is_same<fl::choose_by_size<int8_t, int16_t>::type,
                                  int16_t>::value,
                      "choose_by_size should pick larger type");
        static_assert(fl::is_same<fl::choose_by_size<int16_t, int8_t>::type,
                                  int16_t>::value,
                      "choose_by_size should pick larger type (reversed)");
        static_assert(fl::is_same<fl::choose_by_size<int32_t, int64_t>::type,
                                  int64_t>::value,
                      "choose_by_size should pick int64_t over int32_t");
        static_assert(fl::is_same<fl::choose_by_size<uint8_t, uint32_t>::type,
                                  uint32_t>::value,
                      "choose_by_size should pick uint32_t over uint8_t");

        // Test mixed signedness with different sizes
        static_assert(
            fl::is_same<fl::choose_by_size<int8_t, uint32_t>::type,
                        uint32_t>::value,
            "choose_by_size should pick larger type regardless of signedness");
        static_assert(
            fl::is_same<fl::choose_by_size<uint16_t, int64_t>::type,
                        int64_t>::value,
            "choose_by_size should pick larger type regardless of signedness");
    }

    SUBCASE("choose_by_rank helper tests") {
        // Test rank-based selection when same size
        static_assert(
            fl::is_same<fl::choose_by_rank<int, long>::type, long>::value,
            "choose_by_rank should pick higher rank type");
        static_assert(
            fl::is_same<fl::choose_by_rank<long, int>::type, long>::value,
            "choose_by_rank should pick higher rank type (reversed)");

        // Test with unsigned versions
        static_assert(
            fl::is_same<fl::choose_by_rank<unsigned int, unsigned long>::type,
                        unsigned long>::value,
            "choose_by_rank should work with unsigned types");

        // Test floating point ranks
        static_assert(
            fl::is_same<fl::choose_by_rank<float, double>::type, double>::value,
            "choose_by_rank should pick double over float");
        static_assert(fl::is_same<fl::choose_by_rank<double, long double>::type,
                                  long double>::value,
                      "choose_by_rank should pick long double over double");
    }

    SUBCASE("choose_by_signedness helper tests") {
        // Test signedness selection
        static_assert(
            fl::is_same<fl::choose_by_signedness<int16_t, uint16_t>::type,
                        int16_t>::value,
            "choose_by_signedness should pick signed type");
        static_assert(
            fl::is_same<fl::choose_by_signedness<uint16_t, int16_t>::type,
                        int16_t>::value,
            "choose_by_signedness should pick signed type (reversed)");
        static_assert(
            fl::is_same<fl::choose_by_signedness<int32_t, uint32_t>::type,
                        int32_t>::value,
            "choose_by_signedness should pick signed type for 32-bit");
        static_assert(
            fl::is_same<fl::choose_by_signedness<uint64_t, int64_t>::type,
                        int64_t>::value,
            "choose_by_signedness should pick signed type for 64-bit");

        // Test same signedness (should pick first)
        static_assert(
            fl::is_same<fl::choose_by_signedness<int16_t, int32_t>::type,
                        int16_t>::value,
            "choose_by_signedness should pick first when both signed");
        static_assert(
            fl::is_same<fl::choose_by_signedness<uint16_t, uint32_t>::type,
                        uint16_t>::value,
            "choose_by_signedness should pick first when both unsigned");
    }

    SUBCASE("integer_promotion_impl comprehensive tests") {
        // Test all three promotion paths in sequence

        // Path 1: Different sizes (should use choose_by_size)
        static_assert(
            fl::is_same<fl::integer_promotion_impl<int8_t, int32_t>::type,
                        int32_t>::value,
            "integer_promotion_impl should use size for different sizes");
        static_assert(
            fl::is_same<fl::integer_promotion_impl<uint16_t, int64_t>::type,
                        int64_t>::value,
            "integer_promotion_impl should use size for different sizes");

        // Path 2: Same size, different rank (should use choose_by_rank)
        static_assert(fl::is_same<fl::integer_promotion_impl<int, long>::type,
                                  long>::value,
                      "integer_promotion_impl should use rank for same size "
                      "different rank");
        static_assert(
            fl::is_same<
                fl::integer_promotion_impl<unsigned int, unsigned long>::type,
                unsigned long>::value,
            "integer_promotion_impl should use rank for unsigned same size "
            "different rank");

        // Path 3: Same size, same rank, different signedness (should use
        // choose_by_signedness)
        static_assert(
            fl::is_same<fl::integer_promotion_impl<int16_t, uint16_t>::type,
                        int16_t>::value,
            "integer_promotion_impl should use signedness for same size same "
            "rank");
        static_assert(
            fl::is_same<fl::integer_promotion_impl<uint32_t, int32_t>::type,
                        int32_t>::value,
            "integer_promotion_impl should use signedness for same size same "
            "rank");
    }
}

TEST_CASE("comprehensive type promotion edge cases") {
    SUBCASE(
        "forbidden int8_t and uint8_t combinations should fail compilation") {
        // These should not have a 'type' member and would cause compilation
        // errors if used We can't directly test compilation failure, but we can
        // document the expected behavior

        // The following would fail to compile if uncommented:
        // using forbidden1 = fl::common_type_t<int8_t, uint8_t>;
        // using forbidden2 = fl::common_type_t<uint8_t, int8_t>;

        // But we can test that other int8_t/uint8_t combinations work fine
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, int16_t>, int16_t>::value,
            "int8_t + int16_t should work");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, int16_t>, int16_t>::value,
            "uint8_t + int16_t should work");
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, uint16_t>, uint16_t>::value,
            "int8_t + uint16_t should work");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, uint16_t>, uint16_t>::value,
            "uint8_t + uint16_t should work");
    }

    SUBCASE("all integer size combinations") {
        // Test comprehensive matrix of integer size promotions

        // 8-bit to larger
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, int16_t>, int16_t>::value,
            "int8_t promotes to int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, int32_t>, int32_t>::value,
            "int8_t promotes to int32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, int64_t>, int64_t>::value,
            "int8_t promotes to int64_t");

        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, uint16_t>, uint16_t>::value,
            "uint8_t promotes to uint16_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, uint32_t>, uint32_t>::value,
            "uint8_t promotes to uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, uint64_t>, uint64_t>::value,
            "uint8_t promotes to uint64_t");

        // 16-bit to larger
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, int32_t>, int32_t>::value,
            "int16_t promotes to int32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, int64_t>, int64_t>::value,
            "int16_t promotes to int64_t");

        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, uint32_t>, uint32_t>::value,
            "uint16_t promotes to uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, uint64_t>, uint64_t>::value,
            "uint16_t promotes to uint64_t");

        // 32-bit to larger
        static_assert(
            fl::is_same<fl::common_type_t<int32_t, int64_t>, int64_t>::value,
            "int32_t promotes to int64_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint32_t, uint64_t>, uint64_t>::value,
            "uint32_t promotes to uint64_t");
    }

    SUBCASE("cross-signedness different sizes") {
        // Test mixed signedness with different sizes - larger should always win

        // Signed to unsigned larger
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, uint16_t>, uint16_t>::value,
            "int8_t + uint16_t = uint16_t");
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, uint32_t>, uint32_t>::value,
            "int8_t + uint32_t = uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, uint64_t>, uint64_t>::value,
            "int8_t + uint64_t = uint64_t");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, uint32_t>, uint32_t>::value,
            "int16_t + uint32_t = uint32_t");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, uint64_t>, uint64_t>::value,
            "int16_t + uint64_t = uint64_t");
        static_assert(
            fl::is_same<fl::common_type_t<int32_t, uint64_t>, uint64_t>::value,
            "int32_t + uint64_t = uint64_t");

        // Unsigned to signed larger
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, int16_t>, int16_t>::value,
            "uint8_t + int16_t = int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, int32_t>, int32_t>::value,
            "uint8_t + int32_t = int32_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, int64_t>, int64_t>::value,
            "uint8_t + int64_t = int64_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, int32_t>, int32_t>::value,
            "uint16_t + int32_t = int32_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, int64_t>, int64_t>::value,
            "uint16_t + int64_t = int64_t");
        static_assert(
            fl::is_same<fl::common_type_t<uint32_t, int64_t>, int64_t>::value,
            "uint32_t + int64_t = int64_t");
    }

    SUBCASE("floating point comprehensive tests") {
        // Float with all integer types
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, float>, float>::value,
            "int8_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, float>, float>::value,
            "uint8_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, float>, float>::value,
            "int16_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, float>, float>::value,
            "uint16_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<int32_t, float>, float>::value,
            "int32_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<uint32_t, float>, float>::value,
            "uint32_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<int64_t, float>, float>::value,
            "int64_t + float = float");
        static_assert(
            fl::is_same<fl::common_type_t<uint64_t, float>, float>::value,
            "uint64_t + float = float");

        // Double with all integer types
        static_assert(
            fl::is_same<fl::common_type_t<int8_t, double>, double>::value,
            "int8_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<uint8_t, double>, double>::value,
            "uint8_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, double>, double>::value,
            "int16_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<uint16_t, double>, double>::value,
            "uint16_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<int32_t, double>, double>::value,
            "int32_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<uint32_t, double>, double>::value,
            "uint32_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<int64_t, double>, double>::value,
            "int64_t + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<uint64_t, double>, double>::value,
            "uint64_t + double = double");

        // Symmetric tests (reverse order)
        static_assert(
            fl::is_same<fl::common_type_t<float, int32_t>, float>::value,
            "float + int32_t = float");
        static_assert(
            fl::is_same<fl::common_type_t<double, uint64_t>, double>::value,
            "double + uint64_t = double");

        // Floating point hierarchy
        static_assert(
            fl::is_same<fl::common_type_t<float, double>, double>::value,
            "float + double = double");
        static_assert(
            fl::is_same<fl::common_type_t<double, float>, double>::value,
            "double + float = double");
        static_assert(fl::is_same<fl::common_type_t<float, long double>,
                                  long double>::value,
                      "float + long double = long double");
        static_assert(fl::is_same<fl::common_type_t<long double, float>,
                                  long double>::value,
                      "long double + float = long double");
        static_assert(fl::is_same<fl::common_type_t<double, long double>,
                                  long double>::value,
                      "double + long double = long double");
        static_assert(fl::is_same<fl::common_type_t<long double, double>,
                                  long double>::value,
                      "long double + double = long double");
    }

    SUBCASE("generic vs sized type interactions") {
        // Test interactions between generic types (int, long, etc.) and sized
        // types (int32_t, etc.)

        // On most systems, these should be equivalent but let's test promotion
        // behavior
        static_assert(
            fl::is_same<fl::common_type_t<short, int16_t>, int16_t>::value,
            "short + int16_t promotion");
        static_assert(
            fl::is_same<fl::common_type_t<int16_t, short>, int16_t>::value,
            "int16_t + short promotion");

        // Test char interactions (tricky because char signedness varies)
        static_assert(
            fl::is_same<fl::common_type_t<char, int16_t>, int16_t>::value,
            "char + int16_t = int16_t");
        static_assert(
            fl::is_same<fl::common_type_t<char, uint16_t>, uint16_t>::value,
            "char + uint16_t = uint16_t");

        // Test with long long
        static_assert(
            fl::is_same<fl::common_type_t<long long, int64_t>,
                        long long>::value,
            "long long + int64_t should prefer long long (higher rank)");
        static_assert(
            fl::is_same<fl::common_type_t<int64_t, long long>,
                        long long>::value,
            "int64_t + long long should prefer long long (higher rank)");
    }


}
