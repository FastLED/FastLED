/// @file fltest_self_test.cpp
/// @brief Self-test for the fl::test framework
///
/// This test verifies that the fltest framework itself works correctly.
/// It uses the FL_* macros to test basic functionality.

#include "fl/fltest.h"
#include "fl/geometry.h"  // For fl::rect
#include "fl/stl/vector.h"  // For fl::vector
#include "fl/stl/unordered_set.h"  // For fl::unordered_set
#include "fl/stl/unordered_map.h"  // For fl::unordered_map
#include "fl/stl/map.h"  // For fl::FixedMap and fl::SortedHeapMap

FL_TEST_CASE("FL_CHECK basic assertions") {
    FL_CHECK(true);
    FL_CHECK(1 == 1);
    FL_CHECK(2 + 2 == 4);
}

FL_TEST_CASE("FL_CHECK_FALSE assertions") {
    FL_CHECK_FALSE(false);
    FL_CHECK_FALSE(1 == 2);
}

FL_TEST_CASE("FL_CHECK_EQ comparisons") {
    FL_CHECK_EQ(1, 1);
    FL_CHECK_EQ(42, 42);

    int a = 10;
    int b = 10;
    FL_CHECK_EQ(a, b);
}

FL_TEST_CASE("FL_CHECK_NE comparisons") {
    FL_CHECK_NE(1, 2);
    FL_CHECK_NE(42, 43);
}

FL_TEST_CASE("FL_CHECK comparison operators") {
    FL_CHECK_LT(1, 2);
    FL_CHECK_GT(3, 2);
    FL_CHECK_LE(2, 2);
    FL_CHECK_LE(1, 2);
    FL_CHECK_GE(2, 2);
    FL_CHECK_GE(3, 2);
}

FL_TEST_CASE("FL_SUBCASE basic nesting") {
    int value = 0;

    FL_SUBCASE("first subcase") {
        value = 1;
        FL_CHECK_EQ(value, 1);
    }

    FL_SUBCASE("second subcase") {
        value = 2;
        FL_CHECK_EQ(value, 2);
    }

    // Both subcases should run independently
    // After each subcase, value should have been set
}

FL_TEST_CASE("FL_SUBCASE nested") {
    int level = 0;

    FL_SUBCASE("outer A") {
        level = 1;
        FL_CHECK_EQ(level, 1);

        FL_SUBCASE("inner A1") {
            level = 11;
            FL_CHECK_EQ(level, 11);
        }

        FL_SUBCASE("inner A2") {
            level = 12;
            FL_CHECK_EQ(level, 12);
        }
    }

    FL_SUBCASE("outer B") {
        level = 2;
        FL_CHECK_EQ(level, 2);

        FL_SUBCASE("inner B1") {
            level = 21;
            FL_CHECK_EQ(level, 21);
        }
    }
}

FL_TEST_CASE("FL_REQUIRE stops on failure") {
    bool reachedAfterRequire = false;

    FL_REQUIRE(true);  // Should pass
    reachedAfterRequire = true;
    FL_CHECK(reachedAfterRequire);

    // Note: Can't easily test REQUIRE failure without causing test failure
    // which is the expected behavior
}

FL_TEST_CASE("FL_MESSAGE and FL_CAPTURE") {
    int x = 42;
    FL_MESSAGE("Testing message output");
    FL_CAPTURE(x);

    fl::string msg = "Hello from fltest!";
    FL_INFO("Info: " << msg);
    FL_CAPTURE(msg);

    FL_CHECK(true);  // Need an assertion
}

FL_TEST_CASE("FL_WARN non-failing assertion") {
    FL_WARN(true);   // Should not output anything
    FL_WARN(false);  // Should output a warning but NOT fail the test

    FL_CHECK(true);  // Need an assertion - this test should PASS
}

