
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "FastLED.h"
#include "cled_controller.h"
#include "platforms/wasm/strip_id_map.h"

class FakeCLedController : public CLEDController {
};

TEST_CASE("StripIdMap functionality") {
    SUBCASE("Adding and retrieving controllers") {
        CLEDController* controller1 = (CLEDController*)0x1234;
        CLEDController* controller2 = (CLEDController*)0x5678;

        int id1 = StripIdMap::addOrGetId(controller1);
        int id2 = StripIdMap::addOrGetId(controller2);

        CHECK_EQ(id1, 0);
        CHECK_EQ(id2, 1);

        CHECK_EQ(StripIdMap::getId(controller1), 0);
        CHECK_EQ(StripIdMap::getId(controller2), 1);

        CHECK_EQ(StripIdMap::getOwner(0), controller1);
        CHECK_EQ(StripIdMap::getOwner(1), controller2);
    }

    SUBCASE("Retrieving non-existent controller") {
        CLEDController* non_existent = (CLEDController*)0x9ABC;
        CHECK_EQ(StripIdMap::getId(non_existent), -1);
        CHECK_EQ(StripIdMap::getOwner(99), nullptr);
    }

    SUBCASE("Adding same controller multiple times") {
        CLEDController* controller = (CLEDController*)0xDEF0;
        int id1 = StripIdMap::addOrGetId(controller);
        int id2 = StripIdMap::addOrGetId(controller);
        CHECK_EQ(id1, id2);
    }

    SUBCASE("getOrFindByAddress functionality") {
        CLEDController* controller = (CLEDController*)0x2468;
        int id = StripIdMap::addOrGetId(controller);
        CHECK_EQ(StripIdMap::getOrFindByAddress((intptr_t)controller), id);
        CHECK_EQ(StripIdMap::getOrFindByAddress(0), -1);
    }
}

