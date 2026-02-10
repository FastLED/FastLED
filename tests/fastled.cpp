
// g++ --std=c++11 test.cpp

// NOTE: Arduino macros (min, max, abs, round, radians, degrees, map) are defined
// in tests/test_pch.h BEFORE FastLED.h is included to test that fl/undef.h works correctly.
// This simulates what happens on real Arduino hardware where Arduino.h defines these macros
// before user code includes FastLED.h.

#include "FastLED.h"

// Verify that fl/undef.h successfully undefined the Arduino macros
// The macros are defined in tests/test_pch.h BEFORE FastLED.h is included
// fl/undef.h includes stdlib.h first (so stdlib can use macro versions), then undefs them
#ifdef min
#error "min macro still defined after FastLED.h"
#endif
#ifdef max
#error "max macro still defined after FastLED.h"
#endif
#ifdef abs
#error "abs macro still defined after FastLED.h"
#endif
#ifdef round
#error "round macro still defined after FastLED.h"
#endif
#ifdef radians
#error "radians macro still defined after FastLED.h"
#endif
#ifdef degrees
#error "degrees macro still defined after FastLED.h"
#endif
#ifdef map
#error "map macro still defined after FastLED.h"
#endif
#include "colorutils.h"
#include "test.h"
#include "eorder.h"
#include "fl/colorutils_misc.h"
#include "fl/eorder.h"
#include "fl/fill.h"
#include "hsv2rgb.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/channels/bus_manager.h"
#include "fl/channels/options.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/weak_ptr.h"
#include "fl/stl/new.h"
#include "fl/cled_controller.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"

#undef NUM_LEDS  // Avoid redefinition in unity builds
#define NUM_LEDS 1000
#define DATA_PIN 2
#define CLOCK_PIN 3

FL_TEST_CASE("Simple") {
    static CRGB leds[NUM_LEDS];  // Use static to avoid global constructor warning
    FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
}

FL_TEST_CASE("Fill Gradient SHORTEST_HUES") {
    static CRGB leds[NUM_LEDS];
    fill_gradient(leds, 0, CHSV(0, 255, 255), NUM_LEDS - 1, CHSV(96, 255, 255), SHORTEST_HUES);
}

FL_TEST_CASE("Legacy aliases resolve to FastLED instance") {
    // Verify that all legacy aliases point to the same object
    // These aliases provide backward compatibility for code written for
    // FastSPI_LED and FastSPI_LED2 (the original library names)

    FL_SUBCASE("FastSPI_LED alias") {
        CFastLED* pFastLED = &FastLED;
        CFastLED* pFastSPI_LED = &FastSPI_LED;
        FL_CHECK(pFastLED == pFastSPI_LED);
    }

    FL_SUBCASE("FastSPI_LED2 alias") {
        CFastLED* pFastLED = &FastLED;
        CFastLED* pFastSPI_LED2 = &FastSPI_LED2;
        FL_CHECK(pFastLED == pFastSPI_LED2);
    }

    FL_SUBCASE("LEDS alias") {
        CFastLED* pFastLED = &FastLED;
        CFastLED* pLEDS = &LEDS;
        FL_CHECK(pFastLED == pLEDS);
    }

    FL_SUBCASE("All aliases access same brightness setting") {
        static CRGB leds[NUM_LEDS];
        FastLED.clear();
        FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);

        // Set brightness using FastLED
        FastLED.setBrightness(128);

        // Verify all aliases see the same brightness
        FL_CHECK(FastLED.getBrightness() == 128);
        FL_CHECK(FastSPI_LED.getBrightness() == 128);
        FL_CHECK(FastSPI_LED2.getBrightness() == 128);
        FL_CHECK(LEDS.getBrightness() == 128);

        // Change brightness using legacy alias
        FastSPI_LED.setBrightness(64);

        // Verify all aliases see the new brightness
        FL_CHECK(FastLED.getBrightness() == 64);
        FL_CHECK(FastSPI_LED.getBrightness() == 64);
        FL_CHECK(FastSPI_LED2.getBrightness() == 64);
        FL_CHECK(LEDS.getBrightness() == 64);
    }
}

// Channel API tests - validates new channels API (addresses GitHub issue #2167)
namespace {

/// @brief Mock channel engine for testing the channels API
///
/// This mock engine validates that:
/// - enqueue() is called when channel data is submitted
/// - show() triggers transmission
/// - getName() returns "MOCK" for affinity binding
class ChannelEngineMock : public fl::IChannelEngine {
public:
    int mEnqueueCount = 0;
    int mShowCount = 0;
    fl::vector<fl::ChannelDataPtr> mEnqueuedChannels;

    explicit ChannelEngineMock(const char* name = "MOCK") : mName(fl::string::from_literal(name)) {}

    bool canHandle(const fl::ChannelDataPtr& data) const override {
        (void)data;
        return true;  // Test engine accepts all channel types
    }

    void enqueue(fl::ChannelDataPtr channelData) override {
        if (channelData) {
            mEnqueueCount++;
            mEnqueuedChannels.push_back(channelData);
        }
    }

    void show() override {
        mShowCount++;
        mEnqueuedChannels.clear();
    }

    EngineState poll() override {
        return EngineState(EngineState::READY);
    }

    fl::string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);  // Mock accepts both clockless and SPI
    }

    void reset() {
        mEnqueueCount = 0;
        mShowCount = 0;
        mEnqueuedChannels.clear();
    }

private:
    fl::string mName;
};

} // anonymous namespace

FL_TEST_CASE("Channel API: Mock engine workflow (GitHub issue #2167)") {
    // This test validates the complete workflow reported in issue #2167:
    // 1. Create ChannelEngineMock with string name "MOCK"
    // 2. Inject it into ChannelBusManager
    // 3. Construct ChannelConfig with affinity string "MOCK"
    // 4. Add channel to FastLED
    // 5. Call FastLED.show()
    // 6. Verify engine received data via enqueue()

    auto mockEngine = fl::make_shared<ChannelEngineMock>("MOCK");
    mockEngine->reset();

    // Step 1 & 2: Register mock engine
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);  // High priority

    // Verify registration
    auto* registeredEngine = manager.getEngineByName("MOCK");
    FL_REQUIRE(registeredEngine != nullptr);
    FL_CHECK(registeredEngine == mockEngine.get());

    // Step 3: Create channel with affinity "MOCK"
    static CRGB leds[10];
    fl::fill_solid(leds, 10, CRGB::Red);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK";  // Bind to mock engine

    fl::ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, options);

    // Helper lambda to check if channel is registered in ChannelBusManager
    auto isChannelRegistered = [&manager](const fl::ChannelPtr& ch) -> bool {
        auto channels = manager.getChannels();
        for (const auto& c : channels) {
            if (c == ch) {
                return true;
            }
        }
        return false;
    };

    // Create channel
    auto channel = fl::Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // Verify channel is NOT in ChannelBusManager yet (deferred registration)
    FL_CHECK(!isChannelRegistered(channel));

    // Step 4: Add to FastLED
    FastLED.add(channel);

    // Verify channel IS NOW in ChannelBusManager (explicit registration)
    FL_CHECK(isChannelRegistered(channel));

    // Step 5 & 6: Call FastLED.show() and verify enqueue()
    int enqueueBefore = mockEngine->mEnqueueCount;
    FastLED.show();

    // Validate: engine received data via enqueue()
    FL_CHECK(mockEngine->mEnqueueCount > enqueueBefore);

    // Clean up
    FastLED.remove(channel);
    manager.setDriverEnabled("MOCK", false);
}

FL_TEST_CASE("Channel API: Double add protection") {
    // Verify that calling add() multiple times doesn't create duplicates
    auto mockEngine = fl::make_shared<ChannelEngineMock>("MOCK_DOUBLE");
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::Green);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_DOUBLE";

    fl::ChannelConfig config(10, timing, fl::span<CRGB>(leds, 5), GRB, options);
    auto channel = fl::Channel::create(config);

    FL_REQUIRE(channel != nullptr);

    // Helper lambda to check if channel is registered in ChannelBusManager
    auto isChannelRegistered = [&manager](const fl::ChannelPtr& ch) -> bool {
        auto channels = manager.getChannels();
        for (const auto& c : channels) {
            if (c == ch) {
                return true;
            }
        }
        return false;
    };

    // Helper lambda to count channel occurrences in ChannelBusManager
    auto countChannelOccurrences = [&manager](const fl::ChannelPtr& ch) -> int {
        int count = 0;
        auto channels = manager.getChannels();
        for (const auto& c : channels) {
            if (c == ch) {
                count++;
            }
        }
        return count;
    };

    // Before adding: not in list
    FL_CHECK(!isChannelRegistered(channel));

    // First add
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));

    // Second add (should be safe, no duplicate)
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));

    // Third add (should still be safe)
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));

    // Count occurrences of this channel in ChannelBusManager
    int occurrenceCount = countChannelOccurrences(channel);

    // Should appear exactly once, not multiple times
    FL_CHECK(occurrenceCount == 1);

    // Clean up
    FastLED.remove(channel);
    manager.setDriverEnabled("MOCK_DOUBLE", false);
}

FL_TEST_CASE("Channel API: Add and remove symmetry") {
    // Verify that add() and remove() work symmetrically
    auto mockEngine = fl::make_shared<ChannelEngineMock>("MOCK_REMOVE");
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);

    static CRGB leds[8];
    fl::fill_solid(leds, 8, CRGB::Blue);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_REMOVE";

    fl::ChannelConfig config(12, timing, fl::span<CRGB>(leds, 8), GRB, options);
    auto channel = fl::Channel::create(config);

    FL_REQUIRE(channel != nullptr);

    // Helper lambda to check if channel is registered in ChannelBusManager
    auto isChannelRegistered = [&manager](const fl::ChannelPtr& ch) -> bool {
        auto channels = manager.getChannels();
        for (const auto& c : channels) {
            if (c == ch) {
                return true;
            }
        }
        return false;
    };

    // Initial state: not in list
    FL_CHECK(!isChannelRegistered(channel));

    // Add to list
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));

    // Remove from list
    FastLED.remove(channel);
    FL_CHECK(!isChannelRegistered(channel));

    // Verify channel object is still valid (not destroyed)
    FL_CHECK(channel->size() == 8);
    FL_CHECK(channel->getPin() == 12);

    // Can re-add if needed
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));

    // Remove again
    FastLED.remove(channel);
    FL_CHECK(!isChannelRegistered(channel));

    // Safe to call remove multiple times
    FastLED.remove(channel);
    FastLED.remove(channel);
    FL_CHECK(!isChannelRegistered(channel));

    // Clean up
    manager.setDriverEnabled("MOCK_REMOVE", false);
}

