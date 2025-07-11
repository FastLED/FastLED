#include "test.h"
#include "fl/functional.h"
#include "fl/function.h"

using namespace fl;

// Test data and functions
static int add(int a, int b) {
    return a + b;
}

static int multiply(int a, int b) {
    return a * b;
}

struct TestClass {
    int value = 42;
    
    int getValue() const {
        return value;
    }
    
    void setValue(int v) {
        value = v;
    }
    
    int add(int x) const {
        return value + x;
    }
    
    int multiply(int x) {
        return value * x;
    }
};

struct Functor {
    int operator()(int a, int b) const {
        return a * b + 10;
    }
};

// Test free function pointers
TEST_CASE("fl::invoke with free function pointers") {
    // Test function pointer
    auto result1 = fl::invoke(add, 5, 3);
    CHECK_EQ(8, result1);
    
    auto result2 = fl::invoke(multiply, 4, 7);
    CHECK_EQ(28, result2);
    
    // Test function reference
    auto result3 = fl::invoke(&add, 10, 20);
    CHECK_EQ(30, result3);
}

// Test member function pointers with object references
TEST_CASE("fl::invoke with member function pointers and objects") {
    TestClass obj;
    
    // Test const member function with object reference
    auto result1 = fl::invoke(&TestClass::getValue, obj);
    CHECK_EQ(42, result1);
    
    // Test non-const member function with object reference
    fl::invoke(&TestClass::setValue, obj, 100);
    CHECK_EQ(100, obj.value);
    
    // Test member function with arguments
    auto result2 = fl::invoke(&TestClass::add, obj, 10);
    CHECK_EQ(110, result2);
    
    auto result3 = fl::invoke(&TestClass::multiply, obj, 3);
    CHECK_EQ(300, result3);
}

// Test member function pointers with pointers
TEST_CASE("fl::invoke with member function pointers and pointers") {
    TestClass obj;
    TestClass* ptr = &obj;
    
    // Test const member function with pointer
    auto result1 = fl::invoke(&TestClass::getValue, ptr);
    CHECK_EQ(42, result1);
    
    // Test non-const member function with pointer
    fl::invoke(&TestClass::setValue, ptr, 200);
    CHECK_EQ(200, obj.value);
    
    // Test member function with arguments and pointer
    auto result2 = fl::invoke(&TestClass::add, ptr, 15);
    CHECK_EQ(215, result2);
    
    auto result3 = fl::invoke(&TestClass::multiply, ptr, 2);
    CHECK_EQ(400, result3);
}

// Test member data pointers with object references
TEST_CASE("fl::invoke with member data pointers and objects") {
    TestClass obj;
    obj.value = 123;
    
    // Test member data access with object reference
    auto result1 = fl::invoke(&TestClass::value, obj);
    CHECK_EQ(123, result1);
    
    // Test member data modification
    fl::invoke(&TestClass::value, obj) = 456;
    CHECK_EQ(456, obj.value);
}

// Test member data pointers with pointers
TEST_CASE("fl::invoke with member data pointers and pointers") {
    TestClass obj;
    obj.value = 789;
    TestClass* ptr = &obj;
    
    // Test member data access with pointer
    auto result1 = fl::invoke(&TestClass::value, ptr);
    CHECK_EQ(789, result1);
    
    // Test member data modification with pointer
    fl::invoke(&TestClass::value, ptr) = 999;
    CHECK_EQ(999, obj.value);
}

// Test callable objects (functors, lambdas)
TEST_CASE("fl::invoke with callable objects") {
    // Test functor
    Functor f;
    auto result1 = fl::invoke(f, 5, 6);
    CHECK_EQ(40, result1);  // 5 * 6 + 10 = 40
    
    // Test lambda
    auto lambda = [](int a, int b) { return a - b; };
    auto result2 = fl::invoke(lambda, 10, 3);
    CHECK_EQ(7, result2);
    
    // Test lambda with capture
    int multiplier = 5;
    auto capturing_lambda = [multiplier](int x) { return x * multiplier; };
    auto result3 = fl::invoke(capturing_lambda, 8);
    CHECK_EQ(40, result3);
}

// Test edge cases
TEST_CASE("fl::invoke edge cases") {
    // Test with no arguments
    auto no_args = []() { return 42; };
    auto result1 = fl::invoke(no_args);
    CHECK_EQ(42, result1);
    
    // Test with const object
    const TestClass const_obj;
    auto result2 = fl::invoke(&TestClass::getValue, const_obj);
    CHECK_EQ(42, result2);
    
    // Test with temporary object
    auto result3 = fl::invoke(&TestClass::getValue, TestClass{});
    CHECK_EQ(42, result3);
    
    // Test with const pointer
    const TestClass const_obj2;
    const TestClass* const_ptr = &const_obj2;
    auto result4 = fl::invoke(&TestClass::getValue, const_ptr);
    CHECK_EQ(42, result4);
} 
