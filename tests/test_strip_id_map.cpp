
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"

#include "FastLED.h"
#include "cled_controller.h"
#include "platforms/wasm/strip_id_map.h"
#include "fl/unused.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

struct FakeSpi {
    int value = 0;
};

class FakeCLedController : public CLEDController {
  public:
    FakeSpi fake_spi;
    virtual void showColor(const CRGB &data, int nLeds,
                           uint8_t brightness) override {
                            FL_UNUSED(data);
                            FL_UNUSED(nLeds);
                            FL_UNUSED(brightness);
                           }

    virtual void show(const struct CRGB *data, int nLeds,
                      uint8_t brightness) override {
                        FL_UNUSED(data);
                        FL_UNUSED(nLeds);
                        FL_UNUSED(brightness);
                      }
    virtual void init() override {}
};

TEST_CASE("StripIdMap Simple Test") {
    StripIdMap::test_clear();
    FakeCLedController fake_controller;
    int id = StripIdMap::addOrGetId(&fake_controller);
    CHECK(id == 0);
    CLEDController *owner = StripIdMap::getOwner(id);
    CLEDController *match = &fake_controller;
    CHECK_EQ(owner, match);
    CHECK(StripIdMap::getId(&fake_controller) == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(&fake_controller));
    CHECK(id == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(&fake_controller.fake_spi));
    CHECK(id == 0);

}
