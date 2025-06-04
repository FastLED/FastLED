
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/algorithm.h"
#include "fl/dbg.h"
#include "fl/vector.h"
#include "test.h"

using namespace fl;

TEST_CASE("reverse an int list") {
    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);

    fl::reverse(vec.begin(), vec.end());
    CHECK_EQ(vec[0], 5);
    CHECK_EQ(vec[1], 4);
    CHECK_EQ(vec[2], 3);
    CHECK_EQ(vec[3], 2);
    CHECK_EQ(vec[4], 1);
}
