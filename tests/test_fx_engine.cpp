
// g++ --std=c++11 test.cpp

#include "test.h"


#include "test.h"
#include "fx/fx.h"
#include "fx/fx_engine.h"
#include "fx/fx2d.h"
#include "fl/vector.h"
#include "FastLED.h"

using namespace fl;


FASTLED_SMART_PTR(MockFx);

class MockFx : public Fx {
public:
    MockFx(uint16_t numLeds, CRGB color) : Fx(numLeds), mColor(color) {}

    void draw(DrawContext ctx) override {
        mLastDrawTime = ctx.now;
        for (uint16_t i = 0; i < mNumLeds; ++i) {
            ctx.leds[i] = mColor;
        }
    }

    Str fxName() const override { return "MockFx"; }

private:
    CRGB mColor;
    uint32_t mLastDrawTime = 0;
};

TEST_CASE("test_fx_engine") {
    constexpr uint16_t NUM_LEDS = 10;
    FxEngine engine(NUM_LEDS, false);
    CRGB leds[NUM_LEDS];

    MockFxPtr redFx = fl::make_shared<MockFx>(NUM_LEDS, CRGB::Red);
    MockFxPtr blueFx = fl::make_shared<MockFx>(NUM_LEDS, CRGB::Blue);

    int id0 = engine.addFx(redFx);
    int id1 = engine.addFx(blueFx);

    REQUIRE_EQ(0, id0);
    REQUIRE_EQ(1, id1);

    SUBCASE("Initial state") {
        int currId = engine.getCurrentFxId();
        CHECK(currId == id0);
        const bool ok = engine.draw(0, leds);
        CHECK(ok);
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            // CHECK(leds[i] == CRGB::Red);
            bool is_red = leds[i] == CRGB::Red;
            if (!is_red) {
                Str err = leds[i].toString();
                printf("leds[%d] is not red, was instead: %s\n", i, err.c_str());
                CHECK(is_red);
            }
        }
    }


    SUBCASE("Transition") {
        bool ok = engine.nextFx(1000);
        if (!ok) {
            auto& effects = engine._getEffects();
            for (auto it = effects.begin(); it != effects.end(); ++it) {
                auto& fx = it->second;
                printf("fx: %s\n", fx->fxName().c_str());
            }
            FAIL("Failed to transition to next effect");
        }
        REQUIRE(ok);
        
        // Start of transition
        ok = engine.draw(0, leds);
        REQUIRE(ok);
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            REQUIRE(leds[i] == CRGB::Red);
        }

        // Middle of transition
        REQUIRE(engine.draw(500, leds));
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            REQUIRE(leds[i].r == 128);
            REQUIRE(leds[i].g == 0);
            REQUIRE(leds[i].b == 127);
        }

        // End of transition
        REQUIRE(engine.draw(1000, leds));
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            CHECK(leds[i] == CRGB::Blue);
        }
    }

    SUBCASE("Transition with 0 time duration") {
        engine.nextFx(0);
        engine.draw(0, leds);
        for (uint16_t i = 0; i < NUM_LEDS; ++i) {
            CHECK(leds[i] == CRGB::Blue);
        }
    }

}



TEST_CASE("test_transition") {

    SUBCASE("Initial state") {
        Transition transition;
        CHECK(transition.getProgress(0) == 0);
        CHECK_FALSE(transition.isTransitioning(0));
    }

    SUBCASE("Start transition") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.isTransitioning(100));
        CHECK(transition.isTransitioning(1099));
        CHECK_FALSE(transition.isTransitioning(1100));
    }

    SUBCASE("Progress calculation") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(100) == 0);
        CHECK(transition.getProgress(600) == 127);
        CHECK(transition.getProgress(1100) == 255);
    }

    SUBCASE("Progress before start time") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(50) == 0);
    }

    SUBCASE("Progress after end time") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.getProgress(1200) == 255);
    }

    SUBCASE("Multiple transitions") {
        Transition transition;
        transition.start(100, 1000);
        CHECK(transition.isTransitioning(600));
        
        transition.start(2000, 500);
        CHECK_FALSE(transition.isTransitioning(1500));
        CHECK(transition.isTransitioning(2200));
        CHECK(transition.getProgress(2250) == 127);
    }

    SUBCASE("Zero duration transition") {
        Transition transition;
        transition.start(100, 0);
        CHECK_FALSE(transition.isTransitioning(100));
        CHECK(transition.getProgress(99) == 0);
        CHECK(transition.getProgress(100) == 255);
        CHECK(transition.getProgress(101) == 255);
    }
}


// Simple Fx2d object which writes a single red pixel to the first LED
// with the red component being the intensity of the frame counter.
class Fake2d : public Fx2d {
  public:
    Fake2d() : Fx2d(XYMap::constructRectangularGrid(1,1)) {}

    void draw(DrawContext context) override {
        CRGB c = mColors[mFrameCounter % mColors.size()];
        context.leds[0] = c;
        mFrameCounter++;
    }

    bool hasFixedFrameRate(float *fps) const override {
        *fps = 1;
        return true;
    }

    Str fxName() const override { return "Fake2d"; }
    uint8_t mFrameCounter = 0;
    FixedVector<CRGB, 5> mColors;
};

TEST_CASE("test_fixed_fps") {
    Fake2d fake;
    fake.mColors.push_back(CRGB(0, 0, 0));
    fake.mColors.push_back(CRGB(255, 0, 0));

    CRGB leds[1];
    bool interpolate = true;
    FxEngine engine(1, interpolate);
    int id = engine.addFx(fake);
    CHECK_EQ(0, id);
    engine.draw(0, &leds[0]);
    CHECK_EQ(1, fake.mFrameCounter);
    CHECK_EQ(leds[0], CRGB(0, 0, 0));
    engine.draw(500, &leds[0]);
    CHECK_EQ(2, fake.mFrameCounter);
    CHECK_EQ(leds[0], CRGB(127, 0, 0));
}
