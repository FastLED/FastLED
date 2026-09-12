/// @file tests/fl/channels/channels.cpp
/// @brief Test suite for Channel API with addressing, color order, and
///        Bus-templated create<B>()/FastLED.add<B>() overloads (#2428/#2167)

#include "FastLED.h"
#include "fl/channels/all_drivers.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/channel.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/options.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/gfx/fill.h"
#include "fl/math/screenmap.h"
#include "fl/math/xymap.h"
#include "fl/math/xmap.h"
#include "fl/stl/scope_exit.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"
#include "platforms/stub/bus_traits.h"

using namespace fl;

FL_TEST_FILE(FL_FILEPATH) {

// ============ Channel + Addressing Integration Tests ============
// End-to-end byte capture tests for Channel API with XYMap addressing

/// @brief Mock driver to capture encoded channel data for byte verification
class ByteCapturingMockEngine : public IChannelDriver {
public:
    explicit ByteCapturingMockEngine(const char* name = "BYTE_CAPTURE")
        : mName(name) {}

    int transmitCount = 0;
    fl::vector<ChannelDataPtr> mCapturedChannels;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            mCapturedChannels.push_back(channelData);
        }
    }

    void show() override {
        // Trigger transmission in real drivers, but for mocking
        // we just keep the data as-is for inspection
    }

    DriverState poll() override {
        return DriverState::READY;
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }

private:
    fl::string mName;
};

FL_TEST_CASE("Serpentine 2x2 with APA102 encodes pixels in expected byte order") {
    const int WIDTH = 2, HEIGHT = 2;
    const int NUM_LEDS = WIDTH * HEIGHT;

    // Create a 2x2 grid with known distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red at (0,0)
        CRGB(0, 255, 0),      // Index 1: Green at (1,0)
        CRGB(0, 0, 255),      // Index 2: Blue at (0,1)
        CRGB(255, 255, 0),    // Index 3: Yellow at (1,1)
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("BYTE_CAPTURE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2000, mockEngine);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 6, encoder};
    ChannelOptions options;

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), BGR, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply serpentine addressing
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(WIDTH, HEIGHT);
    channel->setScreenMap(serpentine);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // APA102 format: 4-byte start frame (0x00 0x00 0x00 0x00),
        // then for each LED: [brightness/0xFF][B][G][R], then end frame
        // For 4 LEDs with serpentine order [0,1,3,2]: Red, Green, Yellow, Blue

        // Verify we have enough bytes (4 start + 4*4 LED data + at least 1 end)
        FL_CHECK(encodedBytes.size() >= 4 + (NUM_LEDS * 4));

        // Verify start frame (4 zero bytes for APA102)
        if (encodedBytes.size() >= 4) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);
            FL_CHECK_EQ(encodedBytes[1], 0x00);
            FL_CHECK_EQ(encodedBytes[2], 0x00);
            FL_CHECK_EQ(encodedBytes[3], 0x00);
        }

        // Verify LED data in serpentine order: [pixel0=Red, pixel1=Green, pixel3=Yellow, pixel2=Blue]
        // Each LED is 4 bytes: [0xFF or brightness][B][G][R]
        // Start of LED data is at byte 4
        size_t ledStart = 4;

        // LED0 (Red at 0,0): R=255, G=0, B=0 → [0xFF][0x00][0x00][0xFF]
        if (encodedBytes.size() > ledStart + 3) {
            FL_CHECK_EQ(encodedBytes[ledStart + 1], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 2], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 3], 0xFF);  // Red
        }

        // LED1 (Green at 1,0): R=0, G=255, B=0 → [0xFF][0x00][0xFF][0x00]
        if (encodedBytes.size() > ledStart + 7) {
            FL_CHECK_EQ(encodedBytes[ledStart + 5], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 6], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 7], 0x00);  // Red
        }

        // LED2 (Yellow at 1,1): R=255, G=255, B=0 → [0xFF][0x00][0xFF][0xFF]
        if (encodedBytes.size() > ledStart + 11) {
            FL_CHECK_EQ(encodedBytes[ledStart + 9], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 10], 0xFF); // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 11], 0xFF); // Red
        }

        // LED3 (Blue at 0,1): R=0, G=0, B=255 → [0xFF][0xFF][0x00][0x00]
        if (encodedBytes.size() > ledStart + 15) {
            FL_CHECK_EQ(encodedBytes[ledStart + 13], 0xFF); // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 14], 0x00); // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 15], 0x00); // Red
        }
    }
}

