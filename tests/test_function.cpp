
// g++ --std=c++11 test.cpp

#include "test.h"


#include "fl/function.h"
#include "fl/function_list.h"

using namespace fl;


// Free function for testing
static int add(int a, int b) {
    return a + b;
}

struct Foo {
    int value = 0;
    void set(int v) { value = v; }
    int get() const { return value; }
};

struct Mult {
    int operator()(int a, int b) const { return a * b; }
};

TEST_CASE("function<bool()> is empty by default and bool-convertible") {
    function<void()> f;
    REQUIRE(!f);
}

TEST_CASE("Test function with lambda") {
    function<int(int,int)> f = [](int a, int b) { return a + b; };
    REQUIRE(f);
    REQUIRE(f(2, 3) == 5);
}

TEST_CASE("Test function with free function pointer") {
    function<int(int,int)> f(add);
    REQUIRE(f);
    REQUIRE(f(4, 6) == 10);
}

TEST_CASE("Test function with functor object") {
    Mult m;
    function<int(int,int)> f(m);
    REQUIRE(f);
    REQUIRE(f(3, 7) == 21);
}

TEST_CASE("Test function with non-const member function") {
    Foo foo;
    function<void(int)> fset(&Foo::set, &foo);
    REQUIRE(fset);
    fset(42);
    REQUIRE(foo.value == 42);
}

TEST_CASE("Test function with const member function") {
    Foo foo;
    foo.value = 99;
    function<int()> fget(&Foo::get, &foo);
    REQUIRE(fget);
    REQUIRE(fget() == 99);
}

TEST_CASE("Void free function test") {
    function<void(float)> f = [](float) { /* do nothing */ };
    REQUIRE(f);
    f(1);
}


TEST_CASE("Copy and move semantics") {
    function<int(int,int)> orig = [](int a, int b) { return a - b; };
    REQUIRE(orig(10, 4) == 6);

    // Copy
    function<int(int,int)> copy = orig;
    REQUIRE(copy);
    REQUIRE(copy(8, 3) == 5);

    // Move
    function<int(int,int)> moved = std::move(orig);
    REQUIRE(moved);
    REQUIRE(moved(7, 2) == 5);
    REQUIRE(!orig);
}

TEST_CASE("Function list void float") {
    FunctionList<float> fl;
    fl.add([](float) { /* do nothing */ });
    fl.invoke(1);
}

TEST_CASE("Test clear() method") {
    // Test with lambda
    function<int(int,int)> f = [](int a, int b) { return a + b; };
    REQUIRE(f);
    REQUIRE(f(2, 3) == 5);
    
    f.clear();
    REQUIRE(!f);
    
    // Test with free function
    function<int(int,int)> f2(add);
    REQUIRE(f2);
    REQUIRE(f2(4, 6) == 10);
    
    f2.clear();
    REQUIRE(!f2);
    
    // Test with member function
    Foo foo;
    function<void(int)> f3(&Foo::set, &foo);
    REQUIRE(f3);
    f3(42);
    REQUIRE(foo.value == 42);
    
    f3.clear();
    REQUIRE(!f3);
}