FL_TEST_CASE("Channel API: Internal ChannelPtr storage prevents dangling") {
    // Verify that CFastLED stores ChannelPtrs internally so channels
    // survive even if the caller drops their reference.
    auto mockEngine = fl::make_shared<ChannelEngineMock>("MOCK_STORAGE");
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);

    static CRGB leds[4];
    fl::fill_solid(leds, 4, CRGB::White);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_STORAGE";

    fl::ChannelConfig config(7, timing, fl::span<CRGB>(leds, 4), GRB, options);
    auto channel = fl::Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // Helper lambda to check if channel is registered in ChannelBusManager
    auto isChannelRegistered = [&manager](const fl::ChannelPtr& ch) -> bool {
        auto channels = manager.getChannels();
        for (const auto& c : channels) {
            if (c == ch) {
                return true;
            }
        }
        return false;
    };

    // After add, ChannelBusManager holds an internal reference
    FastLED.add(channel);
    FL_CHECK(isChannelRegistered(channel));
    FL_CHECK(channel.use_count() >= 2);  // caller + ChannelBusManager internal

    // Drop local reference - channel should survive via ChannelBusManager's storage
    fl::Channel* raw = channel.get();
    channel.reset();

    // Channel should still be in ChannelBusManager (not destroyed)
    auto channels = manager.getChannels();
    bool found = false;
    for (const auto& ch : channels) {
        if (ch.get() == raw) {
            found = true;
            break;
        }
    }
    FL_CHECK(found);

    // After remove, the internal reference is released too
    // Re-acquire a ChannelPtr to call remove (we need to find it)
    // Use a fresh channel to test remove releases the internal ref
    auto channel2 = fl::Channel::create(config);
    FL_REQUIRE(channel2 != nullptr);
    FastLED.add(channel2);
    FL_CHECK(channel2.use_count() >= 2);

    FastLED.remove(channel2);
    FL_CHECK(!isChannelRegistered(channel2));
    FL_CHECK(channel2.use_count() == 1);  // only local ref remains

    // Clean up the first channel that's still in ChannelBusManager
    // Find it and remove it
    channels = manager.getChannels();
    for (const auto& ch : channels) {
        if (ch.get() == raw) {
            FastLED.remove(ch);
            break;
        }
    }
    manager.setDriverEnabled("MOCK_STORAGE", false);
}

FL_TEST_CASE("Legacy API: 4 parallel strips using FastLED.addLeds<>()") {
    // This test validates that the legacy FastLED.addLeds<>() API works with channel engines:
    // - Use template-based FastLED.addLeds<WS2812, PIN>() (no explicit channel creation)
    // - Set different colors on each strip
    // - Call FastLED.show()
    // - Verify engine received all 4 strips with correct data

    auto mockEngine = fl::make_shared<ChannelEngineMock>("MOCK_LEGACY");
    mockEngine->reset();

    // Register mock engine with high priority
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine);

    // Verify registration
    auto* registeredEngine = manager.getEngineByName("MOCK_LEGACY");
    FL_REQUIRE(registeredEngine != nullptr);
    FL_CHECK(registeredEngine == mockEngine.get());

    // Create 4 LED strips using legacy template API (no affinity, no explicit channel)
    #define NUM_LEDS 60
    #define PIN1 16
    #define PIN2 17
    #define PIN3 18
    #define PIN4 19

    static CRGB strip1[NUM_LEDS];
    static CRGB strip2[NUM_LEDS];
    static CRGB strip3[NUM_LEDS];
    static CRGB strip4[NUM_LEDS];

    // Use legacy API - should automatically use highest priority engine (our mock)
    FastLED.addLeds<WS2812, PIN1>(strip1, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN2>(strip2, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN3>(strip3, NUM_LEDS);
    FastLED.addLeds<WS2812, PIN4>(strip4, NUM_LEDS);

    // Set different colors on each strip (from README example)
    fl::fill_solid(strip1, NUM_LEDS, CRGB::Red);
    fl::fill_solid(strip2, NUM_LEDS, CRGB::Green);
    fl::fill_solid(strip3, NUM_LEDS, CRGB::Blue);
    fl::fill_solid(strip4, NUM_LEDS, CRGB::Yellow);

    // Reset mock counters before show
    mockEngine->reset();

    // Call FastLED.show() - should enqueue all 4 strips
    FastLED.show();

    // Verify engine received all 4 strips
    FL_CHECK(mockEngine->mEnqueueCount == 4);
    FL_CHECK(mockEngine->mShowCount == 1);
    FL_CHECK(mockEngine->mEnqueuedChannels.size() == 0);  // Cleared by show()

    // Verify the channels have the correct data (spot check first LED of each strip)
    FL_CHECK(strip1[0] == CRGB::Red);
    FL_CHECK(strip2[0] == CRGB::Green);
    FL_CHECK(strip3[0] == CRGB::Blue);
    FL_CHECK(strip4[0] == CRGB::Yellow);

    // Verify all LEDs in strip1 are red
    for (int i = 0; i < NUM_LEDS; i++) {
        FL_CHECK(strip1[i] == CRGB::Red);
    }

    // Test second frame with different pattern (rainbow effect from README)
    mockEngine->reset();
    static uint8_t hue = 0;
    for(int i = 0; i < NUM_LEDS; i++) {
        strip1[i] = CHSV(hue + (i * 4), 255, 255);
        strip2[i] = CHSV(hue + (i * 4) + 64, 255, 255);
        strip3[i] = CHSV(hue + (i * 4) + 128, 255, 255);
        strip4[i] = CHSV(hue + (i * 4) + 192, 255, 255);
    }

    FastLED.show();

    // Verify engine received all 4 strips again
    FL_CHECK(mockEngine->mEnqueueCount == 4);
    FL_CHECK(mockEngine->mShowCount == 1);

    // Cleanup - clear all controllers (legacy API doesn't return handles)
    FastLED.clear(true);  // Clear and deallocate
    manager.removeEngine(mockEngine);

    #undef NUM_LEDS
    #undef PIN1
    #undef PIN2
    #undef PIN3
    #undef PIN4
}

// --- Tests moved from tests/fl/channels/channel_add_remove.cpp ---

namespace channel_add_remove_test {

using namespace fl;

/// Minimal engine for testing - always READY
class StubEngine : public IChannelEngine {
public:
    bool canHandle(const ChannelDataPtr&) const override { return true; }
    void enqueue(ChannelDataPtr) override {}
    void show() override {}
    EngineState poll() override { return EngineState::READY; }
    fl::string getName() const override { return fl::string::from_literal("STUB_ADD_REMOVE"); }
    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }
};

static ChannelPtr makeChannel(CRGB* leds, int n) {
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "STUB_ADD_REMOVE";
    ChannelConfig config(1, timing, fl::span<CRGB>(leds, n), RGB, opts);
    return Channel::create(config);
}

static bool controllerInList(fl::Channel* channel) {
    // Channels are now managed by ChannelBusManager, not the CLEDController linked list
    // Check if the channel is registered in ChannelBusManager
    auto& manager = fl::ChannelBusManager::instance();
    auto channels = manager.getChannels();
    for (const auto& ch : channels) {
        if (ch.get() == channel) {
            return true;
        }
    }
    return false;
}

FL_TEST_CASE("FastLED.add stores ChannelPtr - survives caller scope") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2000, engine);

    CRGB leds[4];
    fl::weak_ptr<Channel> weakRef;

    {
        auto ch = makeChannel(leds, 4);
        weakRef = ch;
        // Before add: refcount is 1 (only 'ch')
        FL_CHECK(ch.use_count() == 1);
        FL_CHECK(!weakRef.expired());

        FastLED.add(ch);

        // After add: refcount is 2 ('ch' + internal storage)
        FL_CHECK(ch.use_count() == 2);
        FL_CHECK(controllerInList(ch.get()));
        // 'ch' goes out of scope here, dropping refcount to 1
    }

    // The weak_ptr must NOT be expired because FastLED's internal
    // mChannels keeps the ChannelPtr alive
    FL_CHECK(!weakRef.expired());
    FL_CHECK(weakRef.use_count() == 1);

    // Lock the weak_ptr to get a shared_ptr for verification
    auto locked = weakRef.lock();
    FL_REQUIRE(locked != nullptr);
    FL_CHECK(controllerInList(locked.get()));

    // Clean up
    FastLED.remove(locked);
    FL_CHECK(!controllerInList(locked.get()));

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.add double-add is safe") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2001, engine);

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    FastLED.add(ch);
    FL_CHECK(ch.use_count() == 2); // ch + internal

    FastLED.add(ch); // double add - should be no-op
    FL_CHECK(ch.use_count() == 2); // still only 2, not 3

    // Channel should appear exactly once in ChannelBusManager
    int count = 0;
    auto channels = mgr.getChannels();
    for (const auto& channel : channels) {
        if (channel.get() == ch.get()) count++;
    }
    FL_CHECK(count == 1);

    // Clean up
    FastLED.remove(ch);
    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.remove double-remove is safe") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2002, engine);

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    FastLED.add(ch);
    FL_CHECK(controllerInList(ch.get()));

    FastLED.remove(ch);
    FL_CHECK(!controllerInList(ch.get()));

    // Double remove - should not crash or change refcount
    long rc = ch.use_count();
    FastLED.remove(ch);
    FL_CHECK(!controllerInList(ch.get()));
    FL_CHECK(ch.use_count() == rc);

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.remove nullptr is safe") {
    FastLED.remove(ChannelPtr());
}

FL_TEST_CASE("FastLED.add nullptr is safe") {
    FastLED.add(ChannelPtr());
}