FL_TEST_CASE("Serpentine 2x2 with WS2812 GRB encodes pixels in expected byte order") {
    const int WIDTH = 2, HEIGHT = 2;
    const int NUM_LEDS = WIDTH * HEIGHT;

    // Create a 2x2 grid with known distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red at (0,0)
        CRGB(0, 255, 0),      // Index 1: Green at (1,0)
        CRGB(0, 0, 255),      // Index 2: Blue at (0,1)
        CRGB(255, 255, 0),    // Index 3: Yellow at (1,1)
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("WS2812_BYTE_CAPTURE");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2001, mockEngine);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;

    ChannelConfig config(1, timing, fl::span<CRGB>(workspace, NUM_LEDS), GRB, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply serpentine addressing
    fl::XYMap serpentine = fl::XYMap::constructSerpentine(WIDTH, HEIGHT);
    channel->setScreenMap(serpentine);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // WS2812 format: No preamble, just raw GRB bytes (3 bytes per LED)
        // Serpentine order [0,1,3,2]: Red, Green, Yellow, Blue
        // Red (R=255, G=0, B=0): GRB order → [G][R][B] = [0x00][0xFF][0x00]
        // Green (R=0, G=255, B=0): GRB order → [G][R][B] = [0xFF][0x00][0x00]
        // Yellow (R=255, G=255, B=0): GRB order → [G][R][B] = [0xFF][0xFF][0x00]
        // Blue (R=0, G=0, B=255): GRB order → [G][R][B] = [0x00][0x00][0xFF]

        // Verify we have enough bytes (3 bytes per LED * 4 LEDs = 12 bytes minimum)
        // WS2812 may add reset bytes at the end, so we just check minimum
        FL_CHECK(encodedBytes.size() >= NUM_LEDS * 3);

        // Verify LED data in serpentine order with GRB color order
        // LED0 (Red at 0,0) in GRB: [0x00][0xFF][0x00]
        if (encodedBytes.size() > 2) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);  // Green component
            FL_CHECK_EQ(encodedBytes[1], 0xFF);  // Red component
            FL_CHECK_EQ(encodedBytes[2], 0x00);  // Blue component
        }

        // LED1 (Green at 1,0) in GRB: [0xFF][0x00][0x00]
        if (encodedBytes.size() > 5) {
            FL_CHECK_EQ(encodedBytes[3], 0xFF);  // Green component
            FL_CHECK_EQ(encodedBytes[4], 0x00);  // Red component
            FL_CHECK_EQ(encodedBytes[5], 0x00);  // Blue component
        }

        // LED2 (Yellow at 1,1) in GRB: [0xFF][0xFF][0x00]
        if (encodedBytes.size() > 8) {
            FL_CHECK_EQ(encodedBytes[6], 0xFF);  // Green component
            FL_CHECK_EQ(encodedBytes[7], 0xFF);  // Red component
            FL_CHECK_EQ(encodedBytes[8], 0x00);  // Blue component
        }

        // LED3 (Blue at 0,1) in GRB: [0x00][0x00][0xFF]
        if (encodedBytes.size() > 11) {
            FL_CHECK_EQ(encodedBytes[9], 0x00);   // Green component
            FL_CHECK_EQ(encodedBytes[10], 0x00);  // Red component
            FL_CHECK_EQ(encodedBytes[11], 0xFF);  // Blue component
        }
    }
}

FL_TEST_CASE("[#1009] Channels sharing one CRGB array keep independent color orders") {
    CRGB leds[] = {
        CRGB(0x11, 0x22, 0x33),
        CRGB(0x44, 0x55, 0x66),
        CRGB(0x77, 0x88, 0x99),
        CRGB(0xaa, 0xbb, 0xcc),
    };

    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("MIXED_ORDER_CAPTURE");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2002, mockEngine);
    auto driverCleanup = fl::make_scope_exit([&]() {
        manager.removeDriver(mockEngine);
    });

    const auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions options;
    auto rgbChannel = Channel::create(
        ChannelConfig(13, timing, fl::span<CRGB>(leds, 2), RGB, options));
    auto grbChannel = Channel::create(
        ChannelConfig(14, timing, fl::span<CRGB>(leds + 2, 2), GRB, options));
    FL_REQUIRE_TRUE(rgbChannel != nullptr);
    FL_REQUIRE_TRUE(grbChannel != nullptr);

    auto channelCleanup = fl::make_scope_exit([&]() {
        FastLED.remove(rgbChannel);
        FastLED.remove(grbChannel);
    });

    FastLED.add(rgbChannel);
    FastLED.add(grbChannel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    FL_REQUIRE_EQ(mockEngine->mCapturedChannels.size(), 2);
    const ChannelDataPtr& rgbData = mockEngine->mCapturedChannels[0];
    const ChannelDataPtr& grbData = mockEngine->mCapturedChannels[1];
    FL_REQUIRE_EQ(rgbData->getPin(), 13);
    FL_REQUIRE_EQ(grbData->getPin(), 14);

    FL_CHECK_EQ(rgbData->getData(),
                fl::vector<u8>({0x11, 0x22, 0x33, 0x44, 0x55, 0x66}));
    FL_CHECK_EQ(grbData->getData(),
                fl::vector<u8>({0x88, 0x77, 0x99, 0xbb, 0xaa, 0xcc}));
}

