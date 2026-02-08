
// g++ --std=c++11 test.cpp


#include "FastLED.h"
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

    const char* getName() const override { return "MOCK"; }

    Capabilities getCapabilities() const override {
        return Capabilities(true, true);  // Mock accepts both clockless and SPI
    }

    void reset() {
        mEnqueueCount = 0;
        mShowCount = 0;
        mEnqueuedChannels.clear();
    }
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

    auto mockEngine = fl::make_shared<ChannelEngineMock>();
    mockEngine->reset();

    // Step 1 & 2: Register mock engine
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK");  // High priority

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

    // Create channel
    auto channel = fl::Channel::create(config);
    FL_REQUIRE(channel != nullptr);
    FL_CHECK(channel->getChannelEngine() == mockEngine.get());

    // Verify channel is NOT in controller list yet (deferred registration)
    FL_CHECK(!channel->isInList());

    // Step 4: Add to FastLED
    FastLED.add(channel);

    // Verify channel IS NOW in controller list (explicit registration)
    FL_CHECK(channel->isInList());

    // Double-check by walking the list
    bool found = false;
    CLEDController* pCur = CLEDController::head();
    while (pCur) {
        if (pCur == channel.get()) {
            found = true;
            break;
        }
        pCur = pCur->next();
    }
    FL_CHECK(found);

    // Step 5 & 6: Call FastLED.show() and verify enqueue()
    int enqueueBefore = mockEngine->mEnqueueCount;
    FastLED.show();

    // Validate: engine received data via enqueue()
    FL_CHECK(mockEngine->mEnqueueCount > enqueueBefore);

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK", false);
}

FL_TEST_CASE("Channel API: Double add protection") {
    // Verify that calling add() multiple times doesn't create duplicates
    auto mockEngine = fl::make_shared<ChannelEngineMock>();
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_DOUBLE");

    static CRGB leds[5];
    fl::fill_solid(leds, 5, CRGB::Green);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_DOUBLE";

    fl::ChannelConfig config(10, timing, fl::span<CRGB>(leds, 5), GRB, options);
    auto channel = fl::Channel::create(config);

    FL_REQUIRE(channel != nullptr);

    // Before adding: not in list
    FL_CHECK(!channel->isInList());

    // First add
    FastLED.add(channel);
    FL_CHECK(channel->isInList());

    // Second add (should be safe, no duplicate)
    FastLED.add(channel);
    FL_CHECK(channel->isInList());

    // Third add (should still be safe)
    FastLED.add(channel);
    FL_CHECK(channel->isInList());

    // Walk the list and count occurrences of this channel
    int occurrenceCount = 0;
    CLEDController* pCur = CLEDController::head();
    while (pCur) {
        if (pCur == channel.get()) {
            occurrenceCount++;
        }
        pCur = pCur->next();
    }

    // Should appear exactly once, not multiple times
    FL_CHECK(occurrenceCount == 1);

    // Clean up
    channel->removeFromDrawList();
    manager.setDriverEnabled("MOCK_DOUBLE", false);
}

FL_TEST_CASE("Channel API: Add and remove symmetry") {
    // Verify that add() and remove() work symmetrically
    auto mockEngine = fl::make_shared<ChannelEngineMock>();
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_REMOVE");

    static CRGB leds[8];
    fl::fill_solid(leds, 8, CRGB::Blue);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_REMOVE";

    fl::ChannelConfig config(12, timing, fl::span<CRGB>(leds, 8), GRB, options);
    auto channel = fl::Channel::create(config);

    FL_REQUIRE(channel != nullptr);

    // Initial state: not in list
    FL_CHECK(!channel->isInList());

    // Add to list
    FastLED.add(channel);
    FL_CHECK(channel->isInList());

    // Remove from list
    FastLED.remove(channel);
    FL_CHECK(!channel->isInList());

    // Verify channel object is still valid (not destroyed)
    FL_CHECK(channel->size() == 8);
    FL_CHECK(channel->getPin() == 12);
    FL_CHECK(channel->getChannelEngine() == mockEngine.get());

    // Can re-add if needed
    FastLED.add(channel);
    FL_CHECK(channel->isInList());

    // Remove again
    FastLED.remove(channel);
    FL_CHECK(!channel->isInList());

    // Safe to call remove multiple times
    FastLED.remove(channel);
    FastLED.remove(channel);
    FL_CHECK(!channel->isInList());

    // Clean up
    manager.setDriverEnabled("MOCK_REMOVE", false);
}