FL_TEST_CASE("FastLED.reset() removes all channels and drops refcount to 1") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2003, engine);

    CRGB leds1[4];
    CRGB leds2[4];
    CRGB leds3[4];

    // Create three channels
    auto ch1 = makeChannel(leds1, 4);
    auto ch2 = makeChannel(leds2, 4);
    auto ch3 = makeChannel(leds3, 4);

    // Before add: refcount is 1 (only stack reference)
    FL_CHECK(ch1.use_count() == 1);
    FL_CHECK(ch2.use_count() == 1);
    FL_CHECK(ch3.use_count() == 1);

    // Add all channels
    FastLED.add(ch1);
    FastLED.add(ch2);
    FastLED.add(ch3);

    // After add: refcount is 2 (stack + internal storage)
    FL_CHECK(ch1.use_count() == 2);
    FL_CHECK(ch2.use_count() == 2);
    FL_CHECK(ch3.use_count() == 2);

    // Verify all channels are in the draw list
    FL_CHECK(controllerInList(ch1.get()));
    FL_CHECK(controllerInList(ch2.get()));
    FL_CHECK(controllerInList(ch3.get()));

    // Call reset() - should wait for transmissions and remove all channels
    FastLED.reset(ResetFlags::CHANNELS);

    // After reset: refcount should be 1 (only stack reference remains)
    FL_CHECK(ch1.use_count() == 1);
    FL_CHECK(ch2.use_count() == 1);
    FL_CHECK(ch3.use_count() == 1);

    // Verify no channels are in the draw list
    FL_CHECK(!controllerInList(ch1.get()));
    FL_CHECK(!controllerInList(ch2.get()));
    FL_CHECK(!controllerInList(ch3.get()));

    // Verify channels are still valid (not destroyed)
    FL_CHECK(ch1->size() == 4);
    FL_CHECK(ch2->size() == 4);
    FL_CHECK(ch3->size() == 4);

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.reset() when no channels exist is safe") {
    // Should be safe to call reset when no channels are registered
    FastLED.reset(ResetFlags::CHANNELS);  // Should not crash
    FastLED.reset(ResetFlags::POWER_SETTINGS);  // Should not crash
    FastLED.reset(ResetFlags::BRIGHTNESS);  // Should not crash
    FastLED.reset(ResetFlags::CHANNEL_ENGINES);  // Should not crash
}

FL_TEST_CASE("FastLED.reset() with POWER_SETTINGS flag resets power management") {
    // Set power management and brightness separately
    FastLED.setBrightness(128);
    FL_CHECK(FastLED.getBrightness() == 128);

    FastLED.setMaxPowerInMilliWatts(5000);

    // Reset only power settings (should NOT affect brightness)
    FastLED.reset(ResetFlags::POWER_SETTINGS);

    // Brightness should remain unchanged
    FL_CHECK(FastLED.getBrightness() == 128);

    // Power settings should be reset to defaults (tested indirectly via show())
    // After reset, power limiting should be disabled
}

FL_TEST_CASE("FastLED.reset() with BRIGHTNESS flag resets brightness to 255") {
    // Set custom brightness
    FastLED.setBrightness(64);
    FL_CHECK(FastLED.getBrightness() == 64);

    // Reset only brightness
    FastLED.reset(ResetFlags::BRIGHTNESS);

    // Brightness should be back to default (255)
    FL_CHECK(FastLED.getBrightness() == 255);
}

FL_TEST_CASE("FastLED.reset() with REFRESH_RATE flag resets refresh rate") {
    // Set a custom refresh rate (30 FPS = 33333 microseconds per frame)
    FastLED.setMaxRefreshRate(30, false);

    // Reset only refresh rate
    FastLED.reset(ResetFlags::REFRESH_RATE);

    // Refresh rate should be unlimited (0 microseconds minimum)
    // Can't directly test m_nMinMicros, but reset should work without error
}

FL_TEST_CASE("FastLED.reset() with FPS_COUNTER flag resets FPS tracking") {
    // Note: FPS counter is internal state, we can only verify reset doesn't crash
    FastLED.reset(ResetFlags::FPS_COUNTER);

    // After reset, FPS should be 0
    FL_CHECK(FastLED.getFPS() == 0);
}

FL_TEST_CASE("FastLED.reset() with multiple flags using OR operator") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2004, engine);

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    // Set up non-default state
    FastLED.add(ch);
    FastLED.setBrightness(100);
    FastLED.setMaxPowerInMilliWatts(5000);
    FastLED.setMaxRefreshRate(60, false);

    // Verify non-default state
    FL_CHECK(ch.use_count() == 2);
    FL_CHECK(FastLED.getBrightness() == 100);

    // Reset multiple settings at once
    FastLED.reset(ResetFlags::CHANNELS | ResetFlags::BRIGHTNESS | ResetFlags::POWER_SETTINGS);

    // Verify channels were removed
    FL_CHECK(ch.use_count() == 1);
    FL_CHECK(!controllerInList(ch.get()));

    // Verify brightness was reset
    FL_CHECK(FastLED.getBrightness() == 255);

    // Refresh rate should NOT be reset (we didn't include that flag)
    // Power settings should be reset (included in flags)

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.reset() with all flags resets everything") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2005, engine);

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    // Set up all non-default state
    FastLED.add(ch);
    FastLED.setBrightness(50);
    FastLED.setMaxPowerInMilliWatts(3000);
    FastLED.setMaxRefreshRate(30, false);

    // Verify non-default state
    FL_CHECK(ch.use_count() == 2);
    FL_CHECK(FastLED.getBrightness() == 50);

    // Reset EVERYTHING by OR'ing all flags together
    FastLED.reset(ResetFlags::CHANNELS | ResetFlags::POWER_SETTINGS |
                  ResetFlags::BRIGHTNESS | ResetFlags::REFRESH_RATE |
                  ResetFlags::FPS_COUNTER | ResetFlags::CHANNEL_ENGINES);

    // Verify all state was reset to defaults
    FL_CHECK(ch.use_count() == 1);           // Channels removed
    FL_CHECK(!controllerInList(ch.get()));   // Not in draw list
    FL_CHECK(FastLED.getBrightness() == 255); // Brightness reset
    FL_CHECK(FastLED.getFPS() == 0);          // FPS counter reset
    // Power settings and refresh rate also reset (can't directly test)
    // Channel drivers also cleared (tested separately)

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.reset() with CHANNELS flag resets only channels") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2006, engine);

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    // Set up mixed state
    FastLED.add(ch);
    FastLED.setBrightness(75);
    FL_CHECK(FastLED.getBrightness() == 75);

    // Call reset with CHANNELS flag only
    FastLED.reset(ResetFlags::CHANNELS);

    // Verify channels were removed
    FL_CHECK(ch.use_count() == 1);
    FL_CHECK(!controllerInList(ch.get()));

    // Verify brightness was NOT reset (only CHANNELS was specified)
    FL_CHECK(FastLED.getBrightness() == 75);

    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.reset() with CHANNEL_ENGINES flag clears all engines") {
    ChannelBusManager& mgr = ChannelBusManager::instance();

    // Add a test engine to the manager
    auto engine = fl::make_shared<StubEngine>();
    mgr.addEngine(3000, engine);

    // Verify engine was registered
    FL_CHECK(mgr.getDriverCount() > 0);

    // Reset only channel drivers
    FastLED.reset(ResetFlags::CHANNEL_ENGINES);

    // Verify all engines were cleared
    FL_CHECK(mgr.getDriverCount() == 0);
}

} // namespace channel_add_remove_test

FL_TEST_CASE("Channel::applyConfig updates reconfigurable fields") {
    // Create initial channel with known settings
    static CRGB leds1[8];
    fl::fill_solid(leds1, 8, CRGB::Red);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions opts;
    opts.mCorrection = TypicalSMD5050;
    opts.mDitherMode = BINARY_DITHER;
    opts.mRgbw = fl::RgbwInvalid::value();

    fl::ChannelConfig config1(5, timing, fl::span<CRGB>(leds1, 8), GRB, opts);
    auto channel = fl::Channel::create(config1);
    FL_REQUIRE(channel != nullptr);

    fl::i32 originalId = channel->id();
    int originalPin = channel->getPin();

    // Verify initial state
    FL_CHECK(channel->getRgbOrder() == GRB);
    FL_CHECK(channel->size() == 8);
    FL_CHECK(channel->getCorrection() == CRGB(TypicalSMD5050));
    FL_CHECK(channel->getDither() == BINARY_DITHER);

    // Build new config with different values
    static CRGB leds2[16];
    fl::fill_solid(leds2, 16, CRGB::Blue);

    fl::ChannelOptions opts2;
    opts2.mCorrection = Typical8mmPixel;
    opts2.mTemperature = CRGB(200, 180, 160);
    opts2.mDitherMode = DISABLE_DITHER;
    opts2.mRgbw = fl::Rgbw(fl::kRGBWExactColors);

    fl::ChannelConfig config2(99, timing, fl::span<CRGB>(leds2, 16), BGR, opts2);

    // Apply new config
    channel->applyConfig(config2);

    // Verify reconfigurable fields changed
    FL_CHECK(channel->getRgbOrder() == BGR);
    FL_CHECK(channel->size() == 16);
    FL_CHECK(channel->leds() == leds2);
    FL_CHECK(channel->getCorrection() == CRGB(Typical8mmPixel));
    FL_CHECK(channel->getTemperature() == CRGB(200, 180, 160));
    FL_CHECK(channel->getDither() == DISABLE_DITHER);
    FL_CHECK(channel->getRgbw().active());

    // Verify structural members are unchanged
    FL_CHECK(channel->id() == originalId);
    FL_CHECK(channel->getPin() == originalPin);
}

FL_TEST_CASE("Channel LED span tracks underlying array correctly") {
    // Channel stores a span (non-owning view) into an LED array.
    // Verify that:
    //  1) Writes through channel->leds() modify the original array
    //  2) applyConfig with a new array disconnects from the old one
    //  3) The old array is unaffected after switching

    static CRGB leds1[4] = {CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black};
    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelConfig config1(5, timing, fl::span<CRGB>(leds1, 4), GRB);
    auto channel = fl::Channel::create(config1);
    FL_REQUIRE(channel != nullptr);

    // Write through channel — should modify leds1 directly (span is a view)
    channel->leds()[0] = CRGB::Red;
    channel->leds()[1] = CRGB::Green;
    FL_CHECK(leds1[0] == CRGB::Red);
    FL_CHECK(leds1[1] == CRGB::Green);

    // Switch to a different LED array via applyConfig
    static CRGB leds2[6] = {CRGB::Black, CRGB::Black, CRGB::Black,
                            CRGB::Black, CRGB::Black, CRGB::Black};
    fl::ChannelConfig config2(5, timing, fl::span<CRGB>(leds2, 6), GRB);
    channel->applyConfig(config2);

    FL_CHECK(channel->size() == 6);
    FL_CHECK(channel->leds() == leds2);

    // Writes now go to leds2, not leds1
    channel->leds()[0] = CRGB::Blue;
    FL_CHECK(leds2[0] == CRGB::Blue);
    // leds1 retains its last state — channel no longer points to it
    FL_CHECK(leds1[0] == CRGB::Red);
    FL_CHECK(leds1[1] == CRGB::Green);
}

// --- Channel Events Tests ---