FL_TEST_CASE("XMap reverse addressing with APA102 encodes pixels in reverse order") {
    const int NUM_LEDS = 4;

    // Create a 4-LED strip with distinct colors
    CRGB workspace[NUM_LEDS] = {
        CRGB(255, 0, 0),      // Index 0: Red
        CRGB(0, 255, 0),      // Index 1: Green
        CRGB(0, 0, 255),      // Index 2: Blue
        CRGB(255, 255, 0),    // Index 3: Yellow
    };

    // Create mock engine to capture encoded bytes
    auto mockEngine = fl::make_shared<ByteCapturingMockEngine>("XMAP_CAPTURE_TEST");
    ChannelManager& manager = ChannelManager::instance();
    manager.addDriver(2001, mockEngine);

    SpiEncoder encoder = SpiEncoder::apa102();
    SpiChipsetConfig spiConfig{5, 6, encoder};
    ChannelOptions options;

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), BGR, options);
    auto channel = Channel::create(config);
    FL_CHECK(channel != nullptr);

    // Set up automatic cleanup at scope exit
    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        manager.removeDriver(mockEngine);
    });

    // Apply reverse addressing using XMap
    fl::XMap reverse(NUM_LEDS, true);  // true = reverse order
    channel->setScreenMap(reverse);

    // Add channel to FastLED and trigger show (encodes pixels)
    FastLED.add(channel);
    mockEngine->mCapturedChannels.clear();
    FastLED.show();

    // Verify we captured the encoded data
    FL_CHECK(mockEngine->mCapturedChannels.size() > 0);

    if (!mockEngine->mCapturedChannels.empty()) {
        const auto& channelData = mockEngine->mCapturedChannels[0];
        const auto& encodedBytes = channelData->getData();

        // APA102 format: 4-byte start frame, then [0xFF][B][G][R] per LED
        // With reverse XMap, physical order should be [3,2,1,0]: Yellow, Blue, Green, Red
        FL_CHECK(encodedBytes.size() >= 4 + (NUM_LEDS * 4));

        // Verify start frame
        if (encodedBytes.size() >= 4) {
            FL_CHECK_EQ(encodedBytes[0], 0x00);
            FL_CHECK_EQ(encodedBytes[1], 0x00);
            FL_CHECK_EQ(encodedBytes[2], 0x00);
            FL_CHECK_EQ(encodedBytes[3], 0x00);
        }

        size_t ledStart = 4;

        // Physical LED 0 (maps to source 3 = Yellow): [0xFF][0x00][0xFF][0xFF]
        if (encodedBytes.size() > ledStart + 3) {
            FL_CHECK_EQ(encodedBytes[ledStart + 1], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 2], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 3], 0xFF);  // Red
        }

        // Physical LED 1 (maps to source 2 = Blue): [0xFF][0xFF][0x00][0x00]
        if (encodedBytes.size() > ledStart + 7) {
            FL_CHECK_EQ(encodedBytes[ledStart + 5], 0xFF);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 6], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 7], 0x00);  // Red
        }

        // Physical LED 2 (maps to source 1 = Green): [0xFF][0x00][0xFF][0x00]
        if (encodedBytes.size() > ledStart + 11) {
            FL_CHECK_EQ(encodedBytes[ledStart + 9], 0x00);   // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 10], 0xFF);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 11], 0x00);  // Red
        }

        // Physical LED 3 (maps to source 0 = Red): [0xFF][0x00][0x00][0xFF]
        if (encodedBytes.size() > ledStart + 15) {
            FL_CHECK_EQ(encodedBytes[ledStart + 13], 0x00);  // Blue
            FL_CHECK_EQ(encodedBytes[ledStart + 14], 0x00);  // Green
            FL_CHECK_EQ(encodedBytes[ledStart + 15], 0xFF);  // Red
        }
    }
}

// ============ Bus-template API tests (#2428 / #2167) ============
// Verify Channel::create<Bus::BIT_BANG>(cfg) and FastLED.add<Bus::BIT_BANG>(cfg)
// on the host build (FL_IS_STUB).  Tests run against the real BIT_BANG driver —
// no mocks needed.

namespace {

/// Reset ChannelManager to a known-empty state and return its reference.
/// Required before each bus-template test so prior registrations don't leak.
ChannelManager& freshBusTestManager() {
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    return mgr;
}

/// Minimal WS2812 ChannelConfig on pin 4 with 8 LEDs.
ChannelConfig makeBusTestConfig(fl::span<CRGB> leds) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    return ChannelConfig(4, timing, leds, RGB);
}

}  // namespace

// ============ Typed mBus runtime dispatch (#2459) ============
// `ChannelOptions::mBus` is the typed primary path for runtime driver
// selection. The non-template `FastLED.add(cfg)` reads it and dispatches
// to `busName(mBus)`. `Bus::AUTO` (the default) falls through to the
// string-affinity escape hatch / priority dispatch.

FL_TEST_CASE("cfg.options.mBus = Bus::BIT_BANG binds the BIT_BANG driver on host") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();
    FL_REQUIRE(mgr.getDriverCount() > 0);

    CRGB leds[8] = {};
    ChannelConfig cfg = makeBusTestConfig(fl::span<CRGB>(leds, 8));
    cfg.options.mBus = Bus::BIT_BANG;

    auto channel = Channel::create(cfg);
    FL_REQUIRE(channel != nullptr);
    channel->addToDrawList();
    channel->showLeds(0);

    FL_CHECK_EQ(channel->getEngineName(), fl::string::from_literal("BIT_BANG"));

    auto bitbangDriver = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(bitbangDriver != nullptr);

    channel->removeFromDrawList();
}