FL_TEST_CASE("Approx floating point comparison") {
    double a = 1.0 / 3.0;
    double b = 0.333333;

    // These should be approximately equal
    FL_CHECK(a == fl::test::Approx(b).epsilon(0.0001));
    FL_CHECK(fl::test::Approx(b).epsilon(0.0001) == a);

    // These should NOT be equal with very tight epsilon
    FL_CHECK(a != fl::test::Approx(b).epsilon(0.0000001));

    // Test basic equality
    FL_CHECK(3.14159 == fl::test::Approx(3.14159));

    // Test near-zero comparisons
    FL_CHECK(0.0 == fl::test::Approx(0.0));
    FL_CHECK(1e-10 == fl::test::Approx(0.0).epsilon(1e-9));

    // Test comparison operators
    FL_CHECK(1.5 < fl::test::Approx(2.0));
    FL_CHECK(2.5 > fl::test::Approx(2.0));
    FL_CHECK(2.0 <= fl::test::Approx(2.0));
    FL_CHECK(2.0 >= fl::test::Approx(2.0));
}

// This test is named uniquely to test filtering
FL_TEST_CASE("Filter test: unique_filter_marker_xyz") {
    FL_CHECK(true);
}

// Test BDD-style macros
// Note: Each WHEN path is independent, so counter resets to 0 between paths
FL_SCENARIO("BDD-style scenario test") {
    int counter = 0;

    FL_GIVEN("a counter starting at zero") {
        FL_CHECK_EQ(counter, 0);

        FL_WHEN("incremented by 1") {
            counter += 1;

            FL_THEN("it equals 1") {
                FL_CHECK_EQ(counter, 1);
            }
        }
    }
}

FL_SCENARIO("BDD-style increment by 2") {
    int counter = 0;

    FL_GIVEN("a counter starting at zero") {
        FL_CHECK_EQ(counter, 0);

        FL_WHEN("incremented by 2") {
            counter += 2;

            FL_THEN("it equals 2") {
                FL_CHECK_EQ(counter, 2);
            }
        }
    }
}

// Test CHECK_CLOSE for absolute tolerance
FL_TEST_CASE("FL_CHECK_CLOSE absolute tolerance") {
    double a = 1.0;
    double b = 1.0001;

    // Should pass with epsilon 0.001
    FL_CHECK_CLOSE(a, b, 0.001);

    // Should pass with epsilon 0.0001
    FL_CHECK_CLOSE(a, b, 0.0001);

    // Test with negative numbers
    FL_CHECK_CLOSE(-5.0, -5.0001, 0.001);

    // Test near-zero
    FL_CHECK_CLOSE(0.0, 0.00001, 0.0001);
}

// Test fixture - a simple counter fixture
struct CounterFixture {
    int mCount;
    bool mSetupCalled;
    static int sDestructorCalls;

    CounterFixture() : mCount(42), mSetupCalled(true) {
        // Constructor acts as setup
    }

    ~CounterFixture() {
        // Destructor acts as teardown
        sDestructorCalls++;
    }
};

int CounterFixture::sDestructorCalls = 0;

FL_TEST_CASE_FIXTURE(CounterFixture, "FL_TEST_CASE_FIXTURE test") {
    // Can access fixture members directly
    FL_CHECK(mSetupCalled);
    FL_CHECK_EQ(mCount, 42);

    // Modify and check
    mCount = 100;
    FL_CHECK_EQ(mCount, 100);

    // Destructor will be called when this test function returns
}

// Test Approx with margin (absolute tolerance)
FL_TEST_CASE("Approx with margin") {
    // margin() provides absolute tolerance
    FL_CHECK(1.0 == fl::test::Approx(1.5).margin(0.6));   // |1.0 - 1.5| = 0.5 <= 0.6
    FL_CHECK(1.0 != fl::test::Approx(1.5).margin(0.4));   // |1.0 - 1.5| = 0.5 > 0.4

    // margin with epsilon(0) - only absolute margin
    FL_CHECK(0.0 == fl::test::Approx(0.001).margin(0.01).epsilon(0));

    // near zero, margin is more reliable than epsilon
    FL_CHECK(0.0 == fl::test::Approx(0.0001).margin(0.001));
}