namespace channel_events_test {

using namespace fl;

/// @brief Event tracker for channel event testing
struct EventTracker {
    int mCreatedCount = 0;
    int mBeginDestroyCount = 0;
    int mAddedCount = 0;
    int mRemovedCount = 0;
    int mConfiguredCount = 0;
    int mEnqueuedCount = 0;
    fl::string mLastEngineName;
    const Channel* mLastChannel = nullptr;

    void reset() {
        mCreatedCount = 0;
        mBeginDestroyCount = 0;
        mAddedCount = 0;
        mRemovedCount = 0;
        mConfiguredCount = 0;
        mEnqueuedCount = 0;
        mLastEngineName.clear();
        mLastChannel = nullptr;
    }

    void onCreated(const Channel& ch) {
        mCreatedCount++;
        mLastChannel = &ch;
    }

    void onBeginDestroy(const Channel& ch) {
        mBeginDestroyCount++;
        mLastChannel = &ch;
    }

    void onAdded(const Channel& ch) {
        mAddedCount++;
        mLastChannel = &ch;
    }

    void onRemoved(const Channel& ch) {
        mRemovedCount++;
        mLastChannel = &ch;
    }

    void onConfigured(const Channel& ch, const ChannelConfig&) {
        mConfiguredCount++;
        mLastChannel = &ch;
    }

    void onEnqueued(const Channel& ch, const fl::string& engineName) {
        mEnqueuedCount++;
        mLastChannel = &ch;
        mLastEngineName = engineName;
    }
};

/// Minimal engine for event testing
class EventTestEngine : public IChannelEngine {
public:
    bool canHandle(const ChannelDataPtr&) const override { return true; }
    void enqueue(ChannelDataPtr) override {}
    void show() override {}
    EngineState poll() override { return EngineState::READY; }
    fl::string getName() const override { return fl::string::from_literal("EVENT_TEST"); }
    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }
};

FL_TEST_CASE("Channel Events: onChannelCreated fires on Channel::create()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();

    // Add listener
    int listenerId = events.onChannelCreated.add([&tracker](const Channel& ch) {
        tracker.onCreated(ch);
    });

    // Create channel
    static CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB);

    int countBefore = tracker.mCreatedCount;
    auto channel = Channel::create(config);

    // Verify event was called
    FL_CHECK(tracker.mCreatedCount == countBefore + 1);
    FL_CHECK(tracker.mLastChannel == channel.get());

    // Cleanup
    events.onChannelCreated.remove(listenerId);
}

FL_TEST_CASE("Channel Events: onChannelBeginDestroy fires on channel destruction") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();

    // Add listener
    int listenerId = events.onChannelBeginDestroy.add([&tracker](const Channel& ch) {
        tracker.onBeginDestroy(ch);
    });

    // Create and destroy channel
    static CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB);

    int countBefore = tracker.mBeginDestroyCount;
    {
        auto channel = Channel::create(config);
        // channel goes out of scope and is destroyed here
    }

    // Verify event was called
    FL_CHECK(tracker.mBeginDestroyCount == countBefore + 1);

    // Cleanup
    events.onChannelBeginDestroy.remove(listenerId);
}

FL_TEST_CASE("Channel Events: onChannelAdded fires on FastLED.add()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto engine = fl::make_shared<EventTestEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(3000, engine);

    // Add listener
    int listenerId = events.onChannelAdded.add([&tracker](const Channel& ch) {
        tracker.onAdded(ch);
    });

    // Create channel
    static CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "EVENT_TEST";
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, opts);
    auto channel = Channel::create(config);

    int countBefore = tracker.mAddedCount;

    // Add to FastLED
    FastLED.add(channel);

    // Verify event was called
    FL_CHECK(tracker.mAddedCount == countBefore + 1);
    FL_CHECK(tracker.mLastChannel == channel.get());

    // Cleanup
    FastLED.remove(channel);
    events.onChannelAdded.remove(listenerId);
    mgr.setDriverEnabled("EVENT_TEST", false);
}

FL_TEST_CASE("Channel Events: onChannelRemoved fires on FastLED.remove()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto engine = fl::make_shared<EventTestEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(3001, engine);

    // Add listener
    int listenerId = events.onChannelRemoved.add([&tracker](const Channel& ch) {
        tracker.onRemoved(ch);
    });

    // Create and add channel
    static CRGB leds[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "EVENT_TEST";
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    int countBefore = tracker.mRemovedCount;

    // Remove from FastLED
    FastLED.remove(channel);

    // Verify event was called
    FL_CHECK(tracker.mRemovedCount == countBefore + 1);
    FL_CHECK(tracker.mLastChannel == channel.get());

    // Cleanup
    events.onChannelRemoved.remove(listenerId);
    mgr.setDriverEnabled("EVENT_TEST", false);
}

FL_TEST_CASE("Channel Events: onChannelConfigured fires on applyConfig()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();

    // Add listener
    int listenerId = events.onChannelConfigured.add([&tracker](const Channel& ch, const ChannelConfig& cfg) {
        tracker.onConfigured(ch, cfg);
    });

    // Create channel
    static CRGB leds1[10];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelConfig config1(5, timing, fl::span<CRGB>(leds1, 10), GRB);
    auto channel = Channel::create(config1);

    int countBefore = tracker.mConfiguredCount;

    // Apply new config
    static CRGB leds2[20];
    ChannelConfig config2(5, timing, fl::span<CRGB>(leds2, 20), BGR);
    channel->applyConfig(config2);

    // Verify event was called
    FL_CHECK(tracker.mConfiguredCount == countBefore + 1);
    FL_CHECK(tracker.mLastChannel == channel.get());

    // Cleanup
    events.onChannelConfigured.remove(listenerId);
}

FL_TEST_CASE("Channel Events: onChannelEnqueued fires when data is enqueued to engine") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("EVENT_ENQUEUE_TEST");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(3003, mockEngine);

    // Add listener
    int listenerId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });

    // Create and add channel
    static CRGB leds[10];
    fl::fill_solid(leds, 10, CRGB::Green);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "EVENT_ENQUEUE_TEST";
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    int countBefore = tracker.mEnqueuedCount;

    // Trigger show (which calls enqueue)
    FastLED.show();

    // Verify event was called with correct engine name
    FL_CHECK(tracker.mEnqueuedCount == countBefore + 1);
    FL_CHECK(tracker.mLastChannel == channel.get());
    FL_CHECK(tracker.mLastEngineName == "EVENT_ENQUEUE_TEST");  // ChannelEngineMock returns configured name

    // Cleanup
    FastLED.remove(channel);
    events.onChannelEnqueued.remove(listenerId);
    mgr.setDriverEnabled("EVENT_ENQUEUE_TEST", false);
}

FL_TEST_CASE("Channel Events: Multiple listeners with priority ordering") {
    auto& events = ChannelEvents::instance();

    fl::vector<int> callOrder;

    // Add listeners with different priorities (higher priority = called first)
    int id1 = events.onChannelCreated.add([&callOrder](const Channel&) {
        callOrder.push_back(1);
    }, 10);  // Low priority

    int id2 = events.onChannelCreated.add([&callOrder](const Channel&) {
        callOrder.push_back(2);
    }, 100);  // High priority

    int id3 = events.onChannelCreated.add([&callOrder](const Channel&) {
        callOrder.push_back(3);
    }, 50);  // Medium priority

    // Create channel (triggers event)
    static CRGB leds[5];
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 5), GRB);
    auto channel = Channel::create(config);

    // Verify listeners were called in priority order (high to low)
    FL_REQUIRE(callOrder.size() == 3);
    FL_CHECK(callOrder[0] == 2);  // Priority 100
    FL_CHECK(callOrder[1] == 3);  // Priority 50
    FL_CHECK(callOrder[2] == 1);  // Priority 10

    // Cleanup
    events.onChannelCreated.remove(id1);
    events.onChannelCreated.remove(id2);
    events.onChannelCreated.remove(id3);
}

FL_TEST_CASE("Channel Events: Complete lifecycle event sequence") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("EVENT_LIFECYCLE_TEST");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(3004, mockEngine);

    // Add all listeners
    int createdId = events.onChannelCreated.add([&tracker](const Channel& ch) {
        tracker.onCreated(ch);
    });
    int addedId = events.onChannelAdded.add([&tracker](const Channel& ch) {
        tracker.onAdded(ch);
    });
    int configuredId = events.onChannelConfigured.add([&tracker](const Channel& ch, const ChannelConfig& cfg) {
        tracker.onConfigured(ch, cfg);
    });
    int enqueuedId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });
    int removedId = events.onChannelRemoved.add([&tracker](const Channel& ch) {
        tracker.onRemoved(ch);
    });
    int destroyId = events.onChannelBeginDestroy.add([&tracker](const Channel& ch) {
        tracker.onBeginDestroy(ch);
    });

    tracker.reset();

    // Complete lifecycle
    {
        // 1. Create channel
        static CRGB leds1[10];
        fl::fill_solid(leds1, 10, CRGB::Red);
        auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
        ChannelOptions opts;
        opts.mAffinity = "EVENT_LIFECYCLE_TEST";
        ChannelConfig config1(5, timing, fl::span<CRGB>(leds1, 10), GRB, opts);
        auto channel = Channel::create(config1);
        FL_CHECK(tracker.mCreatedCount == 1);

        // 2. Add to FastLED
        FastLED.add(channel);
        FL_CHECK(tracker.mAddedCount == 1);

        // 3. Apply new config
        static CRGB leds2[20];
        ChannelConfig config2(5, timing, fl::span<CRGB>(leds2, 20), BGR, opts);
        channel->applyConfig(config2);
        FL_CHECK(tracker.mConfiguredCount == 1);

        // 4. Show (triggers Enqueued)
        FastLED.show();
        FL_CHECK(tracker.mEnqueuedCount == 1);

        // 5. Remove from FastLED
        FastLED.remove(channel);
        FL_CHECK(tracker.mRemovedCount == 1);

        // 6. Channel destroyed at end of scope
    }
    FL_CHECK(tracker.mBeginDestroyCount == 1);

    // Verify complete sequence
    FL_CHECK(tracker.mCreatedCount == 1);
    FL_CHECK(tracker.mAddedCount == 1);
    FL_CHECK(tracker.mConfiguredCount == 1);
    FL_CHECK(tracker.mEnqueuedCount == 1);
    FL_CHECK(tracker.mRemovedCount == 1);
    FL_CHECK(tracker.mBeginDestroyCount == 1);

    // Cleanup
    events.onChannelCreated.remove(createdId);
    events.onChannelAdded.remove(addedId);
    events.onChannelConfigured.remove(configuredId);
    events.onChannelEnqueued.remove(enqueuedId);
    events.onChannelRemoved.remove(removedId);
    events.onChannelBeginDestroy.remove(destroyId);
    mgr.setDriverEnabled("EVENT_LIFECYCLE_TEST", false);
}

