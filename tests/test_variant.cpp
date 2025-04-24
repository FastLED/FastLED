// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/ui.h"
#include "fl/variant.h"
#include "fl/str.h"

using namespace fl;

TEST_CASE("Variant tests") {
    // 1) Default is empty
    Variant<int, fl::Str> v;
    REQUIRE(v.isEmpty());
    REQUIRE(!v.is<int>());
    REQUIRE(!v.is<fl::Str>());

    // 2) Emplace a T
    v.emplaceT(123);
    REQUIRE(v.is<int>());
    REQUIRE(!v.is<fl::Str>());
    REQUIRE_EQ(*v.get<int>(), 123);

    // 3) Reset back to empty
    v.reset();
    REQUIRE(v.isEmpty());

    // 4) Emplace a U
    v.emplaceU("hello");
    REQUIRE(v.is<fl::Str>());
    REQUIRE(!v.is<int>());
    REQUIRE_EQ(*v.get<fl::Str>(), fl::Str("hello"));

    // 5) Copy‐construct preserves the U
    Variant<int, fl::Str> v2(v);
    REQUIRE(v2.is<fl::Str>());

    fl::Str* str_ptr = v2.get<fl::Str>();
    REQUIRE_NE(str_ptr, nullptr);
    REQUIRE_EQ(*str_ptr, fl::Str("hello"));
    const bool is_str = v2.is<fl::Str>();
    const bool is_int = v2.is<int>();

    CHECK(is_str);
    CHECK(!is_int);

#if 0
    // 6) Move‐construct leaves source empty
    Variant<int, fl::Str> v3(std::move(v2));
    REQUIRE(v3.is<fl::Str>());
    REQUIRE_EQ(v3.getU(), fl::Str("hello"));
    REQUIRE(v2.isEmpty());

    // 7) Copy‐assign
    Variant<int, fl::Str> v4;
    v4 = v3;
    REQUIRE(v4.is<fl::Str>());
    REQUIRE_EQ(v4.getU(), fl::Str("hello"));


    // 8) Swap two variants
    v4.emplaceT(7);
    v3.swap(v4);
    REQUIRE(v3.is<int>());
    REQUIRE_EQ(v3.getT(), 7);
    REQUIRE(v4.is<fl::Str>());
    REQUIRE_EQ(v4.getU(), fl::Str("hello"));
#endif
}