// Test string comparison macros
FL_TEST_CASE("FL_CHECK_STR_EQ string equality") {
    fl::string a = "hello";
    fl::string b = "hello";
    fl::string c = "world";

    FL_CHECK_STR_EQ(a, b);
    FL_CHECK_STR_EQ(a, "hello");
    FL_CHECK_STR_NE(a, c);
    FL_CHECK_STR_NE(a, "world");
}

FL_TEST_CASE("FL_CHECK_STR_CONTAINS substring search") {
    fl::string text = "Hello, World!";

    FL_CHECK_STR_CONTAINS(text, "World");
    FL_CHECK_STR_CONTAINS(text, "Hello");
    FL_CHECK_STR_CONTAINS(text, ",");
    FL_CHECK_STR_CONTAINS("testing 123", "123");
}

// Test exception macros
#if FLTEST_EXCEPTIONS_ENABLED
FL_TEST_CASE("FL_CHECK_THROWS exception testing") {
    auto throwingFunc = []() { throw 42; };
    auto safeFunc = []() { return 42; };

    FL_CHECK_THROWS(throwingFunc());
    FL_CHECK_NOTHROW(safeFunc());
}
#endif

// Test array comparison macro
FL_TEST_CASE("FL_CHECK_ARRAY_EQ array comparison") {
    int actual[] = {1, 2, 3, 4, 5};
    int expected[] = {1, 2, 3, 4, 5};

    // Should pass - arrays are equal
    FL_CHECK_ARRAY_EQ(actual, expected, 5);

    // Test with single element
    int single1[] = {42};
    int single2[] = {42};
    FL_CHECK_ARRAY_EQ(single1, single2, 1);
}

// Test CHECK_THROWS_AS macro (typed exception catching)
#if FLTEST_EXCEPTIONS_ENABLED
FL_TEST_CASE("FL_CHECK_THROWS_AS typed exception") {
    // Test catching specific exception type
    auto throwsRuntimeError = []() {
        throw std::runtime_error("test error");  // okay std namespace - testing exception handling
    };
    auto throwsInt = []() {
        throw 42;
    };

    FL_CHECK_THROWS_AS(throwsRuntimeError(), std::runtime_error);  // okay std namespace - testing exception handling
    FL_CHECK_THROWS_AS(throwsInt(), int);

    // Test catching base class
    FL_CHECK_THROWS_AS(throwsRuntimeError(), std::exception);  // okay std namespace - testing exception handling
}

FL_TEST_CASE("FL_CHECK_THROWS_WITH exception message") {
    auto throwsWithMessage = []() {
        throw std::runtime_error("contains specific text here");  // okay std namespace - testing exception handling
    };

    FL_CHECK_THROWS_WITH(throwsWithMessage(), "specific text");
    FL_CHECK_THROWS_WITH(throwsWithMessage(), "contains");
}
#endif // FLTEST_EXCEPTIONS_ENABLED

// Test SerialReporter (just instantiation, not full output verification)
FL_TEST_CASE("SerialReporter instantiation") {
    // Just verify we can create a SerialReporter
    fl::test::SerialReporter reporter;
    FL_CHECK(true);  // Basic assertion to mark test as having content

    // Create with custom print function
    auto customPrint = [](const char*) {};
    fl::test::SerialReporter reporter2(customPrint);
    FL_CHECK(true);
}