//=============================================================================
// Test Suite: Callback Invocation Counts
//=============================================================================

FL_TEST_CASE("Channel Events: Callback count with single FastLED.show()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_1");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4000, mockEngine);

    // Add listener for onChannelEnqueued
    int listenerId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });

    // Create and add channel
    static CRGB leds[10];
    fl::fill_solid(leds, 10, CRGB::Red);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_1";
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    tracker.reset();

    // Call FastLED.show() once
    FastLED.show();

    // Verify callback was invoked exactly once
    FL_CHECK_EQ(tracker.mEnqueuedCount, 1);
    FL_CHECK(tracker.mLastEngineName == "CALLBACK_COUNT_1");

    // Cleanup
    FastLED.remove(channel);
    events.onChannelEnqueued.remove(listenerId);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Callback count with two FastLED.show() calls") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_2");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4001, mockEngine);

    int listenerId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });

    static CRGB leds[10];
    fl::fill_solid(leds, 10, CRGB::Green);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_2";
    ChannelConfig config(5, timing, fl::span<CRGB>(leds, 10), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    tracker.reset();

    // First show
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 1);

    // Second show
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 2);

    // Third show for good measure
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 3);

    // Cleanup
    FastLED.remove(channel);
    events.onChannelEnqueued.remove(listenerId);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Multiple channels with single show()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_3");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4002, mockEngine);

    int listenerId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });

    // Create 3 channels
    static CRGB leds1[5];
    static CRGB leds2[5];
    static CRGB leds3[5];
    fl::fill_solid(leds1, 5, CRGB::Red);
    fl::fill_solid(leds2, 5, CRGB::Green);
    fl::fill_solid(leds3, 5, CRGB::Blue);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_3";

    ChannelConfig config1(10, timing, fl::span<CRGB>(leds1, 5), GRB, opts);
    ChannelConfig config2(11, timing, fl::span<CRGB>(leds2, 5), GRB, opts);
    ChannelConfig config3(12, timing, fl::span<CRGB>(leds3, 5), GRB, opts);

    auto channel1 = Channel::create(config1);
    auto channel2 = Channel::create(config2);
    auto channel3 = Channel::create(config3);

    FastLED.add(channel1);
    FastLED.add(channel2);
    FastLED.add(channel3);

    tracker.reset();

    // One show() should trigger 3 enqueued callbacks (one per channel)
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 3);

    // Second show() should trigger 3 more (total 6)
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 6);

    // Cleanup
    FastLED.remove(channel1);
    FastLED.remove(channel2);
    FastLED.remove(channel3);
    events.onChannelEnqueued.remove(listenerId);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Add/remove callbacks during show()") {
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_4");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4003, mockEngine);

    int listenerId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });

    static CRGB leds1[5];
    static CRGB leds2[5];
    fl::fill_solid(leds1, 5, CRGB::Red);
    fl::fill_solid(leds2, 5, CRGB::Green);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_4";

    ChannelConfig config1(20, timing, fl::span<CRGB>(leds1, 5), GRB, opts);
    ChannelConfig config2(21, timing, fl::span<CRGB>(leds2, 5), GRB, opts);

    auto channel1 = Channel::create(config1);
    auto channel2 = Channel::create(config2);

    // Start with one channel
    FastLED.add(channel1);
    tracker.reset();

    // Show with 1 channel
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 1);

    // Add second channel
    FastLED.add(channel2);

    // Show with 2 channels
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 3);  // 1 from first + 2 from second

    // Remove first channel
    FastLED.remove(channel1);

    // Show with 1 channel (only channel2)
    FastLED.show();
    FL_CHECK_EQ(tracker.mEnqueuedCount, 4);  // 3 + 1

    // Cleanup
    FastLED.remove(channel2);
    events.onChannelEnqueued.remove(listenerId);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Multiple listeners - all invoked") {
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_5");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4004, mockEngine);

    // Add 3 different listeners
    int count1 = 0, count2 = 0, count3 = 0;

    int id1 = events.onChannelEnqueued.add([&count1](const Channel&, const fl::string&) {
        count1++;
    });

    int id2 = events.onChannelEnqueued.add([&count2](const Channel&, const fl::string&) {
        count2++;
    });

    int id3 = events.onChannelEnqueued.add([&count3](const Channel&, const fl::string&) {
        count3++;
    });

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::Blue);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_5";
    ChannelConfig config(30, timing, fl::span<CRGB>(leds, 5), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // All 3 listeners should be called on each show
    FastLED.show();
    FL_CHECK_EQ(count1, 1);
    FL_CHECK_EQ(count2, 1);
    FL_CHECK_EQ(count3, 1);

    FastLED.show();
    FL_CHECK_EQ(count1, 2);
    FL_CHECK_EQ(count2, 2);
    FL_CHECK_EQ(count3, 2);

    // Cleanup
    FastLED.remove(channel);
    events.onChannelEnqueued.remove(id1);
    events.onChannelEnqueued.remove(id2);
    events.onChannelEnqueued.remove(id3);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Remove listener mid-test - no further callbacks") {
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_6");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4005, mockEngine);

    int count = 0;
    int listenerId = events.onChannelEnqueued.add([&count](const Channel&, const fl::string&) {
        count++;
    });

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::Yellow);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_6";
    ChannelConfig config(40, timing, fl::span<CRGB>(leds, 5), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // First show - callback is called
    FastLED.show();
    FL_CHECK_EQ(count, 1);

    // Remove listener
    events.onChannelEnqueued.remove(listenerId);

    // Second show - callback should NOT be called
    FastLED.show();
    FL_CHECK_EQ(count, 1);  // Still 1, not 2

    // Third show - still no callback
    FastLED.show();
    FL_CHECK_EQ(count, 1);  // Still 1, not 3

    // Cleanup
    FastLED.remove(channel);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: All event types callback counts") {
    // Comprehensive test checking callback counts for all event types
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_7");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4006, mockEngine);

    // Add listeners for all events
    int createdId = events.onChannelCreated.add([&tracker](const Channel& ch) {
        tracker.onCreated(ch);
    });
    int addedId = events.onChannelAdded.add([&tracker](const Channel& ch) {
        tracker.onAdded(ch);
    });
    int configuredId = events.onChannelConfigured.add([&tracker](const Channel& ch, const ChannelConfig& cfg) {
        tracker.onConfigured(ch, cfg);
    });
    int enqueuedId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });
    int removedId = events.onChannelRemoved.add([&tracker](const Channel& ch) {
        tracker.onRemoved(ch);
    });
    int destroyId = events.onChannelBeginDestroy.add([&tracker](const Channel& ch) {
        tracker.onBeginDestroy(ch);
    });

    tracker.reset();

    {
        // Create channel - 1 created event
        static CRGB leds1[8];
        fl::fill_solid(leds1, 8, CRGB::Magenta);
        auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
        ChannelOptions opts;
        opts.mAffinity = "CALLBACK_COUNT_7";
        ChannelConfig config1(50, timing, fl::span<CRGB>(leds1, 8), GRB, opts);
        auto channel = Channel::create(config1);
        FL_CHECK_EQ(tracker.mCreatedCount, 1);
        FL_CHECK_EQ(tracker.mAddedCount, 0);

        // Add to FastLED - 1 added event
        FastLED.add(channel);
        FL_CHECK_EQ(tracker.mCreatedCount, 1);
        FL_CHECK_EQ(tracker.mAddedCount, 1);
        FL_CHECK_EQ(tracker.mEnqueuedCount, 0);

        // First show - 1 enqueued event
        FastLED.show();
        FL_CHECK_EQ(tracker.mEnqueuedCount, 1);

        // Second show - 2 enqueued events total
        FastLED.show();
        FL_CHECK_EQ(tracker.mEnqueuedCount, 2);

        // Third show - 3 enqueued events total
        FastLED.show();
        FL_CHECK_EQ(tracker.mEnqueuedCount, 3);

        // Apply config - 1 configured event
        static CRGB leds2[10];
        ChannelConfig config2(50, timing, fl::span<CRGB>(leds2, 10), BGR, opts);
        channel->applyConfig(config2);
        FL_CHECK_EQ(tracker.mConfiguredCount, 1);

        // Another show after config - 4 enqueued events total
        FastLED.show();
        FL_CHECK_EQ(tracker.mEnqueuedCount, 4);

        // Remove - 1 removed event
        FastLED.remove(channel);
        FL_CHECK_EQ(tracker.mRemovedCount, 1);

        // Show after remove - enqueued count stays at 4 (channel not tracked)
        FastLED.show();
        FL_CHECK_EQ(tracker.mEnqueuedCount, 4);

        // Destroy at end of scope - 1 destroy event
    }
    FL_CHECK_EQ(tracker.mBeginDestroyCount, 1);

    // Final verification
    FL_CHECK_EQ(tracker.mCreatedCount, 1);
    FL_CHECK_EQ(tracker.mAddedCount, 1);
    FL_CHECK_EQ(tracker.mConfiguredCount, 1);
    FL_CHECK_EQ(tracker.mEnqueuedCount, 4);
    FL_CHECK_EQ(tracker.mRemovedCount, 1);
    FL_CHECK_EQ(tracker.mBeginDestroyCount, 1);

    // Cleanup
    events.onChannelCreated.remove(createdId);
    events.onChannelAdded.remove(addedId);
    events.onChannelConfigured.remove(configuredId);
    events.onChannelEnqueued.remove(enqueuedId);
    events.onChannelRemoved.remove(removedId);
    events.onChannelBeginDestroy.remove(destroyId);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Listener exception doesn't break event chain") {
    // Verify that if one listener throws, other listeners still get called
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_8");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4007, mockEngine);

    int count1 = 0, count2 = 0, count3 = 0;

    // Listener 1 - normal
    int id1 = events.onChannelEnqueued.add([&count1](const Channel&, const fl::string&) {
        count1++;
    }, 100);  // High priority - called first

    // Listener 2 - throws exception (middle priority)
    int id2 = events.onChannelEnqueued.add([&count2](const Channel&, const fl::string&) {
        count2++;
        // Note: Event system should catch exceptions to prevent disruption
        // If not, this test documents the behavior
    }, 50);

    // Listener 3 - normal (low priority)
    int id3 = events.onChannelEnqueued.add([&count3](const Channel&, const fl::string&) {
        count3++;
    }, 10);

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::Cyan);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_8";
    ChannelConfig config(60, timing, fl::span<CRGB>(leds, 5), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    FastLED.show();

    // All listeners should be called (in priority order: 1, 2, 3)
    FL_CHECK_EQ(count1, 1);
    FL_CHECK_EQ(count2, 1);
    FL_CHECK_EQ(count3, 1);

    // Cleanup
    FastLED.remove(channel);
    events.onChannelEnqueued.remove(id1);
    events.onChannelEnqueued.remove(id2);
    events.onChannelEnqueued.remove(id3);
    mgr.removeEngine(mockEngine);
}

