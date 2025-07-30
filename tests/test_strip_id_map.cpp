
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

// Create a fake WASM CLEDController that matches the WASM platform interface
namespace fl {
    class FakeCLedController : public ::CLEDController {
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
}

TEST_CASE("StripIdMap Simple Test") {
    StripIdMap::test_clear();
    fl::FakeCLedController fake_controller;
    
    // Cast to fl::CLEDController* for WASM StripIdMap interface
    fl::CLEDController *controller_ptr = reinterpret_cast<fl::CLEDController*>(&fake_controller);
    
    int id = StripIdMap::addOrGetId(controller_ptr);
    CHECK(id == 0);
    fl::CLEDController *owner = StripIdMap::getOwner(id);
    fl::CLEDController *match = controller_ptr;
    printf("Owner: %p, Match: %p\n", owner, match);
    CHECK_EQ(owner, match);
    CHECK(StripIdMap::getId(controller_ptr) == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(controller_ptr));
    CHECK(id == 0);
    id = StripIdMap::getOrFindByAddress(reinterpret_cast<uintptr_t>(&fake_controller.fake_spi));
    CHECK(id == 0);

}