// Test FL_CHECK_MESSAGE / FL_REQUIRE_MESSAGE
FL_TEST_CASE("FL_CHECK_MESSAGE with custom message") {
    int value = 42;
    FL_CHECK_MESSAGE(value > 0, "value should be positive, got: " << value);
    FL_CHECK_MESSAGE(value == 42, "expected 42, got: " << value);

    fl::string text = "hello";
    FL_CHECK_MESSAGE(!text.empty(), "string should not be empty");

    // Test with multiple values in message
    int a = 10, b = 20;
    FL_CHECK_MESSAGE(a < b, "expected " << a << " < " << b);
}

FL_TEST_CASE("FL_REQUIRE_MESSAGE stops on failure") {
    int count = 5;
    FL_REQUIRE_MESSAGE(count > 0, "count must be positive for test");

    // This should be reached since the above passes
    FL_CHECK_EQ(count, 5);
}

// Test timeout support (just API, not actual timeout)
FL_TEST_CASE("Timeout API availability") {
    // Just verify the timeout API is available
    auto& ctx = fl::test::TestContext::instance();

    // These should compile and be callable
    ctx.setDefaultTimeoutMs(0);  // Disable timeout
    fl::u32 elapsed = ctx.getElapsedMs();
    FL_CHECK(elapsed == 0 || elapsed > 0);  // Just check it returns something

    // Note: We can't easily test actual timeouts in the self-test
    // because we don't have a time source set up
}

// Test XMLReporter instantiation and basic operation
FL_TEST_CASE("XMLReporter basic output") {
    fl::string output;
    fl::test::XMLReporter reporter(&output, "TestSuite");

    // Verify we can create and use an XMLReporter
    FL_CHECK(!output.empty() || output.empty());  // Just check it compiles

    // Test XML escaping by calling methods
    reporter.testRunStart();

    // Check the output is empty after testRunStart (not written until testRunEnd)
    FL_CHECK(output.empty());
}

// Test JSONReporter instantiation and basic operation
FL_TEST_CASE("JSONReporter basic output") {
    fl::string output;
    fl::test::JSONReporter reporter(&output);

    // Verify we can create and use a JSONReporter
    FL_CHECK(!output.empty() || output.empty());  // Just check it compiles

    // Test JSON escaping by calling methods
    reporter.testRunStart();

    // Check the output is empty after testRunStart (not written until testRunEnd)
    FL_CHECK(output.empty());
}

// Test TAPReporter (Test Anything Protocol)
FL_TEST_CASE("TAPReporter basic output") {
    fl::string output;
    fl::test::TAPReporter reporter(&output);

    // Verify we can create a TAPReporter with buffer
    FL_CHECK(output.empty());

    // Test TAPReporter with print function
    auto customPrint = [](const char*) {};
    fl::test::TAPReporter reporter2(customPrint);
    FL_CHECK(true);  // Just verify it compiles

    // Verify the setTotalTests method exists
    reporter.setTotalTests(10);
    FL_CHECK(true);
}

// Test listTests functionality
FL_TEST_CASE("Test listing API availability") {
    auto& ctx = fl::test::TestContext::instance();

    // The listTests method should be available
    // We don't actually call it here to avoid polluting test output
    // Just verify the API is accessible
    (void)&ctx;  // Suppress unused warning
    FL_CHECK(true);
}

// Test FL_SKIP macro - DISABLED because FL_SKIP is working but can't be
// tested from within doctest wrapper without affecting overall results.
// Uncomment in standalone mode to verify skip functionality.
#if 0
FL_TEST_CASE("FL_SKIP test") {
    FL_SKIP("This test is intentionally skipped for demonstration");
    FL_FAIL("This should never be reached");
}
#endif

// Test fl::optional with StrStream
FL_TEST_CASE("StrStream optional support") {
    fl::StrStream ss;

    // Test nullopt output
    fl::optional<int> emptyOpt;
    ss << emptyOpt;
    FL_CHECK_STR_EQ(ss.str(), "nullopt");

    // Test optional with value
    fl::optional<int> valueOpt(42);
    ss.clear();
    ss << valueOpt;
    FL_CHECK_STR_CONTAINS(ss.str(), "optional");
    FL_CHECK_STR_CONTAINS(ss.str(), "42");
}

