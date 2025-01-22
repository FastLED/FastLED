
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/template_magic.h"
#include "fl/namespace.h"
#include <type_traits>

class Base {};
class Derived : public Base {};

TEST_CASE("is_base_of") {
    CHECK(fl::is_base_of<Base, Derived>::value);
    CHECK_FALSE(fl::is_base_of<Derived, Base>::value);
}

