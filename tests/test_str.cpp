
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "str.h"
#include "fixed_vector.h"

TEST_CASE("str") {
    FixedVector<Str, 10> vec;
    vec.push_back(Str("hello"));
}