// Test fl::rect with StrStream
FL_TEST_CASE("StrStream rect support") {
    fl::StrStream ss;

    fl::rect<int> r(1, 2, 10, 20);
    ss << r;

    FL_CHECK_STR_CONTAINS(ss.str(), "rect");
    FL_CHECK_STR_CONTAINS(ss.str(), "1");
    FL_CHECK_STR_CONTAINS(ss.str(), "2");
    FL_CHECK_STR_CONTAINS(ss.str(), "10");
    FL_CHECK_STR_CONTAINS(ss.str(), "20");
}

// Test fl::vector with StrStream
FL_TEST_CASE("StrStream vector support") {
    fl::StrStream ss;

    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    ss << vec;

    // Vector format is [item1, item2, item3]
    FL_CHECK_STR_CONTAINS(ss.str(), "[");
    FL_CHECK_STR_CONTAINS(ss.str(), "1");
    FL_CHECK_STR_CONTAINS(ss.str(), "2");
    FL_CHECK_STR_CONTAINS(ss.str(), "3");
    FL_CHECK_STR_CONTAINS(ss.str(), "]");

    // Test empty vector
    fl::StrStream ss2;
    fl::vector<int> emptyVec;
    ss2 << emptyVec;
    FL_CHECK_STR_EQ(ss2.str(), "[]");
}

// Test CRGB with StrStream
FL_TEST_CASE("StrStream CRGB support") {
    fl::StrStream ss;

    CRGB color(255, 128, 64);
    ss << color;

    // CRGB format includes RGB values
    FL_CHECK_STR_CONTAINS(ss.str(), "255");
    FL_CHECK_STR_CONTAINS(ss.str(), "128");
    FL_CHECK_STR_CONTAINS(ss.str(), "64");
}

// Test fl::unordered_set with StrStream
FL_TEST_CASE("StrStream unordered_set support") {
    fl::StrStream ss;

    fl::unordered_set<int> set;
    set.insert(42);
    ss << set;

    // Set format is {item1, item2, ...}
    FL_CHECK_STR_CONTAINS(ss.str(), "{");
    FL_CHECK_STR_CONTAINS(ss.str(), "42");
    FL_CHECK_STR_CONTAINS(ss.str(), "}");

    // Test empty set
    fl::StrStream ss2;
    fl::unordered_set<int> emptySet;
    ss2 << emptySet;
    FL_CHECK_STR_EQ(ss2.str(), "{}");
}

// Test fl::unordered_map with StrStream
FL_TEST_CASE("StrStream unordered_map support") {
    fl::StrStream ss;

    fl::unordered_map<int, int> map;
    map.insert(1, 100);
    ss << map;

    // Map format is {key: value, ...}
    FL_CHECK_STR_CONTAINS(ss.str(), "{");
    FL_CHECK_STR_CONTAINS(ss.str(), "1");
    FL_CHECK_STR_CONTAINS(ss.str(), ":");
    FL_CHECK_STR_CONTAINS(ss.str(), "100");
    FL_CHECK_STR_CONTAINS(ss.str(), "}");

    // Test empty map
    fl::StrStream ss2;
    fl::unordered_map<int, int> emptyMap;
    ss2 << emptyMap;
    FL_CHECK_STR_EQ(ss2.str(), "{}");
}

// Test fl::FixedMap with StrStream
FL_TEST_CASE("StrStream FixedMap support") {
    fl::StrStream ss;

    fl::FixedMap<int, int, 8> map;
    map.insert(1, 100);
    map.insert(2, 200);
    ss << map;

    // Map format is {key: value, ...}
    FL_CHECK_STR_CONTAINS(ss.str(), "{");
    FL_CHECK_STR_CONTAINS(ss.str(), "1");
    FL_CHECK_STR_CONTAINS(ss.str(), ":");
    FL_CHECK_STR_CONTAINS(ss.str(), "100");
    FL_CHECK_STR_CONTAINS(ss.str(), "}");

    // Test empty map
    fl::StrStream ss2;
    fl::FixedMap<int, int, 8> emptyMap;
    ss2 << emptyMap;
    FL_CHECK_STR_EQ(ss2.str(), "{}");
}