// `registerWithManager()` must register the BusTraits singleton itself, not a
// freshly constructed driver of the same type -- a copy would collect state
// nothing ever transmits.
//
// Asserted against a registration *this* translation unit performs. The
// obvious form, comparing the driver `fl::enableAllDrivers()` registered
// against `&BusTraits<Bus::BIT_BANG>::instance()`, is not portable:
// `instancePtr()` keeps its singleton in a function-local static inside a
// header, and `enableAllDrivers()` lives in the FastLED library image while
// the expression above is evaluated in the test image. Where those are
// separate modules the loader does not merge -- `fastled_lib` is a
// `shared_library` and every test is another, so that is every Windows DLL
// build -- each module holds its own copy and the pointers differ by
// construction. That compares the harness's linkage, not the registration.
FL_TEST_CASE("BusTraits::registerWithManager registers the singleton itself") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    BusTraits<Bus::BIT_BANG>::registerWithManager();

    auto cleanup = fl::make_scope_exit([&]() { mgr.clearAllDrivers(); });

    auto registered = mgr.getDriverByName(fl::string::from_literal("BIT_BANG"));
    FL_REQUIRE(registered != nullptr);
    FL_CHECK_EQ(registered.get(), &BusTraits<Bus::BIT_BANG>::instance());
}

FL_TEST_CASE("mBus = Bus::AUTO falls back to priority dispatch") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    fl::enableAllDrivers();

    CRGB leds[8] = {};
    ChannelConfig cfg = makeBusTestConfig(fl::span<CRGB>(leds, 8));
    // Leave mBus at default (AUTO).

    auto channel = Channel::create(cfg);
    FL_REQUIRE(channel != nullptr);
    channel->addToDrawList();
    channel->showLeds(0);

    // Priority dispatch picks a host driver — either STUB or BIT_BANG.
    auto bound = channel->getEngineName();
    FL_CHECK(bound == fl::string::from_literal("STUB") ||
             bound == fl::string::from_literal("BIT_BANG"));

    channel->removeFromDrawList();
}

// ============ Affinity-miss diagnostic (#2455) ============
// When an mBus target is set but the named driver isn't registered with
// ChannelManager, Channel::showPixels emits exactly one FL_ERROR with the
// fl::enableDrivers<fl::Bus::X>() / fl::enableAllDrivers() hint, then falls
// back to priority dispatch. The mAffinityWarned flag suppresses duplicates
// on subsequent shows of the same channel.

FL_TEST_CASE("mBus miss to unregistered known Bus falls back and still renders") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    // Register ONLY the host fallbacks (STUB + BIT_BANG via enableAllDrivers).
    // We do not register Bus::RMT, so an mBus = Bus::RMT request must miss.
    fl::enableAllDrivers();
    // Use the silent `findDriverByName` here — `getDriverByName` would emit
    // its own FL_ERROR on miss and pollute any log inspection of the actual
    // showLeds() diagnostic below.
    FL_CHECK(mgr.findDriverByName(fl::string::from_literal("RMT")) == nullptr);

    CRGB leds[8] = {};
    ChannelConfig cfg = makeBusTestConfig(fl::span<CRGB>(leds, 8));
    // Target a typed Bus whose driver is NOT registered on this host.
    cfg.options.mBus = fl::Bus::RMT;

    auto channel = Channel::create(cfg);
    FL_REQUIRE(channel != nullptr);
    channel->addToDrawList();

    // First show: the diagnostic fires AND priority dispatch picks a host
    // driver (STUB or BIT_BANG). Channel still renders.
    //
    // NOTE: we don't assert "exactly one FL_ERROR was emitted" — the project
    // log macros don't have a capture/intercept hook today, so a count
    // assertion isn't possible. The behavioural proxy is: dispatch falls
    // back to a host driver and the bound driver is stable across shows
    // (no churn / no respec). That, plus the mAffinityWarned guard's
    // unconditional latch, is what makes the FL_ERROR one-shot in practice.
    channel->showLeds(0);
    auto bound = channel->getEngineName();
    FL_CHECK(bound == fl::string::from_literal("STUB") ||
             bound == fl::string::from_literal("BIT_BANG"));

    // Second show: mAffinityWarned suppresses the duplicate diagnostic.
    // Driver binding is unchanged.
    channel->showLeds(0);
    FL_CHECK(channel->getEngineName() == bound);

    channel->removeFromDrawList();
}

// ============ #2517 Silent-drop diagnostic ============
// When a channel's bound driver is disabled (typically by
// `FastLED.setExclusiveDriver<OtherBus>()`) the previous behaviour was a
// silent drop in `ChannelManager::onEndFrame()` — the driver still received
// `enqueue()` calls, but the manager never called `show()` on a disabled
// driver, so the frame was dropped on the floor with no diagnostic.
//
// After the fix, `Channel::showPixels()` short-circuits the enqueue and
// emits one actionable FL_ERROR per (channel × disable-event), naming the
// channel, the bound driver, the active exclusive-driver setting (if any),
// and the `FastLED.enableDrivers<>()` / `FastLED.enableAllDrivers()`
// remediation. Verification is behavioural — the project log macros don't
// have a capture hook today, so each test asserts the enqueue did NOT land.

namespace {

/// Minimal fake driver — records every enqueue call so the test can assert
/// whether a frame was actually queued or silently dropped at the manager
/// layer.
class CountingFakeDriver : public IChannelDriver {
public:
    explicit CountingFakeDriver(const char* name) : mName(name) {}

