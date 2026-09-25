/// @file tests/fl/channels/slim_bridge_controller.cpp
/// @brief Conformance suite for fl::SlimBridgeController (#4581 T0)
///
/// SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, DriverTraits>
/// is the shared legacy addLeds()-style bridge controller: it is a plain
/// CLEDController-family object (not fl::Channel/fl::Channel-derived), so
/// it is driven the same way any other legacy chipset controller is --
/// `FastLED.addLeds(&controller, leds, NUM_LEDS)` followed by
/// `FastLED.show()`. `DriverTraits` supplies the concrete `IChannelDriver`
/// singleton (`instance()`) and the ChannelManager registration hook
/// (`registerWithManager()`), and the controller exposes the ChannelData it
/// last built via `channelData()` for inspection.

#include "FastLED.h"
#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/channels/manager.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {

namespace slim_bridge_test {

using DriverState = IChannelDriver::DriverState;

/// @brief Capturing mock driver -- records everything a conformance test
/// needs to see: how many times enqueue()/show() landed, the bytes/format
/// of the last ChannelData it received, and the reset_us the chipset timing
/// carried. State (READY/BUSY) and waitForReady()'s outcome are both
/// controllable so tests can drive the controller's ready-gating behavior.
class SlimBridgeMockDriver : public IChannelDriver {
public:
    int enqueueCount = 0;
    int showCount = 0;
    int pollCount = 0;
    DriverState::Value mState = DriverState::READY;

    fl::vector<u8> lastBytes;
    ChannelPixelFormat lastFormat = ChannelPixelFormat::Unknown;
    u32 lastResetUs = 0;
    const u8* lastDataPtr = nullptr;
    fl::size lastCapacity = 0;

    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }

    void enqueue(ChannelDataPtr data) FL_NO_EXCEPT override {
        ++enqueueCount;
        if (!data) {
            return;
        }
        const auto& bytes = data->getData();
        lastBytes.clear();
        for (fl::size i = 0; i < bytes.size(); ++i) {
            lastBytes.push_back(bytes[i]);
        }
        lastFormat = data->getPixelFormat();
        if (data->isClockless()) {
            lastResetUs = data->getTiming().reset_us;
        }
        lastDataPtr = bytes.data();
        lastCapacity = bytes.capacity();
    }

    void show() FL_NO_EXCEPT override { ++showCount; }

    DriverState poll() FL_NO_EXCEPT override {
        ++pollCount;
        return DriverState(mState);
    }

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("SLIM_BRIDGE_MOCK");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }
};

inline SlimBridgeMockDriver& mockDriverInstance() {
    static SlimBridgeMockDriver driver;
    return driver;
}

/// Counts actual ChannelManager::addDriver() calls made through
/// registerWithManager() -- used to assert registration is idempotent even
/// when multiple controller instances call it.
inline int& registerCallCount() {
    static int count = 0;
    return count;
}

/// Test DriverTraits: registers the shared mock singleton with the manager
/// exactly once, no matter how many controllers/instances invoke it.
struct TestDriverTraits {
    static IChannelDriver& instance() FL_NO_EXCEPT { return mockDriverInstance(); }

    static void registerWithManager() FL_NO_EXCEPT {
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;
        ++registerCallCount();
        ChannelManager::instance().addDriver(9500,
            fl::make_shared_no_tracking(mockDriverInstance()));
    }
};

/// Reset the mock + manager registration to a known state before each test.
struct SlimBridgeFixture {
    SlimBridgeFixture() {
        SlimBridgeMockDriver& driver = mockDriverInstance();
        driver.enqueueCount = 0;
        driver.showCount = 0;
        driver.pollCount = 0;
        driver.mState = DriverState::READY;
        driver.lastBytes.clear();
        driver.lastFormat = ChannelPixelFormat::Unknown;
        driver.lastResetUs = 0;
        driver.lastDataPtr = nullptr;
        driver.lastCapacity = 0;
    }

    // The mock stays registered for the whole file: TestDriverTraits only
    // registers once, so removing it here would silently disable every
    // later test.
};

using TestController =
    SlimBridgeController<4, TIMING_WS2812_800KHZ, GRB, 280, TestDriverTraits>;

// Exposes the protected CPixelLEDController::show overload for direct driving.
class ExposedTestController : public TestController {
  public:
    using TestController::show;
};

}  // namespace slim_bridge_test

using namespace slim_bridge_test;

FL_TEST_CASE("SlimBridgeController::registerWithManager is idempotent across instances") {
    SlimBridgeFixture fixture;
    int before = registerCallCount();

    TestController a;
    TestController b;
    (void)a;
    (void)b;

    FL_CHECK_EQ(registerCallCount(), before + 1);
}

FL_TEST_CASE("FastLED.show() flushes the frame and calls driver show() once") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    int showBefore = mockDriverInstance().showCount;
    FastLED.show();
    FL_CHECK_EQ(mockDriverInstance().showCount, showBefore + 1);
}

