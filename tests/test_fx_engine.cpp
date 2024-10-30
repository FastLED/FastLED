
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN


#include "doctest.h"
#include "fx/fx.h"
#include "fx/fx_engine.h"
#include "FastLED.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

FASTLED_SMART_REF(MockFx);

class MockFx : public Fx {
public:
    MockFx(uint16_t numLeds, CRGB color) : Fx(numLeds), mColor(color) {}

    void draw(DrawContext ctx) override {
        mLastDrawTime = ctx.now;
        for (uint16_t i = 0; i < mNumLeds; ++i) {
            ctx.leds[i] = mColor;
        }
    }

    const char* fxName(int) const override { return "MockFx"; }

private:
    CRGB mColor;
    uint32_t mLastDrawTime = 0;
};

TEST_CASE("test_fx_engine") {
    constexpr uint16_t NUM_LEDS = 10;
    FxEngine engine(NUM_LEDS);
    CRGB leds[NUM_LEDS];

    Ref<MockFx> redFx = MockFxRef::New(NUM_LEDS, CRGB::Red);
    Ref<MockFx> blueFx = MockFxRef::New(NUM_LEDS, CRGB::Blue);

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
            CHECK(leds[i] == CRGB::Red);
        }
    }


    SUBCASE("Transition") {
        bool ok = engine.nextFx(1000);
        if (!ok) {
            auto& effects = engine._getEffects();
            for (auto it = effects.begin(); it != effects.end(); ++it) {
                auto& fx = it->second;
                printf("fx: %s\n", fx->fxName(0));
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
