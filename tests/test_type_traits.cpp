
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/type_traits.h"

using namespace fl;

// Forward declarations
class TestClass;
void takeLValue(TestClass & obj);
void takeRValue(TestClass && obj);
template <typename T> void forwardValue(T && obj);

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
        }

        // Move constructor
        MoveTracker(MoveTracker &&other) : moved_from(false) {
            other.moved_from = true;
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
void takeLValue(TestClass & obj) { obj.value = 42; }

// Function that takes an rvalue reference
void takeRValue(TestClass && obj) { obj.value = 100; }

// Function template that forwards its argument to an lvalue reference function
template <typename T> void forwardToLValue(T &&obj) {
    takeLValue(fl::forward<T>(obj));
}

// Function template that forwards its argument to an rvalue reference function
template <typename T> void forwardValue(T && obj) {
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