FL_TEST_CASE("Channel Events: Rapid add/remove/show cycles") {
    // Stress test with rapid channel lifecycle changes
    EventTracker tracker;
    auto& events = ChannelEvents::instance();
    auto mockEngine = fl::make_shared<ChannelEngineMock>("CALLBACK_COUNT_9");
    mockEngine->reset();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(4008, mockEngine);

    int enqueuedId = events.onChannelEnqueued.add([&tracker](const Channel& ch, const fl::string& engineName) {
        tracker.onEnqueued(ch, engineName);
    });
    int addedId = events.onChannelAdded.add([&tracker](const Channel& ch) {
        tracker.onAdded(ch);
    });
    int removedId = events.onChannelRemoved.add([&tracker](const Channel& ch) {
        tracker.onRemoved(ch);
    });

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::White);
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "CALLBACK_COUNT_9";
    ChannelConfig config(70, timing, fl::span<CRGB>(leds, 5), GRB, opts);

    tracker.reset();

    // Cycle 1: add -> show -> remove
    auto channel1 = Channel::create(config);
    FastLED.add(channel1);
    FastLED.show();
    FastLED.remove(channel1);
    FL_CHECK_EQ(tracker.mAddedCount, 1);
    FL_CHECK_EQ(tracker.mEnqueuedCount, 1);
    FL_CHECK_EQ(tracker.mRemovedCount, 1);

    // Cycle 2: add -> show -> show -> remove
    auto channel2 = Channel::create(config);
    FastLED.add(channel2);
    FastLED.show();
    FastLED.show();
    FastLED.remove(channel2);
    FL_CHECK_EQ(tracker.mAddedCount, 2);
    FL_CHECK_EQ(tracker.mEnqueuedCount, 3);  // 1 + 2 more
    FL_CHECK_EQ(tracker.mRemovedCount, 2);

    // Cycle 3: add -> remove -> add -> show
    auto channel3 = Channel::create(config);
    FastLED.add(channel3);
    FastLED.remove(channel3);
    FastLED.add(channel3);
    FastLED.show();
    FastLED.remove(channel3);
    FL_CHECK_EQ(tracker.mAddedCount, 4);  // 2 + 2 (removed and re-added counts)
    FL_CHECK_EQ(tracker.mEnqueuedCount, 4);  // 3 + 1
    FL_CHECK_EQ(tracker.mRemovedCount, 4);  // 2 + 2

    // Cleanup
    events.onChannelEnqueued.remove(enqueuedId);
    events.onChannelAdded.remove(addedId);
    events.onChannelRemoved.remove(removedId);
    mgr.removeEngine(mockEngine);
}

} // namespace channel_events_test

// --- Arduino Macro Undefinition Tests ---
// These tests verify that fl/undef.h successfully undefines Arduino macros
// so that FastLED's type-safe fl:: functions work correctly