FL_TEST_CASE("Disabled/exclusive-to-another-driver: no enqueue, returns promptly") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    ChannelManager::instance().setDriverEnabled("SLIM_BRIDGE_MOCK", false);
    auto restore = fl::make_scope_exit([&]() {
        ChannelManager::instance().setDriverEnabled("SLIM_BRIDGE_MOCK", true);
    });

    int enqueueBefore = mockDriverInstance().enqueueCount;
    FastLED.show();
    FL_CHECK_EQ(mockDriverInstance().enqueueCount, enqueueBefore);
}

FL_TEST_CASE("RGB golden bytes for known pixels with RGB_ORDER GRB") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[2] = {CRGB(255, 0, 0), CRGB(0, 255, 0)};
    FastLED.addLeds(&controller, leds, 2);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();

    // GRB order: Red(255,0,0) -> [G=0][R=255][B=0], Green(0,255,0) -> [G=255][R=0][B=0]
    FL_CHECK_EQ(mockDriverInstance().lastBytes,
                fl::vector<u8>({0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00}));
    FL_CHECK_EQ(static_cast<int>(mockDriverInstance().lastFormat),
                static_cast<int>(ChannelPixelFormat::RGB));
}

FL_TEST_CASE("RGBW enabled: 4 bytes/pixel and RGBW pixel format") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[2] = {CRGB(255, 0, 0), CRGB(0, 255, 0)};
    controller.setRgbw();
    FastLED.addLeds(&controller, leds, 2);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();

    FL_CHECK_EQ(mockDriverInstance().lastBytes.size(), (fl::size)8);
    FL_CHECK_EQ(static_cast<int>(mockDriverInstance().lastFormat),
                static_cast<int>(ChannelPixelFormat::RGBW));
}

FL_TEST_CASE("RGBWW: 5 bytes/pixel and RGBWW pixel format") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[2] = {CRGB(255, 0, 0), CRGB(0, 255, 0)};
    controller.setRgbww();
    FastLED.addLeds(&controller, leds, 2);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();

    FL_CHECK_EQ(mockDriverInstance().lastBytes.size(), (fl::size)10);
    FL_CHECK_EQ(static_cast<int>(mockDriverInstance().lastFormat),
                static_cast<int>(ChannelPixelFormat::RGBWW));
}

FL_TEST_CASE("WAIT_TIME larger than timing reset folds into reset_us") {
    SlimBridgeFixture fixture;
    // TIMING_WS2812_800KHZ's reset is small (microsecond range); a WAIT_TIME
    // in microseconds far larger than that reset should win.
    using LongWaitController =
        SlimBridgeController<5, TIMING_WS2812_800KHZ, GRB, 1000000, TestDriverTraits>;
    LongWaitController controller;
    CRGB leds[1] = {CRGB(1, 2, 3)};
    FastLED.addLeds(&controller, leds, 1);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();

    FL_CHECK_GE(mockDriverInstance().lastResetUs, (u32)1000000);
}

FL_TEST_CASE("WAIT_TIME smaller than timing reset does not shrink reset_us") {
    SlimBridgeFixture fixture;
    using ShortWaitController =
        SlimBridgeController<6, TIMING_WS2812_800KHZ, GRB, 1, TestDriverTraits>;
    ShortWaitController controller;
    CRGB leds[1] = {CRGB(1, 2, 3)};
    FastLED.addLeds(&controller, leds, 1);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();

    const u32 nativeReset = makeTimingConfig<TIMING_WS2812_800KHZ>().reset_us;
    FL_CHECK_GE(mockDriverInstance().lastResetUs, nativeReset);
}

FL_TEST_CASE("BUSY driver with failing waitForReady suppresses enqueue, READY recovers") {
    SlimBridgeFixture fixture;
    ExposedTestController controller;
    CRGB leds[1] = {CRGB(9, 9, 9)};

    // Previous frame still in flight and the driver never becomes READY:
    // waitForReady() times out, so the frame is dropped. Drive the
    // controller directly so ChannelManager's frame hooks do not add
    // their own waits.
    controller.channelData()->setInUse(true);
    mockDriverInstance().mState = DriverState::BUSY;
    int enqueueBefore = mockDriverInstance().enqueueCount;
    controller.show(leds, 1, 255);
    FL_CHECK_EQ(mockDriverInstance().enqueueCount, enqueueBefore);

    mockDriverInstance().mState = DriverState::READY;
    controller.show(leds, 1, 255);
    FL_CHECK_EQ(mockDriverInstance().enqueueCount, enqueueBefore + 1);
}

FL_TEST_CASE("100 frames keep the same data() pointer and capacity") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();
    const u8* firstPtr = mockDriverInstance().lastDataPtr;
    const fl::size firstCapacity = mockDriverInstance().lastCapacity;
    FL_REQUIRE(firstPtr != nullptr);

    for (int frame = 1; frame < 100; ++frame) {
        FastLED.show();
        FL_CHECK_EQ(mockDriverInstance().lastDataPtr, firstPtr);
        FL_CHECK_EQ(mockDriverInstance().lastCapacity, firstCapacity);
    }
}

}  // FL_TEST_FILE
