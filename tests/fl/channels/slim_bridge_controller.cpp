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
#include "fl/channels/slim_spi_bridge_controller.h"
#include "fl/channels/config.h"
#include "fl/chipsets/spi.h"
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

/// Subclass exercising the onBeforeEncode() hook: counts how many times it
/// was invoked, mirroring how stub/WASM controllers would feed
/// ActiveStripTracker before encoding.
class CountingHookController : public TestController {
  public:
    int beforeEncodeCount = 0;

  protected:
    void onBeforeEncode(PixelController<GRB>& pixels) FL_NO_EXCEPT override {
        ++beforeEncodeCount;
        (void)pixels;
    }
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

FL_TEST_CASE("onBeforeEncode() hook fires once per accepted frame, not on dropped frames") {
    SlimBridgeFixture fixture;
    CountingHookController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();
    FL_CHECK_EQ(controller.beforeEncodeCount, 1);

    // Disable the driver: the frame is dropped before onBeforeEncode() runs.
    ChannelManager::instance().setDriverEnabled("SLIM_BRIDGE_MOCK", false);
    auto restore = fl::make_scope_exit([&]() {
        ChannelManager::instance().setDriverEnabled("SLIM_BRIDGE_MOCK", true);
    });

    FastLED.show();
    FL_CHECK_EQ(controller.beforeEncodeCount, 1);
}

// ===========================================================================
// SlimSpiBridgeController: Hz clock-rate contract (#4593, SAMD21/SAMD51)
// ===========================================================================
//
// SAMD21/SAMD51 route legacy `addLeds<APA102/SK9822, ...>()` through
// SlimSpiBridgeController once fastspi.h puts them on the Hz branch of
// FL_DATA_RATE_MHZ and FastLED.h sets FASTLED_SPI_USES_CHANNEL_API=1. The
// SAMD preprocessor branch cannot be flipped on host, so these cases pin the
// contract that branch relies on: `addLedsSpiChannel<..., DATA_RATE_MHZ(N)>`
// hands the channel a clock in Hz, and the encoded bytes at a SAMD-typical
// rate are identical to the default-rate golden frame.
//
// These cases are last in the file on purpose: they make SLIM_BRIDGE_MOCK
// the exclusive driver so it wins over the host's BIT_BANG affinity
// (the same approach as tests/fl/channels/spi_legacy_golden.cpp).

namespace slim_spi_hz_test {

template <fl::SpiChipset CHIPSET>
using SpiCtrl = SlimSpiBridgeController<CHIPSET, RGB, Bus::AUTO, 0>;

inline void prepareSpiDispatch() {
    TestDriverTraits::registerWithManager();
    ChannelManager::instance().setExclusiveDriverByName("SLIM_BRIDGE_MOCK");
    FastLED.setBrightness(255);
    FastLED.setDither(DISABLE_DITHER);
}

inline u32 clockHzOf(const ChannelDataPtr& data) {
    const SpiChipsetConfig* spi = data->getChipset().ptr<SpiChipsetConfig>();
    return spi ? spi->timing.clock_hz : 0u;
}

inline fl::vector<u8> bytesOf(const ChannelDataPtr& data) {
    fl::vector<u8> out;
    const auto& src = data->getData();
    for (fl::size i = 0; i < src.size(); ++i) {
        out.push_back(src[i]);
    }
    return out;
}

/// Reference frame: a directly-constructed slim controller at the chipset's
/// default rate (the rate the #4613 golden vectors were captured at).
template <fl::SpiChipset CHIPSET>
fl::vector<u8> defaultRateFrame(CRGB* leds, int n) {
    SpiCtrl<CHIPSET> ref(SpiChipsetConfig(
        90, 91, SpiEncoder::spiEncoderForChipset(CHIPSET)));
    FastLED.addLeds(&ref, leds, n);
    FastLED.show();
    fl::vector<u8> bytes = bytesOf(ref.channelData());
    ref.removeFromDrawList();
    return bytes;
}

}  // namespace slim_spi_hz_test

FL_TEST_CASE("DATA_RATE_MHZ/KHZ return Hz on the channel-API SPI branch") {
    // Host takes the same Hz branch (FASTLED_STUB_IMPL) SAMD now joins.
    FL_CHECK_EQ(static_cast<u32>(DATA_RATE_MHZ(12)), 12000000u);
    FL_CHECK_EQ(static_cast<u32>(DATA_RATE_MHZ(24)), 24000000u);
    FL_CHECK_EQ(static_cast<u32>(DATA_RATE_KHZ(500)), 500000u);
}

FL_TEST_CASE("SlimSpiBridge APA102 at DATA_RATE_MHZ(12): Hz clock, golden bytes") {
    using namespace slim_spi_hz_test;
    SlimBridgeFixture fixture;
    prepareSpiDispatch();
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};

    CLEDController& base =
        FastLED.addLedsSpiChannel<APA102, 80, 81, RGB, DATA_RATE_MHZ(12)>(leds, 2);
    auto& ctrl = static_cast<SpiCtrl<SpiChipset::APA102>&>(base);
    FL_CHECK_EQ(clockHzOf(ctrl.channelData()), 12000000u);

    FastLED.show();
    const fl::vector<u8> atRate = bytesOf(ctrl.channelData());
    ctrl.removeFromDrawList();

    const fl::vector<u8> golden = defaultRateFrame<SpiChipset::APA102>(leds, 2);
    FL_CHECK_EQ(atRate, golden);
    // 4-byte zero start frame, [0xE0|bri5][B][G][R] per LED, 0xFF end frame.
    FL_REQUIRE_EQ(atRate.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(atRate[0], 0x00);
    FL_CHECK_EQ(atRate[4] & 0xE0, 0xE0);
    FL_CHECK_EQ(atRate[12], 0xFF);
}

FL_TEST_CASE("SlimSpiBridge SK9822 at DATA_RATE_MHZ(24): Hz clock, golden bytes") {
    using namespace slim_spi_hz_test;
    SlimBridgeFixture fixture;
    prepareSpiDispatch();
    static CRGB leds[2] = {CRGB(0x10, 0x20, 0x30), CRGB(0xFF, 0x00, 0x80)};

    CLEDController& base =
        FastLED.addLedsSpiChannel<SK9822, 82, 83, RGB, DATA_RATE_MHZ(24)>(leds, 2);
    auto& ctrl = static_cast<SpiCtrl<SpiChipset::SK9822>&>(base);
    FL_CHECK_EQ(clockHzOf(ctrl.channelData()), 24000000u);

    FastLED.show();
    const fl::vector<u8> atRate = bytesOf(ctrl.channelData());
    ctrl.removeFromDrawList();

    const fl::vector<u8> golden = defaultRateFrame<SpiChipset::SK9822>(leds, 2);
    FL_CHECK_EQ(atRate, golden);
    FL_REQUIRE_EQ(atRate.size(), (fl::size)(4 + 4 * 2 + 4));
    FL_CHECK_EQ(atRate[0], 0x00);
    FL_CHECK_EQ(atRate[4] & 0xE0, 0xE0);
    FL_CHECK_EQ(atRate[12], 0xFF);
}

}  // FL_TEST_FILE
