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
#include "fl/stl/type_traits.h"
#include "fl/stl/vector.h"
#include "fl/stl/memory_resource.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
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
    int enqueuedAtFirstShow = 0;
    int busyPolls = 0;
    int rejectedFrameCount = 0;
    int workerConfigCount = 0;
    Core requestedCore = Core::Auto;
    Priority requestedPriority = Priority::Auto;
    bool acceptWorker = true;
    DriverState::Value mState = DriverState::READY;

    fl::vector<u8> lastBytes;
    ChannelPixelFormat lastFormat = ChannelPixelFormat::Unknown;
    u32 lastResetUs = 0;
    const u8* lastDataPtr = nullptr;
    fl::size lastCapacity = 0;

    bool canHandle(const ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }

    bool configureWorkerImpl(const WorkerConfig& config) FL_NO_EXCEPT override {
        ++workerConfigCount;
        requestedCore = config.core;
        requestedPriority = config.priority;
        return acceptWorker;
    }

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

    void show() FL_NO_EXCEPT override {
        if (showCount == 0) {
            enqueuedAtFirstShow = enqueueCount;
        }
        ++showCount;
    }

    DriverState poll() FL_NO_EXCEPT override {
        ++pollCount;
        if (busyPolls > 0) {
            --busyPolls;
            return DriverState(DriverState::BUSY);
        }
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

template<fl::u8 PIN, EOrder ORDER>
using WorkerTestChipset = SlimBridgeController<PIN, TIMING_WS2812_800KHZ,
    ORDER, 280, TestDriverTraits>;

struct BusOnlyCustomRebind {
    template<Bus B>
    using WithBus = WorkerTestChipset<27, RGB>::WithBus<B>;
};

struct NativeBusController {};

template<typename Controller, Bus B>
auto hasClocklessBusController(int) -> decltype(
    static_cast<typename detail::ClocklessBusController<Controller, B>::type*>(nullptr),
    fl::true_type());

template<typename Controller, Bus B>
fl::false_type hasClocklessBusController(...);

template<typename API>
auto acceptsClocklessTemplateCore(int) -> decltype(
    fl::declval<API&>().template addLeds<WS2812B, 27, RGB,
        Bus::FLEX_IO, Core::Core1>(static_cast<CRGB*>(nullptr), 1),
    fl::true_type());

template<typename API>
fl::false_type acceptsClocklessTemplateCore(...);

template<typename API>
auto acceptsSpiTemplateCore(int) -> decltype(
    fl::declval<API&>().template addLedsSpiChannel<APA102, 5, 6, RGB,
        1000000, Bus::SPI, 0, Core::Core1>(static_cast<CRGB*>(nullptr), 1),
    fl::true_type());

template<typename API>
fl::false_type acceptsSpiTemplateCore(...);

template<typename API>
auto acceptsClocklessTemplatePriority(int) -> decltype(
    fl::declval<API&>().template addLeds<WS2812B, 27, RGB,
        Bus::FLEX_IO, Priority::High>(static_cast<CRGB*>(nullptr), 1),
    fl::true_type());

template<typename API>
fl::false_type acceptsClocklessTemplatePriority(...);

template<typename API>
auto acceptsSpiTemplatePriority(int) -> decltype(
    fl::declval<API&>().template addLedsSpiChannel<APA102, 5, 6, RGB,
        1000000, Bus::SPI, 0, Priority::High>(static_cast<CRGB*>(nullptr), 1),
    fl::true_type());

template<typename API>
fl::false_type acceptsSpiTemplatePriority(...);

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

// Models native wrappers such as RP PIO and RMT that inherit WithBus while
// providing initialization and refresh-rate hooks of their own.
class NativeHookController : public ClocklessController<26, TIMING_WS2812_800KHZ, RGB> {
  public:
    static bool& initialized() { static bool value = false; return value; }
    void init() FL_NO_EXCEPT override { initialized() = true; }
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }
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

FL_TEST_CASE("Legacy addLeds templates do not accept worker scheduling settings #5001") {
    FL_CHECK_FALSE(decltype(acceptsClocklessTemplateCore<CFastLED>(0))::value);
    FL_CHECK_FALSE(decltype(acceptsSpiTemplateCore<CFastLED>(0))::value);
    FL_CHECK_FALSE(decltype(acceptsClocklessTemplatePriority<CFastLED>(0))::value);
    FL_CHECK_FALSE(decltype(acceptsSpiTemplatePriority<CFastLED>(0))::value);
}

FL_TEST_CASE("addLeds bus rebinding preserves default storage and worker policy #5001") {
    using Original = WS2812<27, RGB>;
    using Automatic = detail::ClocklessBusController<Original, Bus::AUTO>::type;
    // Preserve platform hooks when Auto selects the original transport.
    FL_CHECK_TRUE((fl::is_same<Original, Automatic>::value));
    using Default = WorkerTestChipset<27, RGB>;
    using Rebound = Default::WithBus<Bus::AUTO>;
    FL_CHECK_EQ(sizeof(Rebound), sizeof(Default));
    constexpr Bus bus = DefaultBus<ClocklessChipset>::value == Bus::BIT_BANG
        ? Bus::RMT : Bus::BIT_BANG;
    using ExplicitBus = detail::ClocklessBusController<Default, bus>::type;
    using CustomBus = detail::ClocklessBusController<BusOnlyCustomRebind, bus>::type;
    using Expected = SlimBridgeController<27, TIMING_WS2812_800KHZ,
        RGB, 280, BusTraits<bus>>;
    FL_CHECK_TRUE((fl::is_same<ExplicitBus, Expected>::value));
    FL_CHECK_TRUE((fl::is_same<CustomBus, Expected>::value));
    auto& driver = mockDriverInstance();
    driver.acceptWorker = false;
    driver.requestedCore = Core::Core1;
    driver.requestedPriority = Priority::Medium;
    driver.workerConfigCount = 0;
    driver.enqueueCount = 0;
    CRGB leds[1] = {CRGB::Red};
    auto& controller = FastLED.addLeds<WorkerTestChipset, 27, RGB,
        Bus::AUTO>(leds, 1);
    auto cleanup = fl::make_scope_exit([&]() {
        controller.setLeds(nullptr, 0);
        controller.removeFromDrawList();
        driver.acceptWorker = true;
    });
    controller.showLeds(255);
    FL_CHECK_EQ(driver.workerConfigCount, 0);
    FL_CHECK_EQ(driver.requestedCore, Core::Core1);
    FL_CHECK_EQ(driver.requestedPriority, Priority::Medium);
    FL_CHECK_EQ(driver.enqueueCount, 1);
}

FL_TEST_CASE("Explicit clockless buses rebind or reject unsupported controllers #5001") {
    constexpr Bus platformBus = DefaultBus<ClocklessChipset>::value;
    constexpr Bus otherBus = platformBus == Bus::BIT_BANG ? Bus::RMT : Bus::BIT_BANG;
    using BoundElsewhere = WorkerTestChipset<27, RGB>::WithBus<otherBus>;
    using Selected = detail::ClocklessBusController<BoundElsewhere, platformBus>::type;
    using Expected = BoundElsewhere::WithBus<platformBus>;
    FL_CHECK_TRUE((fl::is_same<Selected, Expected>::value));
    FL_CHECK_FALSE((decltype(hasClocklessBusController<NativeBusController, otherBus>(0))::value));
    FL_CHECK_TRUE((decltype(hasClocklessBusController<NativeBusController, Bus::AUTO>(0))::value));
    FL_CHECK_TRUE((decltype(hasClocklessBusController<NativeBusController, platformBus>(0))::value));
}

FL_TEST_CASE("Explicit native bus preserves stub capture and native wrapper hooks #5001") {
    using Native = ClocklessController<26, TIMING_WS2812_800KHZ, RGB>;
    using Selected = detail::ClocklessBusController<Native, Bus::BIT_BANG>::type;
    FL_CHECK_TRUE((fl::is_same<Selected, Native>::value));
    using SelectedHooks = detail::ClocklessBusController<NativeHookController, Bus::BIT_BANG>::type;
    FL_CHECK_TRUE((fl::is_same<SelectedHooks, NativeHookController>::value));
    SelectedHooks hooked;
    NativeHookController::initialized() = false;
    hooked.init();
    FL_CHECK_EQ(hooked.getMaxRefreshRate(), 400);
    FL_CHECK_TRUE(NativeHookController::initialized());
    auto& strips = ActiveStripData::Instance();
    strips.onBeginFrame();
    auto cleanup = fl::make_scope_exit([&]() { strips.onBeginFrame(); });
    Selected controller;
    CRGB leds[] = {CRGB::Red};
    controller.setLeds(leds, 1);
    controller.showLeds(255);
    FL_CHECK_EQ(strips.getData().size(), 1u);
    if (strips.getData().empty()) { return; }
    const auto& bytes = strips.getData().begin()->second;
    FL_REQUIRE_EQ(bytes.size(), 3u);
    FL_CHECK_EQ(bytes[0], 255);
    FL_CHECK_EQ(bytes[1], 0);
    FL_CHECK_EQ(bytes[2], 0);
}

FL_TEST_CASE("Default worker facade avoids backend dispatch #5001") {
    SlimBridgeMockDriver driver;
    driver.acceptWorker = false;
    IChannelDriver& backend = driver;
    FL_CHECK(backend.configureWorker(WorkerConfig{}));
    FL_CHECK_EQ(driver.workerConfigCount, 0);
    WorkerConfig explicitConfig;
    explicitConfig.core = Core::Core1;
    FL_CHECK_FALSE(backend.configureWorker(explicitConfig));
    FL_CHECK_EQ(driver.workerConfigCount, 1);
    const auto diagnostics = backend.workerDiagnostics();
    FL_CHECK_EQ(diagnostics.core, -2);
    FL_CHECK_EQ(diagnostics.priority, -1);
    FL_CHECK_EQ(diagnostics.totalChunks, 0u);
    FL_CHECK_FALSE(diagnostics.streamError);
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

FL_TEST_CASE("Disabled driver: addLeds falls back to the enabled driver") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    // The one other enabled driver that can take a clockless strip.
    struct FallbackDriver : SlimBridgeMockDriver {
        fl::string getName() const FL_NO_EXCEPT override {
            return fl::string::from_literal("FALLBACK_MOCK");
        }
    };
    auto fallback = fl::make_shared<FallbackDriver>();
    ChannelManager& mgr = ChannelManager::registry();
    mgr.addDriver(1000, fallback);
    mgr.setDriverEnabled("SLIM_BRIDGE_MOCK", false);
    auto restore = fl::make_scope_exit([&]() {
        mgr.setDriverEnabled("SLIM_BRIDGE_MOCK", true);
        mgr.removeDriver(fallback);
    });

    int enqueueBefore = mockDriverInstance().enqueueCount;
    FastLED.show();
    // Never enqueued to the disabled driver; the frame went to the fallback.
    FL_CHECK_EQ(mockDriverInstance().enqueueCount, enqueueBefore);
    FL_CHECK_EQ(fallback->enqueueCount, 1);
    FL_CHECK_EQ(fallback->lastBytes.size(), 12u);
    // ...and was flushed by the end-of-show manager pass.
    FL_CHECK_GE(fallback->showCount, 1);
}

FL_TEST_CASE("Disabled driver with no enabled alternative drops the frame") {
    SlimBridgeFixture fixture;
    TestController controller;
    CRGB leds[4] = {};
    FastLED.addLeds(&controller, leds, 4);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    ChannelManager& mgr = ChannelManager::registry();
    fl::vector<fl::string> disabled;
    for (const fl::DriverInfo& info : mgr.getDriverInfos()) {
        if (info.enabled) disabled.push_back(info.name);
    }
    for (const fl::string& name : disabled) mgr.setDriverEnabled(name.c_str(), false);
    auto restore = fl::make_scope_exit([&]() {
        for (const fl::string& name : disabled) mgr.setDriverEnabled(name.c_str(), true);
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

    // Disable every driver, so there is no fallback either: the frame is
    // dropped before onBeforeEncode() runs.
    ChannelManager& mgr = ChannelManager::registry();
    fl::vector<fl::string> disabled;
    for (const fl::DriverInfo& info : mgr.getDriverInfos()) {
        if (info.enabled) disabled.push_back(info.name);
    }
    for (const fl::string& name : disabled) mgr.setDriverEnabled(name.c_str(), false);
    auto restore = fl::make_scope_exit([&]() {
        for (const fl::string& name : disabled) mgr.setDriverEnabled(name.c_str(), true);
    });

    FastLED.show();
    FL_CHECK_EQ(controller.beforeEncodeCount, 1);
}


// --- Direct-show mode (small-memory tier, FastLED#4671) -------------------
// On !FL_PLATFORM_HAS_LARGE_MEMORY the bridge bypasses ChannelManager: it
// never registers the driver and calls driver.show() itself, so the
// registry (~3 KB) is not linked. Exercised here explicitly via DIRECT_SHOW.

inline SlimBridgeMockDriver& directDriverInstance() {
    static SlimBridgeMockDriver driver;
    return driver;
}

inline int& directRegisterCount() {
    static int count = 0;
    return count;
}

struct DirectDriverTraits {
    static IChannelDriver& instance() FL_NO_EXCEPT { return directDriverInstance(); }
    static void registerWithManager() FL_NO_EXCEPT { ++directRegisterCount(); }
};

struct CountingWorkerDriverTraits : DirectDriverTraits {
    static int& accesses() {
        static int count = 0;
        return count;
    }
    static IChannelDriver& instance() FL_NO_EXCEPT {
        ++accesses();
        return directDriverInstance();
    }
};

FL_TEST_CASE("Legacy bus rebinding does not access the driver singleton at construction #5001") {
    using Automatic = SlimBridgeController<7, TIMING_WS2812_800KHZ, RGB, 280,
        CountingWorkerDriverTraits, 0, true>;
    using Rebound = Automatic::WithBus<Bus::AUTO>;
    FL_CHECK_EQ(sizeof(Automatic), sizeof(Rebound));
    CountingWorkerDriverTraits::accesses() = 0;
    directDriverInstance().acceptWorker = true;
    {
        Automatic automatic;
        FL_CHECK_EQ(CountingWorkerDriverTraits::accesses(), 0);
    }
    {
        Rebound rebound;
        FL_CHECK_EQ(CountingWorkerDriverTraits::accesses(), 0);
    }
}

struct RejectingDirectDriverTraits : DirectDriverTraits {
    static void rejectLegacyFrame() FL_NO_EXCEPT {
        ++directDriverInstance().rejectedFrameCount;
    }
};

class FrameBudgetResource : public fl::memory_resource {
public:
    fl::size limit;
    fl::size largestRequest = 0;
    fl::size allocatedBytes = 0;
    explicit FrameBudgetResource(fl::size budget) : limit(budget) {}

protected:
    void *do_allocate(fl::size bytes) FL_NO_EXCEPT override {
        if (bytes > largestRequest) { largestRequest = bytes; }
        if (bytes > limit) { return nullptr; }
        void *allocation = fl::default_memory_resource()->allocate(bytes);
        if (allocation) { allocatedBytes += bytes; }
        return allocation;
    }
    void do_deallocate(void *pointer, fl::size bytes) FL_NO_EXCEPT override {
        allocatedBytes -= bytes;
        fl::default_memory_resource()->deallocate(pointer, bytes);
    }
};

using BudgetController = SlimBridgeController<7, TIMING_WS2812_800KHZ, RGB, 280,
    RejectingDirectDriverTraits, 0, true>;

FL_TEST_CASE("Slim bridge rejects an incomplete encoded frame on allocation failure") {
    auto &driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.rejectedFrameCount = 0;
    FrameBudgetResource resource(6);
    BudgetController controller;
    fl::vector<fl::u8> encoded(&resource);
    encoded.push_back(0xab);
    controller.channelData()->getData().swap(encoded);
    CRGB pixels[3] = {CRGB::White, CRGB::White, CRGB::White};
    controller.setLeds(fl::span<CRGB>(pixels, 3));

    controller.showLeds(255);

    FL_CHECK_EQ(driver.enqueueCount, 0);
    FL_CHECK_EQ(driver.rejectedFrameCount, 1);
    // Failed reservation must preserve the previous complete snapshot.
    FL_REQUIRE_EQ(controller.channelData()->getSize(), 1);
    FL_CHECK_EQ(controller.channelData()->getData()[0], 0xab);
    FL_CHECK_FALSE(controller.channelData()->isInUse());
}

FL_TEST_CASE("Slim bridge encodes 2000 RGB pixels within a 6000 byte frame budget") {
    auto &driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.rejectedFrameCount = 0;
    FrameBudgetResource resource(6000);
    BudgetController controller;
    fl::vector<fl::u8> encoded(&resource);
    controller.channelData()->getData().swap(encoded);
    fl::vector<CRGB> pixels(2000, CRGB::White);
    controller.setLeds(fl::span<CRGB>(pixels));

    controller.showLeds(255);

    FL_CHECK_EQ(driver.enqueueCount, 1);
    FL_CHECK_EQ(driver.rejectedFrameCount, 0);
    FL_REQUIRE_EQ(driver.lastBytes.size(), 6000);
    for (fl::size i = 0; i < driver.lastBytes.size(); ++i) {
        FL_CHECK_EQ(driver.lastBytes[i], 255);
    }
    // This is the board's actual frame budget: geometric growth previously
    // requested 8092 bytes and silently transmitted only 5395 bytes.
    FL_CHECK_LE(resource.largestRequest, 6000u);
    FL_CHECK_EQ(resource.allocatedBytes, 6000u);
}

using DirectController = SlimBridgeController<7, TIMING_WS2812_800KHZ, GRB, 280,
                                              DirectDriverTraits, 0, /*DIRECT_SHOW=*/true>;

FL_TEST_CASE("Direct-show bridge never registers with ChannelManager") {
    int before = directRegisterCount();
    DirectController controller;
    (void)controller;
    FL_CHECK_EQ(directRegisterCount(), before);
}

FL_TEST_CASE("Direct-show bridge enqueues and shows the frame itself") {
    SlimBridgeMockDriver& driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.showCount = 0;
    driver.mState = DriverState::READY;
    DirectController controller;
    CRGB leds[2] = {CRGB(255, 0, 0), CRGB(0, 255, 0)};  // full scale: dither-proof
    FastLED.addLeds(&controller, leds, 2);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    // The driver is not registered, so only the controller can call show().
    FastLED.show();
    FL_CHECK_EQ(driver.enqueueCount, 1);
    FL_CHECK_EQ(driver.showCount, 1);
    // GRB order, same bytes as the manager-backed golden test.
    FL_CHECK_EQ(driver.lastBytes, fl::vector<u8>({0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00}));
}

FL_TEST_CASE("Immediate direct bridge preserves one strip per show") {
    auto& driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.showCount = 0;
    driver.pollCount = 0;
    driver.enqueuedAtFirstShow = 0;
    driver.mState = DriverState::READY;
    CRGB first[1] = {CRGB::Red};
    CRGB second[1] = {CRGB::Blue};
    DirectController firstController;
    SlimBridgeController<8, TIMING_WS2812_800KHZ, GRB, 280,
                         DirectDriverTraits, 0, true> secondController;
    FastLED.addLeds(&firstController, first, 1);
    FastLED.addLeds(&secondController, second, 1);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();
    FL_CHECK_EQ(driver.enqueuedAtFirstShow, 1);
    FL_CHECK_EQ(driver.showCount, 2);
    FL_CHECK_EQ(driver.pollCount, 0);
}

using BatchedDirectController = SlimBridgeController<7, TIMING_WS2812_800KHZ, GRB, 280,
                                                     DirectDriverTraits, 0, true, true>;

FL_TEST_CASE("Direct-show bridge batches legacy strips before flushing") {
    auto& driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.showCount = 0;
    driver.pollCount = 0;
    driver.enqueuedAtFirstShow = 0;
    driver.mState = DriverState::READY;
    CRGB first[1] = {CRGB::Red};
    CRGB second[1] = {CRGB::Blue};
    BatchedDirectController firstController;
    SlimBridgeController<8, TIMING_WS2812_800KHZ, GRB, 280,
                         DirectDriverTraits, 0, true, true> secondController;
    FastLED.addLeds(&firstController, first, 1);
    FastLED.addLeds(&secondController, second, 1);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    FastLED.show();
    FL_CHECK_EQ(driver.enqueuedAtFirstShow, 2);
    FL_CHECK_GT(driver.pollCount, 0);
}

FL_TEST_CASE("Direct-show bridge polls completion for standalone showLeds") {
    auto& driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.showCount = 0;
    driver.pollCount = 0;
    driver.mState = DriverState::READY;
    CRGB leds[1] = {CRGB::Green};
    BatchedDirectController controller;
    controller.setLeds(leds, 1);

    driver.busyPolls = 3;
    controller.setDither(BINARY_DITHER);
    controller.showLeds(255);
    FL_CHECK_EQ(driver.enqueueCount, 1);
    FL_CHECK_EQ(driver.showCount, 1);
    FL_CHECK_GE(driver.pollCount, 4);
    FL_CHECK_EQ(controller.getDither(), BINARY_DITHER);
}

FL_TEST_CASE("Direct-show bridge respects legacy controller enable state") {
    auto& driver = directDriverInstance();
    driver.enqueueCount = 0;
    driver.showCount = 0;
    driver.pollCount = 0;
    driver.mState = DriverState::READY;
    CRGB leds[1] = {CRGB::Red};
    BatchedDirectController controller;
    FastLED.addLeds(&controller, leds, 1);
    auto cleanup = fl::make_scope_exit([&]() { FastLED.clear(true); });

    controller.setEnabled(false);
    FastLED.show();
    FL_CHECK_EQ(driver.enqueueCount, 0);
    FL_CHECK_EQ(driver.showCount, 0);
    controller.setEnabled(true);
    FastLED.show();
    FL_CHECK_EQ(driver.enqueueCount, 1);
    FL_CHECK_EQ(driver.showCount, 1);
}

}  // FL_TEST_FILE
