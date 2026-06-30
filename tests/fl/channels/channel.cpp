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
#include "fl/test/fltest.h"
#include "platforms/stub/bus_traits.h"

using namespace fl;

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

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), RGB, options);
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

    ChannelConfig config(spiConfig, fl::span<CRGB>(workspace, NUM_LEDS), RGB, options);
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
    FL_CHECK_EQ(bitbangDriver.get(), &BusTraits<Bus::BIT_BANG>::instance());

    channel->removeFromDrawList();
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