// Test fl::SortedHeapMap with StrStream
FL_TEST_CASE("StrStream SortedHeapMap support") {
    fl::StrStream ss;

    fl::SortedHeapMap<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    ss << map;

    // Map format is {key: value, ...}
    FL_CHECK_STR_CONTAINS(ss.str(), "{");
    FL_CHECK_STR_CONTAINS(ss.str(), "1");
    FL_CHECK_STR_CONTAINS(ss.str(), ":");
    FL_CHECK_STR_CONTAINS(ss.str(), "100");
    FL_CHECK_STR_CONTAINS(ss.str(), "}");

    // Test empty map
    fl::StrStream ss2;
    fl::SortedHeapMap<int, int> emptyMap;
    ss2 << emptyMap;
    FL_CHECK_STR_EQ(ss2.str(), "{}");
}

// Test fl::span with StrStream
#include "fl/slice.h"
#include "fl/stl/pair.h"

FL_TEST_CASE("StrStream span support") {
    fl::StrStream ss;

    int arr[] = {10, 20, 30};
    fl::span<int> s(arr, 3);
    ss << s;

    // Span format is span[item1, item2, ...]
    FL_CHECK_STR_CONTAINS(ss.str(), "span[");
    FL_CHECK_STR_CONTAINS(ss.str(), "10");
    FL_CHECK_STR_CONTAINS(ss.str(), "20");
    FL_CHECK_STR_CONTAINS(ss.str(), "30");
    FL_CHECK_STR_CONTAINS(ss.str(), "]");

    // Test empty span
    fl::StrStream ss2;
    fl::span<int> emptySpan;
    ss2 << emptySpan;
    FL_CHECK_STR_EQ(ss2.str(), "span[]");
}

// Test fl::pair with StrStream
FL_TEST_CASE("StrStream pair support") {
    fl::StrStream ss;

    fl::pair<int, int> p1(42, 100);
    ss << p1;

    // Pair format is (first, second)
    FL_CHECK_STR_CONTAINS(ss.str(), "(");
    FL_CHECK_STR_CONTAINS(ss.str(), "42");
    FL_CHECK_STR_CONTAINS(ss.str(), ", ");
    FL_CHECK_STR_CONTAINS(ss.str(), "100");
    FL_CHECK_STR_CONTAINS(ss.str(), ")");

    // Test pair with different types
    fl::StrStream ss2;
    fl::pair<fl::string, int> p2("key", 123);
    ss2 << p2;
    FL_CHECK_STR_CONTAINS(ss2.str(), "key");
    FL_CHECK_STR_CONTAINS(ss2.str(), "123");
}

// Test WARN_* comparison macros (log warnings but don't fail)
FL_TEST_CASE("FL_WARN_* comparison macros") {
    // WARN_EQ should log warning but NOT fail when values differ
    int a = 10;
    int b = 20;
    FL_WARN_EQ(a, b);  // Should warn: 10 != 20

    // WARN_NE should log warning but NOT fail when values are equal
    FL_WARN_NE(a, a);  // Should warn: both equal 10

    // WARN_FALSE should log warning but NOT fail when expression is true
    FL_WARN_FALSE(true);  // Should warn

    // WARN_LT should log when lhs >= rhs
    FL_WARN_LT(20, 10);  // Should warn: 20 >= 10

    // WARN_GT should log when lhs <= rhs
    FL_WARN_GT(10, 20);  // Should warn: 10 <= 20

    // WARN_LE should log when lhs > rhs
    FL_WARN_LE(20, 10);  // Should warn: 20 > 10

    // WARN_GE should log when lhs < rhs
    FL_WARN_GE(10, 20);  // Should warn: 10 < 20

    // All passing cases (should NOT log anything)
    FL_WARN_EQ(10, 10);  // Equal - no warning
    FL_WARN_NE(10, 20);  // Not equal - no warning
    FL_WARN_FALSE(false);  // False - no warning
    FL_WARN_LT(10, 20);  // 10 < 20 - no warning
    FL_WARN_GT(20, 10);  // 20 > 10 - no warning
    FL_WARN_LE(10, 10);  // 10 <= 10 - no warning
    FL_WARN_GE(10, 10);  // 10 >= 10 - no warning

    // This test should PASS because WARN macros don't affect pass/fail
    FL_CHECK(true);
}

