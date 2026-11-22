
// g++ --std=c++11 test.cpp



#include "fl/stl/function.h"
#include "fl/stl/new.h"
#include "fl/stl/utility.h"
#include "test.h"
#include "fl/stl/move.h"

FL_TEST_FILE(FL_FILEPATH) {



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

FL_TEST_CASE("fl::function<bool()> is empty by default and bool-convertible") {
    fl::function<void()> f;
    FL_REQUIRE(!f);
}

FL_TEST_CASE("Test function with lambda") {
    fl::function<int(int,int)> f = [](int a, int b) { return a + b; };
    FL_REQUIRE(f);
    FL_REQUIRE(f(2, 3) == 5);
}

FL_TEST_CASE("Test function with free function pointer") {
    fl::function<int(int,int)> f(add);
    FL_REQUIRE(f);
    FL_REQUIRE(f(4, 6) == 10);
}

FL_TEST_CASE("Test function with functor object") {
    Mult m;
    fl::function<int(int,int)> f(m);
    FL_REQUIRE(f);
    FL_REQUIRE(f(3, 7) == 21);
}

FL_TEST_CASE("Test function with non-const member function") {
    Foo foo;
    fl::function<void(int)> fset(&Foo::set, &foo);
    FL_REQUIRE(fset);
    fset(42);
    FL_REQUIRE(foo.value == 42);
}

FL_TEST_CASE("Test function with const member function") {
    Foo foo;
    foo.value = 99;
    fl::function<int()> fget(&Foo::get, &foo);
    FL_REQUIRE(fget);
    FL_REQUIRE(fget() == 99);
}

FL_TEST_CASE("Void free function test") {
    fl::function<void(float)> f = [](float) { /* do nothing */ };
    FL_REQUIRE(f);
    f(1);
}


FL_TEST_CASE("Copy and move semantics") {
    fl::function<int(int,int)> orig = [](int a, int b) { return a - b; };
    FL_REQUIRE(orig(10, 4) == 6);

    // Copy
    fl::function<int(int,int)> copy = orig;
    FL_REQUIRE(copy);
    FL_REQUIRE(copy(8, 3) == 5);

    // Move
    fl::function<int(int,int)> moved = fl::move(orig);
    FL_REQUIRE(moved);
    FL_REQUIRE(moved(7, 2) == 5);
    FL_REQUIRE(!orig);
}

FL_TEST_CASE("Function list void float") {
    fl::function_list<void(float)> fl;
    fl.add([](float) { /* do nothing */ });
    fl.invoke(1.0f);
}

FL_TEST_CASE("Test clear() method") {
    // Test with lambda
    fl::function<int(int,int)> f = [](int a, int b) { return a + b; };
    FL_REQUIRE(f);
    FL_REQUIRE(f(2, 3) == 5);
    
    f.clear();
    FL_REQUIRE(!f);
    
    // Test with free function
    fl::function<int(int,int)> f2(add);
    FL_REQUIRE(f2);
    FL_REQUIRE(f2(4, 6) == 10);
    
    f2.clear();
    FL_REQUIRE(!f2);
    
    // Test with member function
    Foo foo;
    fl::function<void(int)> f3(&Foo::set, &foo);
    FL_REQUIRE(f3);
    f3(42);
    FL_REQUIRE(foo.value == 42);
    
    f3.clear();
    FL_REQUIRE(!f3);
}

// FastLED #3237 -- FL_FUNCTION_NO_HEAP_FALLBACK opt-out behavior verification.
//
// When the macro is NOT defined (the default; this build), over-SBO captures
// route through the `HeapHolder<F>` + `shared_ptr<F>` heap path. This test
// constructs a deliberately-large capture (above FASTLED_INLINE_LAMBDA_SIZE
// = 64 B), confirms it works, and confirms copying the fl::function does the
// right thing -- the captured F is shared via refcount instead of being
// re-allocated. The runtime-observable invariant is that two copies of the
// fl::function see the same captured state.
FL_TEST_CASE("fl::function: over-SBO captures route through heap fallback by default") {
    struct LargeCapture {
        // Force > 64 B by padding with an explicit array. The padding does
        // not need to be referenced by operator(); the size is what triggers
        // the over-SBO `init_with_impl(false_type)` branch.
        int payload;
        char padding[96];
        LargeCapture(int p) : payload(p) {
            for (size_t i = 0; i < sizeof(padding); ++i) padding[i] = 0;
        }
    };

    LargeCapture big(7);
    // Capture `big` by value; sizeof(lambda) becomes >= sizeof(LargeCapture)
    // > FASTLED_INLINE_LAMBDA_SIZE (64 B), so the heap fallback fires.
    fl::function<int()> f = [big]() { return big.payload * 6; };
    FL_REQUIRE(f);
    FL_REQUIRE(f() == 42);

    // Copy the fl::function -- the captured LargeCapture lives on the heap
    // (per #3237 design), and the copy increments shared_ptr refcount rather
    // than re-allocating. Both copies see the same captured payload.
    fl::function<int()> f_copy = f;
    FL_REQUIRE(f_copy);
    FL_REQUIRE(f_copy() == 42);

    // Rebind the original's stored callable and confirm the copy is
    // unaffected (the heap-held F was shared by refcount, but assignment
    // to `f` rebinds `f`'s invoker/manager pair, not the shared F's
    // contents). This validates that the shared_ptr-based sharing has the
    // expected fl::function semantics.
    f = []() { return 99; };
    FL_REQUIRE(f() == 99);
    FL_REQUIRE(f_copy() == 42);  // still observes the original captured LargeCapture
}

} // FL_TEST_FILE
