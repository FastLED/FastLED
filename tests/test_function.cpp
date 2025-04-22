
// g++ --std=c++11 test.cpp

#include "test.h"


#include "fl/function.h"

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

TEST_CASE("Function<bool()> is empty by default and bool-convertible") {
    Function<void()> f;
    REQUIRE(!f);
}

TEST_CASE("Test function with lambda") {
    Function<int(int,int)> f = [](int a, int b) { return a + b; };
    REQUIRE(f);
    REQUIRE(f(2, 3) == 5);
}

TEST_CASE("Test function with free function pointer") {
    Function<int(int,int)> f(add);
    REQUIRE(f);
    REQUIRE(f(4, 6) == 10);
}

TEST_CASE("Test function with functor object") {
    Mult m;
    Function<int(int,int)> f(m);
    REQUIRE(f);
    REQUIRE(f(3, 7) == 21);
}

TEST_CASE("Test function with non-const member function") {
    Foo foo;
    Function<void(int)> fset(&Foo::set, &foo);
    REQUIRE(fset);
    fset(42);
    REQUIRE(foo.value == 42);
}

TEST_CASE("Test function with const member function") {
    Foo foo;
    foo.value = 99;
    Function<int()> fget(&Foo::get, &foo);
    REQUIRE(fget);
    REQUIRE(fget() == 99);
}

TEST_CASE("Copy and move semantics") {
    Function<int(int,int)> orig = [](int a, int b) { return a - b; };
    REQUIRE(orig(10, 4) == 6);

    // Copy
    Function<int(int,int)> copy = orig;
    REQUIRE(copy);
    REQUIRE(copy(8, 3) == 5);

    // Move
    Function<int(int,int)> moved = std::move(orig);
    REQUIRE(moved);
    REQUIRE(moved(7, 2) == 5);
    REQUIRE(!orig);
}