    int enqueueCount = 0;
    int showCount = 0;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }

    void enqueue(ChannelDataPtr channelData) override {
        (void)channelData;
        ++enqueueCount;
    }

    void show() override {
        ++showCount;
    }

    DriverState poll() override {
        return DriverState::READY;
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }

private:
    fl::string mName;
};

/// Minimal WS2812 ChannelConfig on pin 7 with 4 LEDs.
ChannelConfig makeSilentDropTestConfig(fl::span<CRGB> leds) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    return ChannelConfig(7, timing, leds, RGB);
}

}  // namespace

FL_TEST_CASE("[#2517] Disabled driver: enqueue is suppressed (would-be silent drop)") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    // Register a fake "PARLIO_TEST" driver — this is the only enabled driver,
    // so the priority-dispatch path picks it on the first showLeds() call.
    auto fakeDriver = fl::make_shared<CountingFakeDriver>("PARLIO_TEST");
    mgr.addDriver(9000, fakeDriver);
    FL_REQUIRE(mgr.driverStatus(fl::string::from_literal("PARLIO_TEST"))
               == ChannelManager::DriverStatus::STATUS_ENABLED);

    CRGB leds[4] = {};
    auto channel = Channel::create(makeSilentDropTestConfig(fl::span<CRGB>(leds, 4)));
    FL_REQUIRE(channel != nullptr);

    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        mgr.clearAllDrivers();
    });

    channel->addToDrawList();

    // Frame 1: happy path — driver is enabled, enqueue should land.
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 1);

    // Now disable PARLIO_TEST exclusively in favor of a name that isn't
    // registered. All drivers (including PARLIO_TEST) become DISABLED, but
    // the channel still holds a strong reference to the previously-bound
    // driver via its cached `mDriver` weak_ptr being valid.
    mgr.setExclusiveDriverByName("DOES_NOT_EXIST");
    FL_CHECK(mgr.driverStatus(fl::string::from_literal("PARLIO_TEST"))
             == ChannelManager::DriverStatus::STATUS_DISABLED);
    FL_CHECK_EQ(mgr.exclusiveDriverName(),
                fl::string::from_literal("DOES_NOT_EXIST"));

    // Frame 2: this used to be the silent drop. After the #2517 fix:
    //   (a) Channel::showPixels emits FL_ERROR (visible in test output), and
    //   (b) skips the driver->enqueue() call — so enqueueCount stays at 1.
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 1);

    // Frame 3: one-shot guard suppresses duplicate per-frame FL_ERROR, but
    // enqueue is still suppressed — the data still wouldn't be transmitted.
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 1);
}

FL_TEST_CASE("[#2517] Normal flush path does NOT trigger the silent-drop FL_ERROR") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    auto fakeDriver = fl::make_shared<CountingFakeDriver>("HAPPY_DRIVER");
    mgr.addDriver(9000, fakeDriver);

    CRGB leds[4] = {};
    auto channel = Channel::create(makeSilentDropTestConfig(fl::span<CRGB>(leds, 4)));
    FL_REQUIRE(channel != nullptr);

    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        mgr.clearAllDrivers();
    });

    channel->addToDrawList();

    // Multiple consecutive happy-path frames — driver stays ENABLED, every
    // enqueue lands. Behavioural proxy for "no extra FL_ERROR fires": each
    // frame is faithfully delivered to the driver.
    channel->showLeds(0);
    channel->showLeds(0);
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 3);
}

FL_TEST_CASE("[#2517] Re-enabling the driver resumes enqueue and re-arms the latch") {
    auto& mgr = freshBusTestManager();
    FL_REQUIRE(mgr.getDriverCount() == 0);

    auto fakeDriver = fl::make_shared<CountingFakeDriver>("RECOVERY_DRIVER");
    mgr.addDriver(9000, fakeDriver);

    CRGB leds[4] = {};
    auto channel = Channel::create(makeSilentDropTestConfig(fl::span<CRGB>(leds, 4)));
    FL_REQUIRE(channel != nullptr);

    auto cleanup = fl::make_scope_exit([&]() {
        channel->removeFromDrawList();
        mgr.clearAllDrivers();
    });

    channel->addToDrawList();

    // Happy frame: enqueue lands.
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 1);

    // Disable: subsequent frame is suppressed.
    mgr.setDriverEnabled("RECOVERY_DRIVER", false);
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 1);

    // Re-enable: enqueue resumes.
    mgr.setDriverEnabled("RECOVERY_DRIVER", true);
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 2);

    // Disable a second time — the one-shot latch should have been cleared by
    // the intervening ENABLED frame, so the diagnostic re-arms (we can't
    // observe the FL_ERROR directly, but the enqueue is still suppressed).
    mgr.setDriverEnabled("RECOVERY_DRIVER", false);
    channel->showLeds(0);
    FL_CHECK_EQ(fakeDriver->enqueueCount, 2);
}


// ============ UCS7604 through the Channels encode path (#4326) ============
// `tests/fl/chipsets/ucs7604.cpp` drives `testUCS7604Controller`, the legacy
// `CLEDController` path. `fl::Channel`'s own `writeUCS7604` sits in an
// anonymous namespace in `channel.cpp.hpp` and is reachable only through
// `showPixels()`, so nothing observed what it puts on the wire.

