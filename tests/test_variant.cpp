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
    REQUIRE(!v.isT());
    REQUIRE(!v.isU());

    // 2) Emplace a T
    v.emplaceT(123);
    REQUIRE(v.isT());
    REQUIRE(!v.isU());
    REQUIRE_EQ(v.getT(), 123);

    // 3) Reset back to empty
    v.reset();
    REQUIRE(v.isEmpty());

    // 4) Emplace a U
    v.emplaceU("hello");
    REQUIRE(v.isU());
    REQUIRE(!v.isT());
    REQUIRE_EQ(v.getU(), fl::Str("hello"));

    // 5) Copy‐construct preserves the U
    Variant<int, fl::Str> v2(v);
    REQUIRE(v2.isU());
    REQUIRE_EQ(v2.getU(), fl::Str("hello"));

#if 0
    // 6) Move‐construct leaves source empty
    Variant<int, fl::Str> v3(std::move(v2));
    REQUIRE(v3.isU());
    REQUIRE_EQ(v3.getU(), fl::Str("hello"));
    REQUIRE(v2.isEmpty());

    // 7) Copy‐assign
    Variant<int, fl::Str> v4;
    v4 = v3;
    REQUIRE(v4.isU());
    REQUIRE_EQ(v4.getU(), fl::Str("hello"));


    // 8) Swap two variants
    v4.emplaceT(7);
    v3.swap(v4);
    REQUIRE(v3.isT());
    REQUIRE_EQ(v3.getT(), 7);
    REQUIRE(v4.isU());
    REQUIRE_EQ(v4.getU(), fl::Str("hello"));
#endif
}