TEST_CASE("function alignment requirements") {
    // This test verifies that the function class alignment fix resolves WASM runtime errors
    // like "member access within misaligned address ... for type 'const union Storage', which requires 8 byte alignment"
    // 
    // The fix adds alignas(8) to both the function class and InlinedLambda::Storage union
    // to ensure proper alignment for function objects that require strict alignment.

    // Test that function class itself has proper alignment
    {
        using TestFunction = fl::function<void(int)>;
        
        // Verify the class alignment is at least 8 bytes
        REQUIRE_GE(alignof(TestFunction), 8);
        
        TestFunction f1;
        TestFunction f2;
        
        // Check that function objects are properly aligned in memory
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f1) % 8, 0);
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f2) % 8, 0);
    }
    
    // Test InlinedLambda storage alignment with lambda requiring alignment
    {
        using CallbackFunction = fl::function<void(int)>;
        
        // Create a lambda that might require strict alignment
        auto aligned_lambda = [](int x) {
            // Use some operations that might require alignment
            double aligned_data[2] = {3.14159, 2.71828};
            (void)x;
            (void)aligned_data;
        };
        
        CallbackFunction func = aligned_lambda;
        REQUIRE(func);
        
        // Test that the function can be called without alignment errors
        func(42); // Should not crash or trigger alignment errors
        
        // Test copy construction (this was failing in WASM)
        CallbackFunction func_copy = func;
        REQUIRE(func_copy);
        func_copy(84); // Should work without alignment errors
        
        // Test assignment
        CallbackFunction func_assigned;
        func_assigned = func;
        REQUIRE(func_assigned);
        func_assigned(126); // Should work without alignment errors
    }
    
    // Test array of functions to ensure consistent alignment
    {
        using TestFunction = fl::function<int(int, int)>;
        
        TestFunction functions[5];
        
        // Each function in the array should be properly aligned
        for (int i = 0; i < 5; ++i) {
            REQUIRE_EQ(reinterpret_cast<uintptr_t>(&functions[i]) % 8, 0);
            
            // Assign different types of callables
            if (i % 2 == 0) {
                functions[i] = [i](int a, int b) { return a + b + i; };
            } else {
                functions[i] = add;
            }
            
            REQUIRE(functions[i]);
            REQUIRE_EQ(functions[i](10, 20), (i % 2 == 0) ? (30 + i) : 30);
        }
    }
    
    // Test heap-allocated functions maintain alignment
    {
        using TestFunction = fl::function<double(double)>;
        
        auto* heap_function = new TestFunction([](double x) { return x * 2.0; });
        
        // Even heap-allocated functions should be properly aligned
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(heap_function) % 8, 0);
        
        REQUIRE(*heap_function);
        REQUIRE_EQ((*heap_function)(3.14159), 6.28318);
        
        delete heap_function;
    }
    
    // Test the specific case that was failing - function objects in containers
    {
        using CallbackFunction = fl::function<void(int)>;
        
        fl::vector<CallbackFunction> callbacks;
        
        // Add several function objects
        for (int i = 0; i < 3; ++i) {
            callbacks.push_back([i](int x) {
                // Simple lambda that might trigger alignment issues
                (void)x;
                (void)i;
            });
        }
        
        // Test that all callbacks work without alignment errors
        for (size_t i = 0; i < callbacks.size(); ++i) {
            REQUIRE(callbacks[i]);
            callbacks[i](static_cast<int>(i)); // Should not crash
        }
        
        // Test copying the vector (triggers copy construction)
        fl::vector<CallbackFunction> callbacks_copy = callbacks;
        for (size_t i = 0; i < callbacks_copy.size(); ++i) {
            REQUIRE(callbacks_copy[i]);
            callbacks_copy[i](static_cast<int>(i)); // Should not crash
        }
    }
    
    // Test alignment with different function signatures
    {
        // Test various function signatures that might have different alignment requirements
        fl::function<void()> f1([]() {});
        fl::function<int(int)> f2([](int x) { return x * 2; });
        fl::function<double(double, double)> f3([](double a, double b) { return a + b; });
        fl::function<fl::string(const fl::string&)> f4([](const fl::string& s) { return s + "_suffix"; });
        
        // All should be properly aligned
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f1) % 8, 0);
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f2) % 8, 0);
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f3) % 8, 0);
        REQUIRE_EQ(reinterpret_cast<uintptr_t>(&f4) % 8, 0);
        
        // All should work correctly
        REQUIRE(f1); f1();
        REQUIRE(f2); REQUIRE_EQ(f2(5), 10);
        REQUIRE(f3); REQUIRE_EQ(f3(1.5, 2.5), 4.0);
        REQUIRE(f4); REQUIRE_EQ(f4("test"), "test_suffix");
    }
}

TEST_CASE("function alignment - WASM specific scenario") {
    // This test replicates the exact scenario that was failing in WASM:
    // WasmFetchCallbackManager storing function objects in HashMap and copying them
    // This was triggering: "member access within misaligned address 0x129d4824 for type 'const union Storage'"
    
    using CallbackFunction = fl::function<void(int)>;
    using CallbackMap = fl::HashMap<fl::u32, CallbackFunction>;
    
    CallbackMap callback_storage;
    
    // Test storing and retrieving callbacks (similar to WasmFetchCallbackManager)
    {
        fl::u32 id = 123;
        
        // Create a callback that might require alignment
        CallbackFunction callback = [](int response_code) {
            // Simulate processing a response with potential alignment-sensitive operations
            double result = static_cast<double>(response_code) * 3.14159;
            (void)result; // Avoid unused variable warning
        };
        
        // Store the callback (this creates copies)
        callback_storage[id] = callback;
        REQUIRE_EQ(callback_storage.size(), 1);
        
        // Retrieve and call the callback (this was failing in WASM)
        auto it = callback_storage.find(id);
        REQUIRE_NE(it, callback_storage.end());
        
        const CallbackFunction& stored_callback = it->second;
        REQUIRE(stored_callback);
        
        // This call was triggering alignment errors in WASM
        stored_callback(200); // Should not crash
    }
    
    // Test copying the HashMap (triggers more copy construction)
    {
        CallbackMap callback_storage_copy = callback_storage;
        REQUIRE_EQ(callback_storage_copy.size(), 1);
        
        auto it = callback_storage_copy.find(123);
        REQUIRE_NE(it, callback_storage_copy.end());
        
        const CallbackFunction& copied_callback = it->second;
        REQUIRE(copied_callback);
        copied_callback(404); // Should not crash
    }
    
    // Test multiple callbacks
    {
        for (fl::u32 i = 1; i <= 5; ++i) {
            callback_storage[i] = [i](int code) {
                // Each lambda captures different data
                double calculation = static_cast<double>(code + i) / 2.0;
                (void)calculation;
            };
        }
        
        // Call all callbacks
        for (fl::u32 i = 1; i <= 5; ++i) {
            auto it = callback_storage.find(i);
            REQUIRE_NE(it, callback_storage.end());
            it->second(static_cast<int>(i * 100));
        }
    }
}
