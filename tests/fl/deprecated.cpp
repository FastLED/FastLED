#include "fl/deprecated.h"
#include "fl/compiler_control.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Test that deprecated macros are properly defined
TEST_CASE("fl::deprecated_macros_defined") {
    // These macros should be defined on all platforms
    #ifndef FASTLED_DEPRECATED
    FAIL("FASTLED_DEPRECATED is not defined");
    #endif

    #ifndef FASTLED_DEPRECATED_CLASS
    FAIL("FASTLED_DEPRECATED_CLASS is not defined");
    #endif

    #ifndef FL_DEPRECATED
    FAIL("FL_DEPRECATED is not defined");
    #endif

    #ifndef FL_DEPRECATED_CLASS
    FAIL("FL_DEPRECATED_CLASS is not defined");
    #endif

    // If we get here, all macros are defined
    CHECK(true);
}

// Test that deprecated function macro can be applied
FL_DEPRECATED("This is a test deprecated function")
inline int deprecated_test_function() {
    return 42;
}

TEST_CASE("fl::deprecated_function_usage") {
    // Function should still work even if deprecated
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    int result = deprecated_test_function();
    CHECK_EQ(result, 42);
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

TEST_CASE("fl::deprecated_class_usage") {
    // Class should still work even if deprecated
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    DeprecatedTestClass obj;
    CHECK_EQ(obj.getValue(), 100);

    obj.value = 200;
    CHECK_EQ(obj.getValue(), 200);
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

TEST_CASE("fl::deprecated_method_usage") {
    TestClassWithDeprecatedMethod obj;
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS

    // Old method should still work
    CHECK_EQ(obj.oldMethod(), 1);

    FL_DISABLE_WARNING_POP
    // New method should work
    CHECK_EQ(obj.newMethod(), 2);
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

TEST_CASE("fl::deprecated_macro_equivalence") {
    // Both versions should work
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    CHECK_EQ(deprecated_fl(), 1);
    CHECK_EQ(deprecated_fastled(), 2);
}

// Test struct deprecation
struct FL_DEPRECATED_CLASS("Deprecated struct")
DeprecatedTestStruct {
    int x;
    int y;
};

TEST_CASE("fl::deprecated_struct_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    DeprecatedTestStruct s;
    s.x = 10;
    s.y = 20;
    CHECK_EQ(s.x, 10);
    CHECK_EQ(s.y, 20);
    FL_DISABLE_WARNING_POP
}

// Test deprecated typedef
FL_DEPRECATED("Use int instead")
typedef int deprecated_int_type;

TEST_CASE("fl::deprecated_typedef_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    deprecated_int_type value = 42;
    CHECK_EQ(value, 42);
    FL_DISABLE_WARNING_POP
}

// Test deprecated variable
FL_DEPRECATED("Use new_constant instead")
static const int OLD_CONSTANT = 100;

static const int NEW_CONSTANT = 200;

TEST_CASE("fl::deprecated_variable_usage") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    CHECK_EQ(OLD_CONSTANT, 100);
    FL_DISABLE_WARNING_POP
    CHECK_EQ(NEW_CONSTANT, 200);
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

TEST_CASE("fl::deprecated_template_function") {
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_DEPRECATED_DECLARATIONS
    CHECK_EQ(oldTemplateFunction(5), 10);
    CHECK_EQ(oldTemplateFunction(3.0), 6.0);
    FL_DISABLE_WARNING_POP

    CHECK_EQ(newTemplateFunction(5), 15);
    CHECK_EQ(newTemplateFunction(3.0), 9.0);
}
