
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/atomic.h"

TEST_CASE("atomic") {
  fl::atomic<int> atomic(0);
  atomic.store(1);
  REQUIRE(atomic.load() == 1);
  atomic.store(2);
  REQUIRE(atomic.load() == 2);
  atomic.store(3);
  REQUIRE(atomic.load() == 3);
}
