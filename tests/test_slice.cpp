
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/slice.h"
#include "fl/unused.h"
#include "fl/vector.h"
#include "test.h"

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

TEST_CASE("matrix compile") {
    int data[2][2] = {{1, 2}, {3, 4}};

    // Window from (0,0) up to (1,1)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           2,           // data width
                           2,           // data height
                           0, 0,        // bottom-left x,y
                           1, 1         // top-right x,y
    );

    FASTLED_UNUSED(slice);
    // just a compile‐time smoke test
}

TEST_CASE("matrix slice returns correct values") {
    int data[2][2] = {{1, 2}, {3, 4}};

    // Window from (0,0) up to (1,1)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           2,           // data width
                           2,           // data height
                           0, 0,        // bottom-left x,y
                           1, 1         // top-right x,y
    );

    // sanity‐check each element
    REQUIRE_EQ(slice(0, 0), data[0][0]);
    REQUIRE_EQ(slice(1, 0), data[0][1]);
    REQUIRE_EQ(slice(0, 1), data[1][0]);
    REQUIRE_EQ(slice(1, 1), data[1][1]);

    // Require that the [][] operator works the same as the data
    REQUIRE_EQ(slice[0][0], data[0][0]);
    REQUIRE_EQ(slice[0][1], data[0][1]);
    REQUIRE_EQ(slice[1][0], data[1][0]);
    REQUIRE_EQ(slice[1][1], data[1][1]);
}

TEST_CASE("4x4 matrix slice returns correct values") {
    int data[4][4] = {
        {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}};

    // Take a 2×2 window from (1,1) up to (2,2)
    MatrixSlice<int> slice(&data[0][0], // data pointer
                           4,           // data width
                           4,           // data height
                           1, 1,        // bottom-left x,y
                           2, 2         // top-right x,y
    );

    // test array access
    REQUIRE_EQ(slice[0][0], data[1][1]);
    REQUIRE_EQ(slice[0][1], data[1][2]);
    REQUIRE_EQ(slice[1][0], data[2][1]);
    REQUIRE_EQ(slice[1][1], data[2][2]);

    // Remember that array access is row-major, so data[y][x] == slice(x,y)
    REQUIRE_EQ(slice(0, 0), data[1][1]);
    REQUIRE_EQ(slice(1, 0), data[1][2]);
    REQUIRE_EQ(slice(0, 1), data[2][1]);
    REQUIRE_EQ(slice(1, 1), data[2][2]);
}