namespace {

/// Records the encoded bytes of the last frame enqueued.
class CapturingDriver : public IChannelDriver {
public:
    fl::vector<u8> last;
    int frames = 0;

    bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;
    }
    void enqueue(ChannelDataPtr channelData) override {
        ++frames;
        last.clear();
        if (channelData) {
            const auto& d = channelData->getData();
            for (fl::size i = 0; i < d.size(); ++i) {
                last.push_back(d[i]);
            }
        }
    }
    void show() override {}
    DriverState poll() override { return DriverState::READY; }
    fl::string getName() const override {
        return fl::string::from_literal("UCS_CAPTURE");
    }
    Capabilities getCapabilities() const override { return Capabilities(true, true); }
};

constexpr EmitterProfile kProfile = EmitterProfile::rgb(
    "fixture/ucs-4326",
    Chromaticity(0.640f, 0.330f), Chromaticity(0.300f, 0.600f),
    Chromaticity(0.150f, 0.060f), 1.0f, 1.0f, 1.0f);

ClocklessChipset ucs16(int pin) {
    auto timing = ChipsetTimingConfig(800, 450, 450, 50, "UCS7604");
    return ClocklessChipset(pin, timing,
                            ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT);
}

/// Encode one frame and hand back the bytes the driver saw.
fl::vector<u8> encodeOnce(CRGB colour, bool bindProfile, float gamma, int pin) {
    // Reset first: registrations leak across calls otherwise, and with several
    // same-priority drivers alive the frame can land on one this call does not
    // hold, which silently makes every comparison below meaningless.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    auto driver = fl::make_shared<CapturingDriver>();
    mgr.addDriver(9100, driver);
    // And take it back out on the way home. Leaving UCS_CAPTURE registered
    // lets a later test select it, or trip a zero-driver precondition, in a
    // way that depends on case order -- which is how the gamma assertion in
    // this block first went wrong.
    auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

    CRGB leds[1] = {colour};
    ChannelOptions options;
    // Dither off explicitly. With it on, a temporal step can carry 127 to 128
    // and the gamma assertion below moves with the frame -- which is exactly
    // what happened when these cases were first written in a file of their
    // own and then moved next to neighbours that leave dithering enabled.
    options.mDitherMode = DISABLE_DITHER;
    options.mGamma = gamma;
    if (bindProfile) {
        // Note this also resets mGamma, mCorrection, mTemperature and
        // mDitherMode -- see the gamma case below.
        options.setColorProfile(kProfile, SourceProfile::linearSrgb());
    }
    ChannelConfig config(ucs16(pin), fl::span<CRGB>(leds, 1), RGB, options);
    ChannelPtr channel = Channel::create(config);
    if (channel) {
        channel->showLeds(255);
    }
    return driver->last;
}

}  // namespace

FL_TEST_CASE("[#4326] the Channels UCS7604 path encodes a frame at all") {
    // The floor this file exists to lay down. `tests/fl/chipsets/ucs7604.cpp`
    // drives the legacy CLEDController; `fl::Channel`'s writeUCS7604 lives in
    // an anonymous namespace and is reachable only through showPixels(), so
    // nothing observed it. If this stops producing bytes the cases below stop
    // meaning anything.
    fl::vector<u8> out = encodeOnce(CRGB(127, 0, 0), false, 2.8f, 11);
    FL_REQUIRE_GT((int)out.size(), 0);

    // 15-byte preamble, then RGB16 big-endian. Gamma 2.8 of 127 is the value
    // the legacy path produces too, so this pins the whole chain rather than
    // just "some bytes came out".
    FL_REQUIRE_GT((int)out.size(), 16);
    const int r16 = (out[15] << 8) | out[16];
    FL_CHECK_EQ(r16, (int)fl::gamma_2_8(127));
}

FL_TEST_CASE("[#4326] gamma reaches the encoder when no profile is bound") {
    // Legacy behaviour, and correct: an unmanaged channel is meant to be
    // shaped by mGamma. This is also the positive control for the case below
    // -- without it, "gamma does not vary the output once a profile binds"
    // could be satisfied by an encoder that ignores gamma entirely.
    fl::vector<u8> a = encodeOnce(CRGB(127, 0, 0), false, 2.8f, 12);
    fl::vector<u8> b = encodeOnce(CRGB(127, 0, 0), false, 1.6f, 13);

    FL_REQUIRE_GT((int)a.size(), 16);
    FL_REQUIRE_EQ((int)a.size(), (int)b.size());
    FL_CHECK_NE((int)((a[15] << 8) | a[16]), (int)((b[15] << 8) | b[16]));
}