FL_TEST_CASE("Channel API: Internal ChannelPtr storage prevents dangling") {
    // Verify that CFastLED stores ChannelPtrs internally so channels
    // survive even if the caller drops their reference.
    auto mockEngine = fl::make_shared<ChannelEngineMock>();
    mockEngine->reset();

    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_STORAGE");

    static CRGB leds[4];
    fl::fill_solid(leds, 4, CRGB::White);

    auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
    fl::ChannelOptions options;
    options.mAffinity = "MOCK_STORAGE";

    fl::ChannelConfig config(7, timing, fl::span<CRGB>(leds, 4), GRB, options);
    auto channel = fl::Channel::create(config);
    FL_REQUIRE(channel != nullptr);

    // After add, CFastLED holds an internal reference
    FastLED.add(channel);
    FL_CHECK(channel->isInList());
    FL_CHECK(channel.use_count() >= 2);  // caller + CFastLED internal

    // Drop local reference - channel should survive via CFastLED's storage
    fl::Channel* raw = channel.get();
    channel.reset();

    // Channel should still be in the linked list (not destroyed)
    bool found = false;
    CLEDController* pCur = CLEDController::head();
    while (pCur) {
        if (pCur == raw) {
            found = true;
            break;
        }
        pCur = pCur->next();
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
    FL_CHECK(!channel2->isInList());
    FL_CHECK(channel2.use_count() == 1);  // only local ref remains

    // Clean up the first channel that's still in the list
    // Walk list and remove the raw pointer's entry
    pCur = CLEDController::head();
    while (pCur) {
        if (pCur == raw) {
            static_cast<fl::Channel*>(pCur)->removeFromDrawList();
            break;
        }
        pCur = pCur->next();
    }
    manager.setDriverEnabled("MOCK_STORAGE", false);
}

FL_TEST_CASE("Legacy API: 4 parallel strips using FastLED.addLeds<>()") {
    // This test validates that the legacy FastLED.addLeds<>() API works with channel engines:
    // - Use template-based FastLED.addLeds<WS2812, PIN>() (no explicit channel creation)
    // - Set different colors on each strip
    // - Call FastLED.show()
    // - Verify engine received all 4 strips with correct data

    auto mockEngine = fl::make_shared<ChannelEngineMock>();
    mockEngine->reset();

    // Register mock engine with high priority
    fl::ChannelBusManager& manager = fl::ChannelBusManager::instance();
    manager.addEngine(1000, mockEngine, "MOCK_LEGACY");

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
    const char* getName() const override { return "STUB_ADD_REMOVE"; }
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

static bool controllerInList(CLEDController* ctrl) {
    CLEDController* cur = CLEDController::head();
    while (cur) {
        if (cur == ctrl) return true;
        cur = cur->next();
    }
    return false;
}

FL_TEST_CASE("FastLED.add stores ChannelPtr - survives caller scope") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2000, engine, "STUB_ADD_REMOVE");

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
    mgr.addEngine(2001, engine, "STUB_ADD_REMOVE");

    CRGB leds[4];
    auto ch = makeChannel(leds, 4);

    FastLED.add(ch);
    FL_CHECK(ch.use_count() == 2); // ch + internal

    FastLED.add(ch); // double add - should be no-op
    FL_CHECK(ch.use_count() == 2); // still only 2, not 3

    // Controller should appear exactly once in the linked list
    int count = 0;
    CLEDController* cur = CLEDController::head();
    while (cur) {
        if (cur == ch.get()) count++;
        cur = cur->next();
    }
    FL_CHECK(count == 1);

    // Clean up
    FastLED.remove(ch);
    mgr.setDriverEnabled("STUB_ADD_REMOVE", false);
}

FL_TEST_CASE("FastLED.remove double-remove is safe") {
    auto engine = fl::make_shared<StubEngine>();
    ChannelBusManager& mgr = ChannelBusManager::instance();
    mgr.addEngine(2002, engine, "STUB_ADD_REMOVE");

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
