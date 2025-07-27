#include "fl/tuple.h"
#include "fl/string.h"
#include "test.h"

TEST_CASE("BasicTupleCreation") {
    auto t = fl::make_tuple(42, "hello", 3.14f);
    
    REQUIRE_EQ(42, fl::get<0>(t));
    REQUIRE_EQ(std::string("hello"), std::string(fl::get<1>(t)));
    REQUIRE_EQ(3.14f, fl::get<2>(t));
}

TEST_CASE("TupleSize") {
    auto t1 = fl::make_tuple(1, 2, 3);
    auto t2 = fl::make_tuple();
    auto t3 = fl::make_tuple(1, "test");
    
    REQUIRE_EQ(3, fl::tuple_size<decltype(t1)>::value);
    REQUIRE_EQ(0, fl::tuple_size<decltype(t2)>::value);
    REQUIRE_EQ(2, fl::tuple_size<decltype(t3)>::value);
}

TEST_CASE("TupleElement") {
    using tuple_type = fl::tuple<int, fl::string, float>;
    
    REQUIRE((fl::is_same<fl::tuple_element<0, tuple_type>::type, int>::value));
    REQUIRE((fl::is_same<fl::tuple_element<1, tuple_type>::type, fl::string>::value));
    REQUIRE((fl::is_same<fl::tuple_element<2, tuple_type>::type, float>::value));
}

TEST_CASE("TupleMoveSemantics") {
    auto t1 = fl::make_tuple(42, fl::string("test"));
    auto t2 = fl::move(t1);
    
    REQUIRE_EQ(42, fl::get<0>(t2));
    REQUIRE_EQ(fl::string("test"), fl::get<1>(t2));
}