FL_TEST_CASE("[#4326] binding a profile clears the caller's gamma") {
    // ChannelOptions::setColorProfile() resets mGamma alongside mCorrection,
    // mTemperature and mDitherMode (options.h). Pinned here because it is the
    // half of the managed-mode exclusion policy that #4156 R9 asked for and
    // it had no test: two channels that differ only in the gamma they asked
    // for produce identical bytes once a profile is bound.
    fl::vector<u8> a = encodeOnce(CRGB(127, 0, 0), true, 2.8f, 14);
    fl::vector<u8> b = encodeOnce(CRGB(127, 0, 0), true, 1.6f, 15);

    FL_REQUIRE_GT((int)a.size(), 0);
    FL_REQUIRE_EQ((int)a.size(), (int)b.size());
    for (fl::size i = 0; i < a.size(); ++i) {
        FL_CHECK_EQ((int)a[i], (int)b[i]);
    }
}

FL_TEST_CASE("[#4326] a bound profile reaches the encode path") {
    // The name has to be earned: this encodes, rather than only checking that
    // the channel owns a profile. What it pins is that the binding changes
    // what goes on the wire -- the same pixel through the same encoder, bound
    // and unbound, does not produce the same bytes.
    //
    // Whether the 2.8 that `mGamma.value_or(2.8f)` falls back to should give
    // way to identity once the device solve runs is #4326's remaining half.
    fl::vector<u8> bound = encodeOnce(CRGB(127, 0, 0), true, 2.8f, 16);
    fl::vector<u8> unbound = encodeOnce(CRGB(127, 0, 0), false, 2.8f, 17);

    FL_REQUIRE_GT((int)bound.size(), 16);
    FL_REQUIRE_EQ((int)bound.size(), (int)unbound.size());

    bool differs = false;
    for (fl::size k = 0; k < bound.size(); ++k) {
        if (bound[k] != unbound[k]) { differs = true; }
    }
    FL_CHECK(differs);

    // And specifically in the payload rather than only in the preamble, so a
    // difference in current-control bytes could not satisfy it.
    FL_CHECK_NE((int)((bound[15] << 8) | bound[16]),
                (int)((unbound[15] << 8) | unbound[16]));

    // Deliberately no isColorManaged() assertion. That accessor was a
    // hardcoded constant when this file was written; #4328/#4329 give it real
    // semantics, and pinning its old value here would only have made
    // whichever of the two landed second fail.
}


FL_TEST_CASE("[#4326] the managed path puts the device drive on the wire") {
    // This case used to assert the defect. It read 849 and said so, and said
    // it was written to fail when #4326 was fixed. This is that failure,
    // turned into the statement of the fix.
    //
    // `kProfile` gives all three emitters luminance 1.0 at sRGB
    // chromaticities, and the source is linear sRGB. Full red is therefore a
    // target of Y = 0.2126 -- sRGB red's luminance share -- against an emitter
    // making Y = 1.0 at full drive, so the device solve's answer is a red
    // drive of 0.2126. That is where the pipeline's job ends, and a 16-bit
    // encoder should put 0.2126 * 65535 = 13933 on the wire.
    //
    // It used to put 849: the drive narrowed to 8 bits (0.2126 * 255 = 54)
    // and then widened by the gamma-2.8 LUT, (54/255)^2.8 * 65535 = 849.
    // Both halves of #4326 in one number -- the 8-bit round trip, and the
    // second shaping stage B1/§6 forbid after the device solve.
    fl::vector<u8> bound = encodeOnce(CRGB(255, 0, 0), true, 2.8f, 20);
    FL_REQUIRE_EQ((int)bound.size(), 21);

    const int red16 = (bound[15] << 8) | bound[16];

    // Within rounding of the drive itself. Two counts of 65535, which is the
    // s16.16 drive and `quantize16` each rounding once -- a single
    // quantization, which is what B3 asks for.
    FL_CHECK_GT(red16, 13900);
    FL_CHECK_LT(red16, 13970);

    // And specifically not the old value, so a regression that restored the
    // gamma stage could not pass the bounds above by coincidence.
    FL_CHECK_NE(red16, 849);

    // Black still encodes to black.
    fl::vector<u8> black = encodeOnce(CRGB(0, 0, 0), true, 2.8f, 20);
    FL_REQUIRE_EQ((int)black.size(), 21);
    FL_CHECK_EQ((int)((black[15] << 8) | black[16]), 0);

    // The unbound path is untouched: it has no device drive to carry, its
    // pixels are 8-bit, and the gamma there is the legacy behaviour rather
    // than a second stage. The resolution case below measures what that
    // costs; this one is only about the managed path.
    fl::vector<u8> unbound = encodeOnce(CRGB(255, 0, 0), false, 2.8f, 20);
    FL_REQUIRE_EQ((int)unbound.size(), 21);
    FL_CHECK_EQ((int)((unbound[15] << 8) | unbound[16]), 65535);
}

