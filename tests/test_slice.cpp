
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/slice.h"
#include "fl/vector.h"

using namespace fl;

TEST_CASE("vector slice") {
    HeapVector<int> vec;

    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);

    Slice<int> slice(vec.data(), vec.size());

    REQUIRE_EQ(slice.length(), 4);
    REQUIRE_EQ(slice[0], 1);
    REQUIRE_EQ(slice[1], 2);
    REQUIRE_EQ(slice[2], 3);
    REQUIRE_EQ(slice[3], 4);


    Slice<int> slice2 = slice.slice(1, 3);
    REQUIRE_EQ(slice2.length(), 2);
    REQUIRE_EQ(slice2[0], 2);
    REQUIRE_EQ(slice2[1], 3);
}

#if 0
TEST_CASE("matrix slice") {
    int data[2][2] = {
        {1, 2},
        {3, 4}
    };

    point_xy<uint16_t> bottomLeft(0, 0);
    point_xy<uint16_t> topRight(1, 1);

    MatrixSlice<int, uint16_t> slice(
        &data[0][0], 2, 2, bottomLeft, topRight);

    point_xy<uint16_t> p1(0, 0);
    point_xy<uint16_t> p2(0, 1);
    point_xy<uint16_t> p3(1, 0);
    point_xy<uint16_t> p4(1, 1);

    int v1 = slice.at(p1);
    int v2 = slice.at(p2);
    int v3 = slice.at(p3);
    int v4 = slice.at(p4);

    REQUIRE_EQ(v1, 1);
    REQUIRE_EQ(v2, 2);
    REQUIRE_EQ(v3, 3);
    REQUIRE_EQ(v4, 4);
}

TEST_CASE("4x4 matrix slice") {
    int data[4][4] = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16}};

    point_xy<uint16_t> bottomLeft(1, 1);
    point_xy<uint16_t> topRight(2, 2);
    MatrixSlice<int, uint16_t> slice(
        &data[0][0], 4, 4, bottomLeft, topRight);

    // REQUIRE_EQ(slice(point_xy<uint16_t>(0, 0)), 6);
    // REQUIRE_EQ(slice(point_xy<uint16_t>(0, 1)), 7);
    // REQUIRE_EQ(slice(point_xy<uint16_t>(1, 0)), 10);
    // REQUIRE_EQ(slice(point_xy<uint16_t>(1, 1)), 11);

    point_xy<uint16_t> p1(0, 0);
    point_xy<uint16_t> p2(1, 1);
    point_xy<uint16_t> p3(2, 2);
    point_xy<uint16_t> p4(3, 3);

    int v1 = slice.at(p1);
    int v2 = slice.at(p2);
    int v3 = slice.at(p3);
    int v4 = slice.at(p4);

    REQUIRE_EQ(v1, 6);
    REQUIRE_EQ(v2, 7);
    REQUIRE_EQ(v3, 10);
    REQUIRE_EQ(v4, 11);
}

#endif  // 0