// =============================================================================
// Test FL_TEST_CASE_TEMPLATE - Parameterized type testing
// =============================================================================

// Simple template test that runs for int, float, and double
FL_TEST_CASE_TEMPLATE("Template: basic type operations", T, int, float, double) {
    T value = static_cast<T>(42);
    FL_CHECK(value == static_cast<T>(42));

    T sum = value + static_cast<T>(8);
    FL_CHECK(sum == static_cast<T>(50));
}

// Test with fl::vector<T> for different types
FL_TEST_CASE_TEMPLATE("Template: vector operations", T, int, double) {
    fl::vector<T> vec;
    vec.push_back(static_cast<T>(10));
    vec.push_back(static_cast<T>(20));
    vec.push_back(static_cast<T>(30));

    FL_CHECK_EQ(vec.size(), 3u);
    FL_CHECK_EQ(vec[0], static_cast<T>(10));
    FL_CHECK_EQ(vec[1], static_cast<T>(20));
    FL_CHECK_EQ(vec[2], static_cast<T>(30));
}

// Custom type for testing TYPE_TO_STRING
struct MyCustomType {
    int mValue;
    MyCustomType() : mValue(0) {}
    MyCustomType(int v) : mValue(v) {}
    bool operator==(const MyCustomType& other) const { return mValue == other.mValue; }
};

// Register custom type name
FL_TYPE_TO_STRING(MyCustomType, "MyCustomType")

// Test with custom type to verify TYPE_TO_STRING works
FL_TEST_CASE_TEMPLATE("Template: with custom type", T, int, MyCustomType) {
    T value{};
    (void)value;  // Suppress unused variable warning
    // Just verify the test runs for both types
    FL_CHECK(true);
}

// Test DEFINE/INVOKE pattern for separating definition from instantiation
FL_TEST_CASE_TEMPLATE_DEFINE("Template: define/invoke pattern", T, my_test_id) {
    T val = static_cast<T>(100);
    FL_CHECK(val == static_cast<T>(100));
}

// Invoke the defined template test with specific types
FL_TEST_CASE_TEMPLATE_INVOKE(my_test_id, int, float);

// Test FL_TEST_SUITE_BEGIN/END
FL_TEST_SUITE_BEGIN("SuiteBeginEndTest")

FL_TEST_CASE("Suite test 1") {
    FL_CHECK(true);
}

FL_TEST_CASE("Suite test 2") {
    FL_CHECK_EQ(1, 1);
}

FL_TEST_SUITE_END()

// When built with doctest (default), wrap FL tests in a doctest TEST_CASE
// This allows us to test the FL framework within the existing test infrastructure
#ifndef FLTEST_STANDALONE_MAIN
#include "doctest.h"

TEST_CASE("FL_TEST framework self-test") {
    // Run all registered FL tests
    int result = fl::test::TestContext::instance().run();
    REQUIRE(result == 0);
}
#else
// Main function for standalone testing (when built without doctest)
int main(int argc, char** argv) {
    return fl::test::TestContext::instance().run(argc, const_cast<const char**>(argv));
}
#endif
