
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include "FastLED.h"
#include "cled_controller.h"
#include "platforms/wasm/strip_id_map.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

struct FakeSpi {
    int value = 0;
};

class FakeCLedController : public CLEDController {
  public:
    FakeSpi fake_spi;
    virtual void showColor(const CRGB &data, int nLeds,
                           uint8_t brightness) override {}

    virtual void show(const struct CRGB *data, int nLeds,
                      uint8_t brightness) override {}
    virtual void init() override {}
};

TEST_CASE("StripIdMap Simple Test") {
    StripIdMap::test_clear();
    FakeCLedController fake_controller;
    int id = StripIdMap::addOrGetId(&fake_controller);
    CHECK(id == 0);
    CLEDController *owner = StripIdMap::getOwner(id);
    CLEDController *match = &fake_controller;
    printf("Owner: %p, Match: %p\n", owner, match);
    CHECK_EQ(owner, match);
    CHECK(StripIdMap::getId(&fake_controller) == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(&fake_controller));
    CHECK(id == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(&fake_controller.fake_spi));
    CHECK(id == 0);

}
