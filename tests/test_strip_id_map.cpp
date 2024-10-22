
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"

#include "FastLED.h"
#include "cled_controller.h"
#include "platforms/wasm/strip_id_map.h"

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
    FakeCLedController fake_controller;
    int id = StripIdMap::addOrGetId(&fake_controller);
    CHECK(id == 0);
    CLEDController *owner = StripIdMap::getOwner(id);
    CLEDController *match = &fake_controller;
    printf("Owner: %p, Match: %p\n", owner, match);
    CHECK_EQ(owner, match);

#if 0
    CHECK(StripIdMap::getId(&fake_controller) == 0);
    CHECK(StripIdMap::getOrFindByAddress(
              reinterpret_cast<uintptr_t>(&fake_controller)) == 0);
    CHECK(StripIdMap::getOwnerByAddress(reinterpret_cast<uintptr_t>(
              &fake_controller)) == &fake_controller);
#endif
}
