#include "test.h"
#include "fl/functional.h"
#include "fl/function.h"
#include "fl/ptr.h"
#include "fl/scoped_ptr.h"

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



// Test fl::invoke with fl::scoped_ptr smart pointers
TEST_CASE("fl::invoke with scoped_ptr smart pointers") {
    struct TestScopedPtrClass {
        int value = 42;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int add(int x) const { return value + x; }
        int multiply(int x) { return value * x; }
    };

    // Test with scoped_ptr
    fl::scoped_ptr<TestScopedPtrClass> scopedPtr(new TestScopedPtrClass);

    // Member function: const getter
    CHECK_EQ(42, fl::invoke(&TestScopedPtrClass::getValue, scopedPtr));

    // Member function: setter
    fl::invoke(&TestScopedPtrClass::setValue, scopedPtr, 123);
    CHECK_EQ(123, scopedPtr->value);

    // Member function with additional arg, const
    CHECK_EQ(133, fl::invoke(&TestScopedPtrClass::add, scopedPtr, 10));

    // Member function with additional arg, non-const
    CHECK_EQ(246, fl::invoke(&TestScopedPtrClass::multiply, scopedPtr, 2));

    // Member data pointer access and modification
    CHECK_EQ(123, fl::invoke(&TestScopedPtrClass::value, scopedPtr));
    fl::invoke(&TestScopedPtrClass::value, scopedPtr) = 999;
    CHECK_EQ(999, scopedPtr->value);

    // Test with custom deleter
    struct CustomDeleter {
        void operator()(TestScopedPtrClass* ptr) {
            delete ptr;
        }
    };
    
    fl::scoped_ptr<TestScopedPtrClass, CustomDeleter> customScopedPtr(new TestScopedPtrClass, CustomDeleter{});
    
    // Member function with custom deleter scoped_ptr
    CHECK_EQ(42, fl::invoke(&TestScopedPtrClass::getValue, customScopedPtr));
    fl::invoke(&TestScopedPtrClass::setValue, customScopedPtr, 555);
    CHECK_EQ(555, customScopedPtr->value);
    CHECK_EQ(565, fl::invoke(&TestScopedPtrClass::add, customScopedPtr, 10));
} 

// Test fl::invoke with fl::function objects
TEST_CASE("fl::invoke with fl::function objects") {
    struct TestFunctionClass {
        int value = 100;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
        int add(int x) const { return value + x; }
        int multiply(int x) { return value * x; }
    };

    // 1. Test fl::function with free function
    fl::function<int(int, int)> free_func = add;
    auto result1 = fl::invoke(free_func, 10, 20);
    CHECK_EQ(30, result1);

    // 2. Test fl::function with lambda
    fl::function<int(int, int)> lambda_func = [](int a, int b) { return a * b; };
    auto result2 = fl::invoke(lambda_func, 6, 7);
    CHECK_EQ(42, result2);

    // 3. Test fl::function with member function bound to object
    TestFunctionClass obj;
    fl::function<int()> member_func = fl::function<int()>(&TestFunctionClass::getValue, &obj);
    auto result3 = fl::invoke(member_func);
    CHECK_EQ(100, result3);

    // 4. Test fl::function with member function bound to raw pointer
    TestFunctionClass* raw_ptr = &obj;
    fl::function<void(int)> setter_func = fl::function<void(int)>(&TestFunctionClass::setValue, raw_ptr);
    fl::invoke(setter_func, 200);
    CHECK_EQ(200, obj.value);

    // 5. Test fl::function with member function bound to scoped_ptr
    fl::scoped_ptr<TestFunctionClass> scoped_ptr(new TestFunctionClass);
    scoped_ptr->setValue(300);
    
    // Create function bound to scoped_ptr
    fl::function<int()> scoped_getter = fl::function<int()>(&TestFunctionClass::getValue, scoped_ptr.get());
    auto result4 = fl::invoke(scoped_getter);
    CHECK_EQ(300, result4);

    // 6. Test fl::function with member function with args bound to scoped_ptr
    fl::function<int(int)> scoped_adder = fl::function<int(int)>(&TestFunctionClass::add, scoped_ptr.get());
    auto result5 = fl::invoke(scoped_adder, 50);
    CHECK_EQ(350, result5);

    // 7. Test fl::function with complex lambda capturing scoped_ptr
    auto complex_lambda = [&scoped_ptr](int multiplier) {
        return fl::invoke(&TestFunctionClass::multiply, scoped_ptr, multiplier);
    };
    fl::function<int(int)> complex_func = complex_lambda;
    auto result6 = fl::invoke(complex_func, 3);
    CHECK_EQ(900, result6);  // 300 * 3 = 900

    // 8. Test nested fl::invoke calls
    fl::function<int(int)> nested_func = [&scoped_ptr](int x) {
        // Use fl::invoke inside a function that's also invoked with fl::invoke
        return fl::invoke(&TestFunctionClass::add, scoped_ptr, x) * 2;
    };
    auto result7 = fl::invoke(nested_func, 25);
    CHECK_EQ(650, result7);  // (300 + 25) * 2 = 650
} 
