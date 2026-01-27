
// g++ --std=c++11 test.cpp



#include "fl/stl/function.h"
#include "fl/stl/new.h"
#include "fl/stl/utility.h"
#include "doctest.h"
#include "fl/stl/move.h"



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

TEST_CASE("fl::function<bool()> is empty by default and bool-convertible") {
    fl::function<void()> f;
    FL_REQUIRE(!f);
}

TEST_CASE("Test function with lambda") {
    fl::function<int(int,int)> f = [](int a, int b) { return a + b; };
    FL_REQUIRE(f);
    FL_REQUIRE(f(2, 3) == 5);
}

TEST_CASE("Test function with free function pointer") {
    fl::function<int(int,int)> f(add);
    FL_REQUIRE(f);
    FL_REQUIRE(f(4, 6) == 10);
}

TEST_CASE("Test function with functor object") {
    Mult m;
    fl::function<int(int,int)> f(m);
    FL_REQUIRE(f);
    FL_REQUIRE(f(3, 7) == 21);
}

TEST_CASE("Test function with non-const member function") {
    Foo foo;
    fl::function<void(int)> fset(&Foo::set, &foo);
    FL_REQUIRE(fset);
    fset(42);
    FL_REQUIRE(foo.value == 42);
}

TEST_CASE("Test function with const member function") {
    Foo foo;
    foo.value = 99;
    fl::function<int()> fget(&Foo::get, &foo);
    FL_REQUIRE(fget);
    FL_REQUIRE(fget() == 99);
}

TEST_CASE("Void free function test") {
    fl::function<void(float)> f = [](float) { /* do nothing */ };
    FL_REQUIRE(f);
    f(1);
}


TEST_CASE("Copy and move semantics") {
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

TEST_CASE("Function list void float") {
    fl::function_list<void(float)> fl;
    fl.add([](float) { /* do nothing */ });
    fl.invoke(1.0f);
}

TEST_CASE("Test clear() method") {
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