FL_TEST_CASE("[#4326] the 16-bit encoder reaches 252 of 65536 levels") {
    // P8 also owns "encoders consume wide output directly -- no RGB8
    // correction round-trip". #4326 states the consequence without a number:
    // the native 16-bit chipsets "reach at most 256 distinct levels per
    // channel out of 65536".
    //
    // Measured on the unbound path, where the 8-bit-plus-gamma chain is the
    // legacy behaviour and correct, so this is the resolution the encoder
    // itself has rather than a managed-mode defect: 252 distinct values from
    // the 256 source codes -- four pairs collide in the gamma LUT -- and the
    // widest step between neighbours is 717 counts.
    fl::vector<int> seen;
    int previous = -1;
    int widest_step = 0;
    int top = -1;
    for (int code = 0; code < 256; ++code) {
        fl::vector<u8> out = encodeOnce(CRGB(code, 0, 0), false, 2.8f, 20);
        FL_REQUIRE_EQ((int)out.size(), 21);
        const int value = (out[15] << 8) | out[16];
        bool found = false;
        for (fl::size j = 0; j < seen.size(); ++j) {
            if (seen[j] == value) { found = true; }
        }
        if (!found) { seen.push_back(value); }
        if (previous >= 0 && value - previous > widest_step) {
            widest_step = value - previous;
        }
        previous = value;
        top = value;
    }

    FL_CHECK_EQ((int)seen.size(), 252);

    // The endpoints are reached, so the range is used -- the loss is
    // resolution inside it, not a squeezed scale.
    FL_CHECK_EQ(top, 65535);

    // And the coarseness, which is what "256 of 65536" costs in practice:
    // near full scale, neighbouring source codes are 717 counts apart, so
    // 716 of every 717 representable values are unreachable there.
    FL_CHECK_EQ(widest_step, 717);
}

// ============ Legacy output with colour management disabled (#4034) ============
// The tracker lists "Legacy output byte-identical with CM disabled" as an
// acceptance criterion and nothing verified it. The colour pipeline reaches
// into `showPixels` and into how the pixel iterator is built, so a change
// there can alter output for every sketch that never asked for colour
// management -- silently, because no managed test would notice.
//
// The observable form of "byte-identical" in one build is that an unbound
// channel emits exactly the legacy arithmetic: colour order applied, each
// channel scaled by brightness, and nothing else. Literals rather than a
// recomputation, so this compares against a recorded answer instead of
// against the same code that produced it.

FL_TEST_CASE("[#4034] an unbound channel emits the legacy bytes exactly") {
    struct Case {
        const char* name;
        EOrder order;
        u8 brightness;
        int expect[3];
    };
    // Source is CRGB(200, 100, 50) throughout.
    const Case cases[] = {
        {"full brightness is the identity",   RGB, 255, {200, 100, 50}},
        {"half",                              RGB, 128, {100,  50, 25}},
        {"quarter",                           RGB,  64, { 50,  25, 12}},
        // scale8 with FASTLED_SCALE8_FIXED is (v * (b + 1)) >> 8, so the
        // lowest brightness keeps a non-zero channel alive rather than
        // rounding the whole pixel to black.
        {"lowest brightness keeps red lit",   RGB,   1, {  1,   0,  0}},
        // Colour order moves bytes and must not touch values.
        {"GRB reorders without rescaling",    GRB, 255, {100, 200, 50}},
        {"BGR reorders and scales",           BGR, 128, { 25,  50, 100}},
    };

    for (fl::size i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        FL_SUBCASE(cases[i].name) {
            auto& mgr = ChannelManager::instance();
            mgr.clearAllDrivers();
            auto driver = fl::make_shared<CapturingDriver>();
            mgr.addDriver(9300, driver);
            auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

            CRGB leds[1] = {CRGB(200, 100, 50)};
            ChannelOptions options;
            options.mDitherMode = DISABLE_DITHER;
            auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
            ChannelConfig config(120 + (int)i, timing, fl::span<CRGB>(leds, 1),
                                 cases[i].order, options);
            ChannelPtr ch = Channel::create(config);
            FL_REQUIRE(ch != nullptr);
            FL_CHECK_FALSE(ch->hasColorProfile());
            ch->showLeds(cases[i].brightness);

            FL_REQUIRE_EQ((int)driver->last.size(), 3);
            FL_CHECK_EQ((int)driver->last[0], cases[i].expect[0]);
            FL_CHECK_EQ((int)driver->last[1], cases[i].expect[1]);
            FL_CHECK_EQ((int)driver->last[2], cases[i].expect[2]);
        }
    }
}

FL_TEST_CASE("[#4034] and binding a profile does change them") {
    // The guard. Without it, the case above would pass just as well against a
    // pipeline that had stopped doing anything at all -- which is the other
    // way this criterion can be satisfied for the wrong reason.
    auto& mgr = ChannelManager::instance();
    mgr.clearAllDrivers();
    auto driver = fl::make_shared<CapturingDriver>();
    mgr.addDriver(9300, driver);
    auto cleanup = fl::make_scope_exit([&mgr]() { mgr.clearAllDrivers(); });

    CRGB leds[1] = {CRGB(200, 100, 50)};
    ChannelOptions options;
    options.mDitherMode = DISABLE_DITHER;
    FL_REQUIRE(options.setColorProfile(kProfile, SourceProfile::linearSrgb()));
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelConfig config(126, timing, fl::span<CRGB>(leds, 1), RGB, options);
    ChannelPtr ch = Channel::create(config);
    FL_REQUIRE(ch != nullptr);
    FL_REQUIRE(ch->isColorManaged());
    ch->showLeds(255);

    FL_REQUIRE_EQ((int)driver->last.size(), 3);
    const bool same = driver->last[0] == 200 && driver->last[1] == 100 &&
                      driver->last[2] == 50;
    FL_CHECK_FALSE(same);
}

}  // FL_TEST_FILE
