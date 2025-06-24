// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/ui.h"
#include "fl/variant.h"
#include "fl/optional.h"
#include "fl/str.h"

using namespace fl;

TEST_CASE("Variant tests") {
    // 1) Default is empty
    Variant<int, fl::string> v;
    REQUIRE(v.empty());
    REQUIRE(!v.is<int>());
    REQUIRE(!v.is<fl::string>());

    // 2) Emplace a T
    v = 123;
    REQUIRE(v.is<int>());
    REQUIRE(!v.is<fl::string>());
    REQUIRE_EQ(*v.ptr<int>(), 123);

    // 3) Reset back to empty
    v.reset();
    REQUIRE(v.empty());


    // 4) Emplace a U
    v = fl::string("hello");
    REQUIRE(v.is<fl::string>());
    REQUIRE(!v.is<int>());
    REQUIRE(v.equals(fl::string("hello")));


    // 5) Copy‐construct preserves the U
    Variant<int, fl::string> v2(v);
    REQUIRE(v2.is<fl::string>());

    fl::string* str_ptr = v2.ptr<fl::string>();
    REQUIRE_NE(str_ptr, nullptr);
    REQUIRE_EQ(*str_ptr, fl::string("hello"));
    const bool is_str = v2.is<fl::string>();
    const bool is_int = v2.is<int>();

    CHECK(is_str);
    CHECK(!is_int);

#if 0
    // 6) Move‐construct leaves source empty
    Variant<int, fl::string> v3(std::move(v2));
    REQUIRE(v3.is<fl::string>());
    REQUIRE_EQ(v3.getU(), fl::string("hello"));
    REQUIRE(v2.isEmpty());

    // 7) Copy‐assign
    Variant<int, fl::string> v4;
    v4 = v3;
    REQUIRE(v4.is<fl::string>());
    REQUIRE_EQ(v4.getU(), fl::string("hello"));


    // 8) Swap two variants
    v4.emplaceT(7);
    v3.swap(v4);
    REQUIRE(v3.is<int>());
    REQUIRE_EQ(v3.getT(), 7);
    REQUIRE(v4.is<fl::string>());
    REQUIRE_EQ(v4.getU(), fl::string("hello"));
#endif
}

TEST_CASE("Variant") {
    // 1) Default is empty
    Variant<int, fl::string, double> v;
    REQUIRE(v.empty());
    REQUIRE(!v.is<int>());
    REQUIRE(!v.is<fl::string>());
    REQUIRE(!v.is<double>());

    // 2) Construct with a value
    Variant<int, fl::string, double> v1(123);
    REQUIRE(v1.is<int>());
    REQUIRE(!v1.is<fl::string>());
    REQUIRE(!v1.is<double>());
    REQUIRE_EQ(*v1.ptr<int>(), 123);

    // 3) Construct with a different type
    Variant<int, fl::string, double> v2(fl::string("hello"));
    REQUIRE(!v2.is<int>());
    REQUIRE(v2.is<fl::string>());
    REQUIRE(!v2.is<double>());
    REQUIRE_EQ(*v2.ptr<fl::string>(), fl::string("hello"));

    // 4) Copy construction
    Variant<int, fl::string, double> v3(v2);
    REQUIRE(v3.is<fl::string>());
    REQUIRE(v3.equals(fl::string("hello")));

    // 5) Assignment
    v = v1;
    REQUIRE(v.is<int>());
    REQUIRE_EQ(*v.ptr<int>(), 123);

    // 6) Reset
    v.reset();
    REQUIRE(v.empty());

    // 7) Assignment of a value
    v = 3.14;
    REQUIRE(v.is<double>());
    // REQUIRE_EQ(v.get<double>(), 3.14);
    REQUIRE_EQ(*v.ptr<double>(), 3.14);

    // 8) Visitor pattern
    struct TestVisitor {
        int result = 0;
        
        void accept(int value) { result = value; }
        void accept(const fl::string& value) { result = value.length(); }
        void accept(double value) { result = static_cast<int>(value); }
    };

    TestVisitor visitor;
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 3); // 3.14 truncated to 3

    v = fl::string("hello world");
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 11); // length of "hello world"

    v = 42;
    v.visit(visitor);
    REQUIRE_EQ(visitor.result, 42);
}


// TEST_CASE("Optional") {
//     Optional<int> opt;
//     REQUIRE(opt.empty());

//     opt = 42;
//     REQUIRE(!opt.empty());
//     REQUIRE_EQ(*opt.ptr(), 42);

//     Optional<int> opt2 = opt;
//     REQUIRE(!opt2.empty());
//     REQUIRE_EQ(*opt2.ptr(), 42);

//     opt2 = 100;
//     REQUIRE_EQ(*opt2.ptr(), 100);
// }