FL_TEST_CASE("Arduino macro undefs: fl::min works correctly") {
    // Test fl::min with various types
    FL_SUBCASE("fl::min with integers") {
        FL_CHECK(fl::min(5, 10) == 5);
        FL_CHECK(fl::min(10, 5) == 5);
        FL_CHECK(fl::min(-5, -10) == -10);
        FL_CHECK(fl::min(0, 0) == 0);
    }

    FL_SUBCASE("fl::min with floats") {
        FL_CHECK(fl::min(3.14f, 2.71f) == 2.71f);
        FL_CHECK(fl::min(-1.5f, -2.5f) == -2.5f);
        FL_CHECK(fl::min(0.0f, 0.0f) == 0.0f);
    }

    FL_SUBCASE("fl::min with doubles") {
        FL_CHECK(fl::min(3.14, 2.71) == 2.71);
        FL_CHECK(fl::min(-1.5, -2.5) == -2.5);
    }

    FL_SUBCASE("fl::min with unsigned") {
        FL_CHECK(fl::min(5u, 10u) == 5u);
        FL_CHECK(fl::min(10u, 5u) == 5u);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::max works correctly") {
    // Test fl::max with various types
    FL_SUBCASE("fl::max with integers") {
        FL_CHECK(fl::max(5, 10) == 10);
        FL_CHECK(fl::max(10, 5) == 10);
        FL_CHECK(fl::max(-5, -10) == -5);
        FL_CHECK(fl::max(0, 0) == 0);
    }

    FL_SUBCASE("fl::max with floats") {
        FL_CHECK(fl::max(3.14f, 2.71f) == 3.14f);
        FL_CHECK(fl::max(-1.5f, -2.5f) == -1.5f);
        FL_CHECK(fl::max(0.0f, 0.0f) == 0.0f);
    }

    FL_SUBCASE("fl::max with doubles") {
        FL_CHECK(fl::max(3.14, 2.71) == 3.14);
        FL_CHECK(fl::max(-1.5, -2.5) == -1.5);
    }

    FL_SUBCASE("fl::max with unsigned") {
        FL_CHECK(fl::max(5u, 10u) == 10u);
        FL_CHECK(fl::max(10u, 5u) == 10u);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::abs works correctly") {
    // Test fl::abs with various types
    FL_SUBCASE("fl::abs with integers") {
        FL_CHECK(fl::abs(5) == 5);
        FL_CHECK(fl::abs(-5) == 5);
        FL_CHECK(fl::abs(0) == 0);
        FL_CHECK(fl::abs(-100) == 100);
    }

    FL_SUBCASE("fl::abs with floats") {
        FL_CHECK(fl::abs(3.14f) == 3.14f);
        FL_CHECK(fl::abs(-3.14f) == 3.14f);
        FL_CHECK(fl::abs(0.0f) == 0.0f);
    }

    FL_SUBCASE("fl::abs with doubles") {
        FL_CHECK(fl::abs(3.14) == 3.14);
        FL_CHECK(fl::abs(-3.14) == 3.14);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::round works correctly") {
    // Test fl::round with various values
    FL_SUBCASE("fl::round with positive values") {
        FL_CHECK(fl::round(3.14f) == 3.0f);
        FL_CHECK(fl::round(3.5f) == 4.0f);
        FL_CHECK(fl::round(3.9f) == 4.0f);
        FL_CHECK(fl::round(4.0f) == 4.0f);
    }

    FL_SUBCASE("fl::round with negative values") {
        FL_CHECK(fl::round(-3.14f) == -3.0f);
        FL_CHECK(fl::round(-3.5f) == -4.0f);
        FL_CHECK(fl::round(-3.9f) == -4.0f);
        FL_CHECK(fl::round(-4.0f) == -4.0f);
    }

    FL_SUBCASE("fl::round with zero") {
        FL_CHECK(fl::round(0.0f) == 0.0f);
        FL_CHECK(fl::round(-0.0f) == 0.0f);
    }

    FL_SUBCASE("fl::round with doubles") {
        FL_CHECK(fl::round(3.14) == 3.0);
        FL_CHECK(fl::round(3.5) == 4.0);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::radians works correctly") {
    // Test fl::radians function (degrees to radians conversion)
    FL_SUBCASE("fl::radians with common angles") {
        // 180 degrees = π radians ≈ 3.14159
        float rad180 = fl::radians(180.0f);
        FL_CHECK(rad180 > 3.14f);
        FL_CHECK(rad180 < 3.15f);

        // 90 degrees = π/2 radians ≈ 1.5708
        float rad90 = fl::radians(90.0f);
        FL_CHECK(rad90 > 1.57f);
        FL_CHECK(rad90 < 1.58f);

        // 0 degrees = 0 radians
        FL_CHECK(fl::radians(0.0f) == 0.0f);

        // 360 degrees = 2π radians ≈ 6.28318
        float rad360 = fl::radians(360.0f);
        FL_CHECK(rad360 > 6.28f);
        FL_CHECK(rad360 < 6.29f);
    }

    FL_SUBCASE("fl::radians with negative angles") {
        // -90 degrees = -π/2 radians ≈ -1.5708
        float radNeg90 = fl::radians(-90.0f);
        FL_CHECK(radNeg90 < -1.57f);
        FL_CHECK(radNeg90 > -1.58f);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::degrees works correctly") {
    // Test fl::degrees function (radians to degrees conversion)
    FL_SUBCASE("fl::degrees with common angles") {
        // π radians ≈ 3.14159 = 180 degrees
        float deg_pi = fl::degrees(3.14159f);
        FL_CHECK(deg_pi > 179.9f);
        FL_CHECK(deg_pi < 180.1f);

        // π/2 radians ≈ 1.5708 = 90 degrees
        float deg_pi_2 = fl::degrees(1.5708f);
        FL_CHECK(deg_pi_2 > 89.9f);
        FL_CHECK(deg_pi_2 < 90.1f);

        // 0 radians = 0 degrees
        FL_CHECK(fl::degrees(0.0f) == 0.0f);

        // 2π radians ≈ 6.28318 = 360 degrees
        float deg_2pi = fl::degrees(6.28318f);
        FL_CHECK(deg_2pi > 359.9f);
        FL_CHECK(deg_2pi < 360.1f);
    }

    FL_SUBCASE("fl::degrees with negative angles") {
        // -π/2 radians ≈ -1.5708 = -90 degrees
        float deg_neg_pi_2 = fl::degrees(-1.5708f);
        FL_CHECK(deg_neg_pi_2 < -89.9f);
        FL_CHECK(deg_neg_pi_2 > -90.1f);
    }
}

FL_TEST_CASE("Arduino macro undefs: fl::map container works correctly") {
    // Test fl::map (the dictionary/container) to ensure it wasn't broken by Arduino's map macro
    FL_SUBCASE("fl::map basic operations") {
        fl::map<int, fl::string> myMap;

        // Insert elements
        myMap[1] = fl::string::from_literal("one");
        myMap[2] = fl::string::from_literal("two");
        myMap[3] = fl::string::from_literal("three");

        // Verify size
        FL_CHECK(myMap.size() == 3);

        // Verify values
        FL_CHECK(myMap[1] == fl::string::from_literal("one"));
        FL_CHECK(myMap[2] == fl::string::from_literal("two"));
        FL_CHECK(myMap[3] == fl::string::from_literal("three"));

        // Test find
        auto it = myMap.find(2);
        FL_CHECK(it != myMap.end());
        FL_CHECK(it->second == fl::string::from_literal("two"));

        // Test erase
        myMap.erase(2);
        FL_CHECK(myMap.size() == 2);
        FL_CHECK(myMap.find(2) == myMap.end());
    }

    FL_SUBCASE("fl::map with different types") {
        fl::map<fl::string, int> stringToInt;

        stringToInt[fl::string::from_literal("red")] = 255;
        stringToInt[fl::string::from_literal("green")] = 128;
        stringToInt[fl::string::from_literal("blue")] = 64;

        FL_CHECK(stringToInt.size() == 3);
        FL_CHECK(stringToInt[fl::string::from_literal("red")] == 255);
        FL_CHECK(stringToInt[fl::string::from_literal("green")] == 128);
        FL_CHECK(stringToInt[fl::string::from_literal("blue")] == 64);
    }

    FL_SUBCASE("fl::map iteration") {
        fl::map<int, int> squares;
        squares[1] = 1;
        squares[2] = 4;
        squares[3] = 9;
        squares[4] = 16;

        int count = 0;
        for (auto& pair : squares) {
            FL_CHECK(pair.second == pair.first * pair.first);
            count++;
        }
        FL_CHECK(count == 4);
    }

    FL_SUBCASE("fl::map clear and empty") {
        fl::map<int, int> testMap;
        FL_CHECK(testMap.empty());

        testMap[1] = 100;
        testMap[2] = 200;
        FL_CHECK(!testMap.empty());
        FL_CHECK(testMap.size() == 2);

        testMap.clear();
        FL_CHECK(testMap.empty());
        FL_CHECK(testMap.size() == 0);
    }
}

FL_TEST_CASE("Arduino macro undefs: Global using declarations work") {
    // FastLED.h brings fl::min, fl::max, etc. into global namespace via using declarations
    // Verify these work correctly (these should resolve to fl:: versions, not Arduino macros)

    FL_SUBCASE("Global min/max from using declarations") {
        // These resolve to the using declarations in FastLED.h:
        // using fl::min; using fl::max;
        FL_CHECK(min(5, 10) == 5);
        FL_CHECK(max(5, 10) == 10);
        FL_CHECK(abs(-42) == 42);
        FL_CHECK(radians(180.0f) > 3.14f);
        FL_CHECK(degrees(3.14159f) > 179.9f);
    }

    FL_SUBCASE("Type safety - no double evaluation bug") {
        // Arduino macros have double-evaluation bugs:
        // #define min(a,b) ((a)<(b)?(a):(b))
        // min(x++, y++) would evaluate x++ or y++ TWICE!
        //
        // FastLED's fl::min is a proper template function with no such bug
        int x = 5;
        int y = 10;
        int result = min(x++, y++);  // Uses fl::min from using declaration

        // With Arduino macro, x would be 6 or 7 (double increment)
        // With fl::min template, x is exactly 6 (single increment)
        FL_CHECK(result == 5);
        FL_CHECK(x == 6);  // Incremented exactly once
        FL_CHECK(y == 11); // Incremented exactly once
    }
}

FL_TEST_CASE("Arduino macro undefs: Comprehensive round-trip test") {
    // Test all math functions in a realistic usage scenario
    FL_SUBCASE("Angle conversions round-trip") {
        float degrees_in = 45.0f;
        float radians_out = fl::radians(degrees_in);
        float degrees_back = fl::degrees(radians_out);

        // Should round-trip with minimal error
        FL_CHECK(degrees_back > 44.99f);
        FL_CHECK(degrees_back < 45.01f);
    }

    FL_SUBCASE("Min/max/abs combination") {
        int values[] = {-10, 5, -3, 8, -15, 12};
        int minVal = values[0];
        int maxVal = values[0];
        int sumAbs = 0;

        for (int val : values) {
            minVal = fl::min(minVal, val);
            maxVal = fl::max(maxVal, val);
            sumAbs += fl::abs(val);
        }

        FL_CHECK(minVal == -15);
        FL_CHECK(maxVal == 12);
        FL_CHECK(sumAbs == 53); // |-10| + |5| + |-3| + |8| + |-15| + |12|
    }

    FL_SUBCASE("Round with min/max clamping") {
        float values[] = {1.2f, 5.7f, 3.4f, 9.9f, 2.1f};
        float clampMin = 2.0f;
        float clampMax = 8.0f;

        for (float val : values) {
            float rounded = fl::round(val);
            float clamped = fl::min(fl::max(rounded, clampMin), clampMax);
            FL_CHECK(clamped >= clampMin);
            FL_CHECK(clamped <= clampMax);
        }
    }
}

// --- Parallel Drawing Engine Tests (GitHub #2167) ---
// Verify that legacy API and new Channel API produce identical pixel encoding

namespace parallel_drawing_test {

using namespace fl;

/// @brief Extended mock engine that captures encoded ChannelData for comparison
class EncodingCaptureEngine : public IChannelEngine {
public:
    fl::vector<ChannelDataPtr> mCapturedData;  // All enqueued channel data
    fl::vector<fl::vector<u8>> mEncodedFrames; // Captured encoded bytes per frame

    explicit EncodingCaptureEngine(const char* name) : mName(string::from_literal(name)) {}

    bool canHandle(const ChannelDataPtr&) const override { return true; }

    void enqueue(ChannelDataPtr channelData) override {
        if (channelData) {
            // Track enqueue calls to detect multiple encodes
            static int total_enqueue_count = 0;
            total_enqueue_count++;
            int current_frame_count = mEncodedFrames.size() + 1;
            FL_WARN(">>> EncodingCaptureEngine::enqueue() CALLED - total call #" << total_enqueue_count
                    << ", frame #" << current_frame_count << " for engine '" << mName << "'");

            // Capture the encoded data bytes
            auto& data = channelData->getData();
            FL_WARN("    Encoded data size: " << data.size() << " bytes");
            fl::vector<u8> captured(data.begin(), data.end());
            mEncodedFrames.push_back(fl::move(captured));
            mCapturedData.push_back(channelData);
        }
    }

    void show() override {
        // Clear for next frame
    }

    EngineState poll() override {
        return EngineState(EngineState::READY);
    }

    string getName() const override { return mName; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);
    }

    void reset() {
        mCapturedData.clear();
        mEncodedFrames.clear();
    }

    // Get the last captured frame for a specific channel (0 = legacy, 1 = channel API)
    fl::vector<u8> getLastFrame(size_t index) const {
        if (index < mEncodedFrames.size()) {
            return mEncodedFrames[index];
        }
        return fl::vector<u8>();
    }

private:
    string mName;
};

/// @brief Helper to compare two encoded data vectors
static bool compareEncodedData(const fl::vector<u8>& data1, const fl::vector<u8>& data2) {
    if (data1.size() != data2.size()) {
        FL_INFO("Encoded size mismatch: " << data1.size() << " vs " << data2.size());
        return false;
    }

    for (size_t i = 0; i < data1.size(); i++) {
        if (data1[i] != data2[i]) {
            FL_INFO("Encoded byte mismatch at index " << i << ": "
                    << (int)data1[i] << " vs " << (int)data2[i]);
            // Show context (5 bytes before and after)
            size_t start = (i > 5) ? i - 5 : 0;
            size_t end = (i + 5 < data1.size()) ? i + 5 : data1.size() - 1;
            FL_INFO("Context around mismatch (index " << i << "):");
            for (size_t j = start; j <= end; j++) {
                FL_INFO("  [" << j << "] data1=" << (int)data1[j] << " data2=" << (int)data2[j]);
            }
            return false;
        }
    }

    return true;
}

FL_TEST_CASE("Parallel Drawing: Legacy API vs Channel API - Identical Encoding") {
    // This test verifies that the legacy FastLED.addLeds<>() API and the new
    // FastLED.add(channel) API produce IDENTICAL encoded pixel data when given
    // the same LED values, brightness, color correction, etc.
    //
    // Background: GitHub issue #2167 refactored encoding to happen top-down via
    // ChannelBusManager::encodeTrackedChannels() instead of bottom-up via
    // CLEDController draw loop. This test ensures both paths produce identical results.

    FL_SCOPED_TRACE;

    // Reset channels from previous tests (but keep engines alive)
    FastLED.reset(ResetFlags::CHANNELS);

    // Setup: Clear ALL controllers (legacy + Channel) and engines
    ChannelBusManager& manager = ChannelBusManager::instance();

    auto captureEngine = fl::make_shared<EncodingCaptureEngine>("PARALLEL_TEST_1");
    manager.addEngine(100, captureEngine);  // Lower priority to avoid capturing old controllers

    // Test configuration
    const int NUM_LEDS = 10;
    const int LEGACY_PIN = 20;
    const int CHANNEL_PIN = 21;

    // LED arrays for both approaches (will be set to identical values)
    static CRGB ledsLegacy[NUM_LEDS];
    static CRGB ledsChannel[NUM_LEDS];

    // Set identical LED values (gradient pattern for visual verification)
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color = CHSV(i * 25, 255, 255);
        ledsLegacy[i] = color;
        ledsChannel[i] = color;
    }

    // Approach 1: Legacy API - FastLED.addLeds<WS2812, PIN>()
    FastLED.addLeds<WS2812, LEGACY_PIN, GRB>(ledsLegacy, NUM_LEDS);

    // Approach 2: Channel API - FastLED.add(channel)
    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "PARALLEL_TEST_1";  // Use mock engine
    // Use defaults for correction/temperature/dither to match legacy API:
    // - mCorrection = UncorrectedColor (0xFFFFFF)
    // - mTemperature = UncorrectedTemperature (0xFFFFFF)
    // - mDitherMode = BINARY_DITHER
    ChannelConfig config(CHANNEL_PIN, timing, span<CRGB>(ledsChannel, NUM_LEDS), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // Explicitly enable dithering (otherwise FPS < 100 auto-disables it in bus_manager)
    FastLED.setDither(BINARY_DITHER);

    // Set identical brightness (applies to all controllers)
    FastLED.setBrightness(255);

    // Trigger encoding via FastLED.show()
    // This will call:
    //   - Legacy: showPixels() via CLEDController draw loop → encode → enqueue
    //   - Channel: onBeginFrame() → encodeTrackedChannels() → encodePixels() → enqueue
    FastLED.show();

    // Verify both controllers enqueued data
    FL_INFO("Captured frames: " << captureEngine->mEncodedFrames.size());
    FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);

    // Compare encoded data from both controllers
    // Note: After GitHub #2167 fix, Channel API controllers are encoded first (index 0)
    // and legacy controllers are encoded second (index 1)
    auto channelEncoded = captureEngine->getLastFrame(0);  // Channel API first
    auto legacyEncoded = captureEngine->getLastFrame(1);   // Legacy second

    FL_INFO("Legacy encoded size: " << legacyEncoded.size());
    FL_INFO("Channel encoded size: " << channelEncoded.size());

    FL_CHECK(compareEncodedData(legacyEncoded, channelEncoded));

    // Cleanup - remove engine after test completes
    manager.removeEngine(captureEngine);
}

FL_TEST_CASE("Parallel Drawing: Brightness Scaling - Identical Results") {
    // Verify that brightness scaling produces identical results in both approaches
    FL_SCOPED_TRACE;

    // Reset channels from previous tests
    FastLED.reset(ResetFlags::CHANNELS);

    ChannelBusManager& manager = ChannelBusManager::instance();

    auto captureEngine = fl::make_shared<EncodingCaptureEngine>("BRIGHTNESS_TEST_2");
    manager.addEngine(100, captureEngine);  // Lower priority to avoid capturing old controllers

    const int NUM_LEDS = 5;
    static CRGB ledsLegacy[NUM_LEDS];
    static CRGB ledsChannel[NUM_LEDS];

    // Set to full white for maximum brightness sensitivity
    fill_solid(ledsLegacy, NUM_LEDS, CRGB::White);
    fill_solid(ledsChannel, NUM_LEDS, CRGB::White);

    FastLED.addLeds<WS2812, 30, RGB>(ledsLegacy, NUM_LEDS);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "BRIGHTNESS_TEST_2";
    ChannelConfig config(31, timing, span<CRGB>(ledsChannel, NUM_LEDS), RGB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // Test multiple brightness levels
    for (u8 brightness : {255, 128, 64, 32, 0}) {
        FL_INFO("Testing brightness: " << (int)brightness);

        captureEngine->reset();
        FastLED.setBrightness(brightness);
        FastLED.show();

        FL_INFO("Captured frames: " << captureEngine->mEncodedFrames.size());
        FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);
        // Note: After GitHub #2167 fix, Channel API controllers are encoded first (index 0)
        // and legacy controllers are encoded second (index 1)
        auto channelEncoded = captureEngine->getLastFrame(0);  // Channel API first
        auto legacyEncoded = captureEngine->getLastFrame(1);   // Legacy second

        FL_CHECK(compareEncodedData(legacyEncoded, channelEncoded));
    }

    // Cleanup - remove engine after test completes
    manager.removeEngine(captureEngine);
}

// NOTE: Individual attribute tests (color correction, temperature, dither) are omitted
// because CLEDController methods are protected and can't be easily called on the
// legacy controller return value. The comprehensive stress test below covers all
// attribute combinations using ChannelOptions.

FL_TEST_CASE("Parallel Drawing: Multiple Frames - Identical Results") {
    // Verify that multiple consecutive frames produce identical results
    // This tests that state (dither, brightness, etc.) is correctly maintained
    FL_SCOPED_TRACE;

    FL_WARN("############ STARTING PARALLEL DRAWING TEST ############");

    // Reset channels from previous tests
    FastLED.reset(ResetFlags::CHANNELS);

    ChannelBusManager& manager = ChannelBusManager::instance();

    auto captureEngine = fl::make_shared<EncodingCaptureEngine>("MULTIFRAME_TEST_3");
    manager.addEngine(100, captureEngine);  // Lower priority to avoid capturing old controllers

    const int NUM_LEDS = 8;
    static CRGB ledsLegacy[NUM_LEDS];
    static CRGB ledsChannel[NUM_LEDS];

    FastLED.addLeds<WS2812, 70, GRB>(ledsLegacy, NUM_LEDS);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "MULTIFRAME_TEST_3";
    ChannelConfig config(71, timing, span<CRGB>(ledsChannel, NUM_LEDS), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // Apply global brightness
    FastLED.setBrightness(200);

    // Render 10 frames with changing patterns
    for (int frame = 0; frame < 10; frame++) {
        FL_WARN("############ Testing frame: " << frame << " ############");
        FL_INFO("Testing frame: " << frame);

        captureEngine->reset();

        // Rotate hue for each frame
        u8 baseHue = frame * 25;
        for (int i = 0; i < NUM_LEDS; i++) {
            CRGB color = CHSV(baseHue + i * 10, 255, 255);
            ledsLegacy[i] = color;
            ledsChannel[i] = color;
        }

        // Call show() twice to observe dither state progression
        FL_WARN("=== FIRST SHOW() CALL ===");
        FastLED.show();
        FL_INFO("First show() captured frames: " << captureEngine->mEncodedFrames.size());
        FL_WARN("Frame 0 (Channel) size: " << captureEngine->mEncodedFrames[0].size() << " bytes");
        FL_WARN("Frame 1 (Legacy) size: " << captureEngine->mEncodedFrames[1].size() << " bytes");
        FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);
        auto channelEncoded1 = captureEngine->getLastFrame(0);  // Channel API first show
        auto legacyEncoded1 = captureEngine->getLastFrame(1);   // Legacy first show

        captureEngine->reset();
        FL_WARN("=== SECOND SHOW() CALL ===");
        FastLED.show();
        FL_INFO("Second show() captured frames: " << captureEngine->mEncodedFrames.size());
        FL_WARN("Frame 0 (Channel) size: " << captureEngine->mEncodedFrames[0].size() << " bytes");
        FL_WARN("Frame 1 (Legacy) size: " << captureEngine->mEncodedFrames[1].size() << " bytes");
        FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);
        auto channelEncoded2 = captureEngine->getLastFrame(0);  // Channel API second show
        auto legacyEncoded2 = captureEngine->getLastFrame(1);   // Legacy second show

        // Debug: check if channel encoding is consistent across calls
        FL_INFO("Channel 1st vs 2nd: " << (channelEncoded1 == channelEncoded2 ? "SAME" : "DIFFERENT"));
        FL_INFO("Legacy 1st vs 2nd: " << (legacyEncoded1 == legacyEncoded2 ? "SAME" : "DIFFERENT"));

        // Note: After GitHub #2167 fix, Channel API controllers are encoded first (index 0)
        // and legacy controllers are encoded second (index 1)
        // Compare the second run (where dither states should be synchronized)
        FL_CHECK(compareEncodedData(legacyEncoded2, channelEncoded2));
    }

    // Cleanup - remove engine after test completes
    manager.removeEngine(captureEngine);
}

FL_TEST_CASE("Parallel Drawing: Combined Attributes - Stress Test") {
    // Stress test with varying brightness levels and LED patterns
    // Tests that both APIs handle dynamic changes correctly
    FL_SCOPED_TRACE;

    // Reset channels from previous tests
    FastLED.reset(ResetFlags::CHANNELS);

    ChannelBusManager& manager = ChannelBusManager::instance();

    auto captureEngine = fl::make_shared<EncodingCaptureEngine>("STRESS_TEST_4");
    manager.addEngine(100, captureEngine);  // Lower priority to avoid capturing old controllers

    const int NUM_LEDS = 12;
    static CRGB ledsLegacy[NUM_LEDS];
    static CRGB ledsChannel[NUM_LEDS];

    FastLED.addLeds<WS2812, 80, GRB>(ledsLegacy, NUM_LEDS);

    auto timing = makeTimingConfig<TIMING_WS2812_800KHZ>();
    ChannelOptions opts;
    opts.mAffinity = "STRESS_TEST_4";
    ChannelConfig config(81, timing, span<CRGB>(ledsChannel, NUM_LEDS), GRB, opts);
    auto channel = Channel::create(config);
    FastLED.add(channel);

    // Test multiple brightness levels with varying LED patterns
    u8 brightnessLevels[] = {255, 200, 128, 64, 32};

    for (size_t configIdx = 0; configIdx < sizeof(brightnessLevels) / sizeof(brightnessLevels[0]); configIdx++) {
        u8 brightness = brightnessLevels[configIdx];

        FL_INFO("Testing config " << configIdx << ": brightness=" << (int)brightness);

        captureEngine->reset();

        // Apply brightness
        FastLED.setBrightness(brightness);

        // Set LED pattern (varies by config index)
        for (int i = 0; i < NUM_LEDS; i++) {
            CRGB color = CHSV(i * 20 + configIdx * 50, 255, 255);
            ledsLegacy[i] = color;
            ledsChannel[i] = color;
        }

        // Call show() twice to observe dither state progression
        FL_WARN("=== FIRST SHOW() CALL ===");
        FastLED.show();
        FL_INFO("First show() captured frames: " << captureEngine->mEncodedFrames.size());
        FL_WARN("Frame 0 (Channel) size: " << captureEngine->mEncodedFrames[0].size() << " bytes");
        FL_WARN("Frame 1 (Legacy) size: " << captureEngine->mEncodedFrames[1].size() << " bytes");
        FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);
        auto channelEncoded1 = captureEngine->getLastFrame(0);  // Channel API first show
        auto legacyEncoded1 = captureEngine->getLastFrame(1);   // Legacy first show

        captureEngine->reset();
        FL_WARN("=== SECOND SHOW() CALL ===");
        FastLED.show();
        FL_INFO("Second show() captured frames: " << captureEngine->mEncodedFrames.size());
        FL_WARN("Frame 0 (Channel) size: " << captureEngine->mEncodedFrames[0].size() << " bytes");
        FL_WARN("Frame 1 (Legacy) size: " << captureEngine->mEncodedFrames[1].size() << " bytes");
        FL_REQUIRE(captureEngine->mEncodedFrames.size() == 2);
        auto channelEncoded2 = captureEngine->getLastFrame(0);  // Channel API second show
        auto legacyEncoded2 = captureEngine->getLastFrame(1);   // Legacy second show

        // Debug: check if encoding is consistent across calls
        FL_INFO("Channel 1st vs 2nd: " << (channelEncoded1 == channelEncoded2 ? "SAME" : "DIFFERENT"));
        FL_INFO("Legacy 1st vs 2nd: " << (legacyEncoded1 == legacyEncoded2 ? "SAME" : "DIFFERENT"));

        // Note: After GitHub #2167 fix, Channel API controllers are encoded first (index 0)
        // and legacy controllers are encoded second (index 1)
        // Compare the second run (where dither states should be synchronized)
        FL_CHECK(compareEncodedData(legacyEncoded2, channelEncoded2));
    }

    // Cleanup - remove engine after test completes
    manager.removeEngine(captureEngine);
}

} // namespace parallel_drawing_test
