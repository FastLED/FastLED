#pragma once

#include "FastLED.h"
#include "LegacyClocklessProxy.h"
#include "fl/channels/manager.h"
#include "fl/stl/json.h"
#include "fl/stl/vector.h"
#include "fl/stl/chrono.h"
#include "fl/channels/channel_events.h"
#include "fl/system/engine_events.h"
#include <Arduino.h> // digitalRead observes the actual output pad.

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h" // ok platform headers; ok platform specific - RMT4 worker stack has no generic driver diagnostic API.
#endif

namespace autoresearch {

inline fl::json legacyPixelAllocationFailure() {
    fl::json result = fl::json::object();
    result.set("success", false);
    result.set("error", "Cannot allocate caller pixels for hardware fixture");
    return result;
}

/// AutoResearch registers a ChannelManager for its selectable driver tests.
/// Remove only that listener to reproduce a legacy-only sketch's frame path.
struct LegacyAsyncManagerScope {
    fl::ChannelManager &manager;
    bool restore;
    explicit LegacyAsyncManagerScope(bool legacyOnly)
        : manager(fl::ChannelManager::registry()),
          restore(legacyOnly && fl::EngineEvents::hasListener(&manager)) {
        if (restore) { fl::EngineEvents::removeListener(&manager); }
    }
    ~LegacyAsyncManagerScope() {
        if (restore) { fl::EngineEvents::addListener(&manager); }
    }
};

inline unsigned sampleLegacyAsyncPad(int pin, fl::u32 durationUs) {
    unsigned samples = 0;
    const fl::u32 started = fl::micros();
    while (fl::micros() - started < durationUs) {
        samples += digitalRead(pin) ? 1u : 0u;
    }
    return samples;
}

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
// These probes inspect the existing RMT4 bridge's real retained frame. Other
// backends do not expose that ownership observation and must not fake it.
inline bool legacyFrameMatches(const fl::weak_ptr<fl::ChannelData> &observer,
                               int count, bool white) {
    auto frame = observer.lock();
    if (!frame || frame->getSize() != static_cast<size_t>(count) * 3) {
        return false;
    }
    const auto &bytes = frame->getData();
    for (size_t i = 0; i < bytes.size(); ++i) {
        const fl::u8 expected = white || i % 3 == 2 ? 255 : 0;
        if (bytes[i] != expected) { return false; }
    }
    return true;
}

inline bool legacyFrameLeased(const fl::weak_ptr<fl::ChannelData> &observer) {
    auto frame = observer.lock();
    return frame && frame->isInUse();
}

inline fl::json legacyAsyncControllerProbe(const fl::string &scenario, int pin,
        int count, const fl::shared_ptr<fl::IChannelDriver> &driver) {
    fl::json result = fl::json::object();
    fl::vector<CRGB> pixels(static_cast<size_t>(count), CRGB::White);
    if (pixels.size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
    fl::weak_ptr<fl::ChannelData> observer;
    bool disabledSkipped = true;
    bool firstLease = false;
    bool firstSnapshot = false;
    bool secondSnapshot = true;
    bool retainedAfterRemoval = false;
    fl::u32 secondShowUs = 0;
    {
        LegacyClocklessProxy proxy(pin, pixels.data(), count);
        if (!proxy.valid()) {
            result.set("success", false);
            result.set("error", "Unsupported legacy pin");
            return result;
        }
        observer = proxy.frameData();
        if (scenario == "enable") {
            // Prime the pad to a known idle-low output before testing disable.
            FastLED.show();
            FastLED.wait();
            proxy.setEnabled(false);
            FastLED.show();
            disabledSkipped = !legacyFrameLeased(observer) &&
                sampleLegacyAsyncPad(pin, 2000) == 0;
            proxy.setEnabled(true);
        }
        if (scenario == "standalone") {
            proxy.showLeds();
        } else {
            FastLED.show();
        }
        firstLease = legacyFrameLeased(observer);
        fill_solid(pixels.data(), count, CRGB::Blue);
        firstSnapshot = legacyFrameMatches(observer, count, true);
        if (scenario == "backpressure") {
            // Change both the caller buffer and length while the old encoded
            // snapshot is leased, then immediately reuse the same controller.
            fl::vector<CRGB> replacement(static_cast<size_t>(count + 1), CRGB::Blue);
            if (replacement.size() != static_cast<size_t>(count + 1)) { return legacyPixelAllocationFailure(); }
            proxy.setLeds(fl::span<CRGB>(replacement));
            pixels.clear();
            const fl::u32 started = fl::micros();
            FastLED.show();
            secondShowUs = fl::micros() - started;
            secondSnapshot = legacyFrameMatches(observer, count + 1, false) &&
                legacyFrameLeased(observer);
        }
    } // Controller and (for backpressure) replacement source are destroyed.
    pixels.clear();
    pixels.shrink_to_fit();
    retainedAfterRemoval = legacyFrameLeased(observer);
    const unsigned samples = sampleLegacyAsyncPad(pin, 250000);
    const bool autonomous = observer.expired() &&
        driver->poll().state == fl::IChannelDriver::DriverState::READY;
    const bool waited = FastLED.wait(1000);
    result.set("scenario", scenario);
    result.set("disabledControllerSkipped", disabledSkipped);
    result.set("firstFrameLeasedAtReturn", firstLease);
    result.set("firstSnapshotSurvivedSourceMutation", firstSnapshot);
    result.set("reconfiguredSnapshotCorrect", secondSnapshot);
    result.set("secondShowDurationUs", static_cast<int>(secondShowUs));
    result.set("retainedAfterControllerRemoval", retainedAfterRemoval);
    result.set("padHighSamplesAfterRemoval", static_cast<int>(samples));
    result.set("completedWithoutApplicationPolling", autonomous);
    result.set("waitSucceeded", waited);
    result.set("success", disabledSkipped && firstLease && firstSnapshot &&
        secondSnapshot && retainedAfterRemoval && samples > 0 && autonomous && waited);
    return result;
}

inline fl::json legacyAsyncReplacementProbe(int pin, int count,
        const fl::shared_ptr<fl::IChannelDriver> &driver) {
    fl::json result = fl::json::object();
    fl::weak_ptr<fl::ChannelData> oldFrame;
    fl::weak_ptr<fl::ChannelData> nextFrame;
    fl::u32 resetUs = 0;
    {
        fl::vector<CRGB> oldPixels(static_cast<size_t>(count), CRGB::White);
        if (oldPixels.size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
        LegacyClocklessProxy oldController(pin, oldPixels.data(), count);
        oldFrame = oldController.frameData();
        FastLED.show();
        auto snapshot = oldFrame.lock();
        if (snapshot) { resetUs = snapshot->getTiming().reset_us; }
    }
    bool bothLeased = false;
    bool snapshots = false;
    {
        fl::vector<CRGB> nextPixels(static_cast<size_t>(count), CRGB::Blue);
        if (nextPixels.size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
        LegacyClocklessProxy nextController(pin, nextPixels.data(), count);
        nextFrame = nextController.frameData();
        FastLED.show();
        bothLeased = legacyFrameLeased(oldFrame) && legacyFrameLeased(nextFrame);
        snapshots = legacyFrameMatches(oldFrame, count, true) &&
                    legacyFrameMatches(nextFrame, count, false);
    }
    unsigned oldHigh = 0;
    unsigned nextHigh = 0;
    fl::u32 lastOldHigh = 0;
    fl::u32 firstNextHigh = 0;
    bool oldReleased = false;
    const fl::u32 started = fl::micros();
    while (fl::micros() - started < static_cast<fl::u32>(count) * 60 + 100000) {
        const fl::u32 elapsed = fl::micros() - started;
        oldReleased |= oldFrame.expired();
        if (digitalRead(pin)) {
            if (oldReleased) {
                if (!nextHigh) { firstNextHigh = elapsed; }
                ++nextHigh;
            } else {
                ++oldHigh;
                lastOldHigh = elapsed;
            }
        }
    }
    const bool released = oldFrame.expired() && nextFrame.expired();
    const bool autonomous = driver->poll().state == fl::IChannelDriver::DriverState::READY;
    const bool waited = FastLED.wait(1000);
    const fl::u32 observedGap = nextHigh && oldHigh ? firstNextHigh - lastOldHigh : 0;
    result.set("scenario", "replacement");
    result.set("samePinFramesLeasedTogether", bothLeased);
    result.set("bothSnapshotsCorrectAfterSourceRemoval", snapshots);
    result.set("oldFramePadHighSamples", static_cast<int>(oldHigh));
    result.set("replacementPadHighSamples", static_cast<int>(nextHigh));
    result.set("sampledGapUs", static_cast<int>(observedGap));
    result.set("requiredResetUs", static_cast<int>(resetUs));
    result.set("sampledGapMeetsReset", observedGap >= resetUs);
    result.set("releasedWithoutApplicationPolling", released);
    result.set("completedWithoutApplicationPolling", autonomous);
    result.set("waitSucceeded", waited);
    // Pad polling is a forward-progress observation, not waveform decoding.
    result.set("success", bothLeased && snapshots && oldHigh && nextHigh &&
        observedGap >= resetUs && released && autonomous && waited);
    return result;
}

inline fl::json legacyAsyncMixedProbe(int pin, int count,
        const fl::shared_ptr<fl::IChannelDriver> &driver) {
    fl::json result = fl::json::object();
    const int modernPin = pin == 5 ? 4 : 5;
    fl::vector<CRGB> legacyPixels(static_cast<size_t>(count), CRGB::White);
    fl::vector<CRGB> modernPixels(static_cast<size_t>(count), CRGB::White);
    if (legacyPixels.size() != static_cast<size_t>(count) ||
        modernPixels.size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
    fl::ChannelOptions options;
    options.mBus = fl::Bus::RMT;
    const fl::ChannelConfig config(modernPin,
        fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>(),
        fl::span<CRGB>(modernPixels), RGB, options);
    auto modern = FastLED.add(config);
    const bool modernCreated = modern != nullptr;
    bool modernEncoded = false;
    const int event = FastLED.channelEvents().onChannelDataEncoded.add(
        [&](const fl::IChannel &, const fl::ChannelData &data) {
            if (data.getPin() != modernPin) { return; }
            modernEncoded = data.getSize() == static_cast<size_t>(count) * 3;
            for (size_t i = 0; modernEncoded && i < data.getSize(); ++i) {
                modernEncoded = data.getData()[i] == 255;
            }
        });
    fl::weak_ptr<fl::ChannelData> legacyFrame;
    bool legacyLeased = false;
    {
        LegacyClocklessProxy legacy(pin, legacyPixels.data(), count);
        legacyFrame = legacy.frameData();
        FastLED.show();
        legacyLeased = legacyFrameLeased(legacyFrame) &&
            legacyFrameMatches(legacyFrame, count, true);
        fill_solid(legacyPixels.data(), count, CRGB::Blue);
        fill_solid(modernPixels.data(), count, CRGB::Blue);
    }
    FastLED.channelEvents().onChannelDataEncoded.remove(event);
    unsigned legacyHigh = 0;
    unsigned modernHigh = 0;
    const fl::u32 started = fl::micros();
    while (fl::micros() - started < 250000) {
        legacyHigh += digitalRead(pin) ? 1u : 0u;
        modernHigh += digitalRead(modernPin) ? 1u : 0u;
    }
    const bool autonomous = driver->poll().state == fl::IChannelDriver::DriverState::READY;
    const bool waited = FastLED.wait(1000);
    FastLED.remove(modern);
    modern.reset();
    result.set("scenario", "mixed");
    result.set("modernPin", modernPin);
    result.set("modernChannelCreated", modernCreated);
    result.set("modernEncodedSnapshotCorrect", modernEncoded);
    result.set("legacyFrameLeasedAtReturn", legacyLeased);
    result.set("legacyPadHighSamples", static_cast<int>(legacyHigh));
    result.set("modernPadHighSamples", static_cast<int>(modernHigh));
    result.set("legacyLeaseReleased", legacyFrame.expired());
    result.set("completedWithoutApplicationPolling", autonomous);
    result.set("waitSucceeded", waited);
    result.set("success", modernCreated && modernEncoded && legacyLeased && legacyHigh && modernHigh &&
        legacyFrame.expired() && autonomous && waited);
    return result;
}
#endif

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
inline void setLegacyTxDiagnostics(fl::json &result,
                                  const fl::Rmt4TxDiagnostics &diagnostics) {
    result.set("translatedBytes", static_cast<int>(diagnostics.translatedBytes));
    result.set("cumulativeTranslatedBytes", static_cast<int64_t>(diagnostics.cumulativeTranslatedBytes));
    result.set("latchChecks", static_cast<int>(diagnostics.latchChecks));
    result.set("latchViolations", static_cast<int>(diagnostics.latchViolations));
    result.set("minimumLatchHeldUs", static_cast<int>(diagnostics.minLatchHeldUs));
    result.set("maximumRequiredResetUs", static_cast<int>(diagnostics.maxRequiredResetUs));
    result.set("lateAborts", static_cast<int>(diagnostics.lateAborts));
    result.set("maxRefillDeltaCycles", static_cast<int64_t>(diagnostics.maxDeltaCycles));
    result.set("abortDeltaCycles", static_cast<int64_t>(diagnostics.abortDeltaCycles));
    result.set("allowedDeltaCycles", static_cast<int64_t>(diagnostics.allowedDeltaCycles));
    result.set("preloadCycles", static_cast<int64_t>(diagnostics.preloadCycles));
    result.set("firstIsrCycles", static_cast<int64_t>(diagnostics.firstIsrCycles));
    result.set("postStartCycles", static_cast<int64_t>(diagnostics.postStartCycles));
    result.set("postStartUs", static_cast<int64_t>(diagnostics.postStartUs));
    result.set("firstIsrUs", static_cast<int64_t>(diagnostics.firstIsrUs));
    result.set("maxRefillDeltaUs", static_cast<int>(diagnostics.maxDeltaUs));
    result.set("abortDeltaUs", static_cast<int>(diagnostics.abortDeltaUs));
    result.set("allowedDeltaUs", static_cast<int>(diagnostics.allowedDeltaUs));
    result.set("preloadCore", diagnostics.preloadCore);
    result.set("isrCore", diagnostics.isrCore);
}

inline fl::Rmt4TxDiagnostics legacyTxDiagnostics(int channel = -1) {
    auto &selected = static_cast<fl::detail::Rmt4LegacyDriver &>(fl::BusTraits<fl::Bus::RMT>::instance());
    return selected.txDiagnostics(channel);
}

inline void addLegacyTxDiagnostics(fl::json &result) {
    result.set("translatorDiagnosticsEnabled", FL_RMT4_DIAGNOSTICS != 0);
    setLegacyTxDiagnostics(result, legacyTxDiagnostics());
#if FL_RMT4_DIAGNOSTICS
    fl::json channels = fl::json::array();
    for (int channel = 0; channel < FASTLED_RMT_MAX_CHANNELS; ++channel) {
        fl::json entry = fl::json::object();
        entry.set("channel", channel);
        setLegacyTxDiagnostics(entry, legacyTxDiagnostics(channel));
        channels.push_back(entry);
    }
    result.set("translatorChannels", channels);
#endif
}
#endif

#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
// Five strips exceed the four classic ESP32 TX slots with two blocks each.
// Weak observations do not keep buffers alive after controller destruction.
inline fl::json legacyAsyncOverflowProbe(int count,
                                         fl::shared_ptr<fl::IChannelDriver> driver) {
    const int pins[] = {2, 4, 5, 18, 19};
    fl::weak_ptr<fl::ChannelData> frames[5];
    unsigned highSamples[5] = {};
    unsigned encodedSizes[5] = {};
    unsigned encodedCapacities[5] = {};
    unsigned leasedAtReturn = 0;
    unsigned retainedAfterRemoval = 0;
    bool snapshotsIntact = true;
    bool returnedActive = false;
    const auto before = legacyTxDiagnostics();
    {
        fl::vector<CRGB> pixels[5];
        fl::unique_ptr<LegacyClocklessProxy> proxies[5];
        for (unsigned i = 0; i < 5; ++i) {
            pixels[i].resize(static_cast<size_t>(count));
            if (pixels[i].size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
            fill_solid(pixels[i].data(), count, CRGB::White);
            proxies[i] = fl::make_unique<LegacyClocklessProxy>(
                pins[i], pixels[i].data(), count);
            if (!proxies[i]->valid()) {
                fl::json failure = fl::json::object();
                failure.set("success", false);
                failure.set("error", "Unsupported overflow probe pin");
                return failure;
            }
            frames[i] = proxies[i]->frameData();
        }
        FastLED.show();
        const auto state = driver->poll().state;
        returnedActive = state == fl::IChannelDriver::DriverState::BUSY ||
                         state == fl::IChannelDriver::DriverState::DRAINING;
        for (unsigned i = 0; i < 5; ++i) {
            fill_solid(pixels[i].data(), count, CRGB::Blue);
            auto frame = frames[i].lock();
            if (frame) {
                encodedSizes[i] = frame->getSize();
                encodedCapacities[i] = frame->getData().capacity();
            }
            if (frame && frame->isInUse()) { ++leasedAtReturn; }
            snapshotsIntact &= legacyFrameMatches(frames[i], count, true);
            frame.reset();
            proxies[i].reset();
            auto retained = frames[i].lock();
            if (retained && retained->isInUse()) { ++retainedAfterRemoval; }
        }
    } // Caller pixel arrays and every controller are gone while TX is active.
    const fl::u32 start = fl::micros();
    const fl::u32 observeUs = static_cast<fl::u32>(count) * 60 + 50000;
    while (fl::micros() - start < observeUs) {
        for (unsigned i = 0; i < 5; ++i) {
            highSamples[i] += digitalRead(pins[i]) ? 1u : 0u;
        }
    }
    // No poll/show/wait/yield above: the worker must finish the overflow strip.
    const bool completed = driver->poll().state == fl::IChannelDriver::DriverState::READY;
    unsigned released = 0;
    unsigned observedPins = 0;
    fl::json samples = fl::json::array();
    fl::json sizes = fl::json::array();
    fl::json capacities = fl::json::array();
    for (unsigned i = 0; i < 5; ++i) {
        if (frames[i].expired()) { ++released; }
        if (highSamples[i]) { ++observedPins; }
        samples.push_back(static_cast<int>(highSamples[i]));
        sizes.push_back(static_cast<int>(encodedSizes[i]));
        capacities.push_back(static_cast<int>(encodedCapacities[i]));
    }
    const bool drained = FastLED.wait(1000);
    const auto after = legacyTxDiagnostics();
    const fl::u32 translatedInBatch = after.cumulativeTranslatedBytes - before.cumulativeTranslatedBytes;
    const fl::u32 latchChecks = after.latchChecks - before.latchChecks;
    const fl::u32 latchViolations = after.latchViolations - before.latchViolations;
    fl::json result = fl::json::object();
    result.set("returnedActive", returnedActive);
    result.set("leasedFramesAtReturn", static_cast<int>(leasedAtReturn));
    result.set("retainedFramesAfterControllerRemoval", static_cast<int>(retainedAfterRemoval));
    result.set("encodedSnapshotsUnaffectedBySourceMutation", snapshotsIntact);
    result.set("padHighSamples", samples);
    result.set("encodedFrameSizes", sizes);
    result.set("encodedBufferCapacities", capacities);
    result.set("releasedFramesWithoutApplicationPolling", static_cast<int>(released));
    result.set("completedWithoutApplicationPolling", completed);
    result.set("waitSucceeded", drained);
    result.set("batchTranslatedBytes", static_cast<int>(translatedInBatch));
    result.set("batchLatchChecks", static_cast<int>(latchChecks));
    result.set("batchLatchViolations", static_cast<int>(latchViolations));
    addLegacyTxDiagnostics(result);
    result.set("success", returnedActive && leasedAtReturn == 5 &&
        retainedAfterRemoval == 5 && snapshotsIntact && observedPins == 5 &&
        released == 5 && completed && drained &&
        (!FL_RMT4_DIAGNOSTICS || (translatedInBatch == static_cast<fl::u32>(count) * 15 &&
            latchChecks == 5 && latchViolations == 0)));
    return result;
}
#endif

/// Call after the regular driver test has registered the selected backend.
/// Poll only at the two observation points; application work between them
/// deliberately performs no delay, yield, wait, or driver servicing.
inline fl::json legacyAsyncProbe(const fl::json& args) {
    int pin = 4;
    int count = 2000;
    if (args.is_object()) {
        if (args["pin"].is_int()) { pin = static_cast<int>(args["pin"].as_int().value()); }
        if (args["numLeds"].is_int()) { count = static_cast<int>(args["numLeds"].as_int().value()); }
    }
    fl::json result = fl::json::object();
    if (count < 100 || count > 4096) {
        result.set("success", false);
        result.set("error", "numLeds must be in [100,4096]");
        return result;
    }
    const char* name = legacyClocklessBoundDriverName();
    auto driver = name ? fl::channelManager().findDriverByName(name) : nullptr;
    if (!driver) {
        result.set("success", false);
        result.set("error", "Run the regular driver test first to register the backend");
        return result;
    }
    FastLED.clear(ClearFlags::CHANNELS);
    FastLED.setBrightness(255);
    const fl::string scenario = args.is_object()
        ? args["scenario"].as_string().value_or("") : fl::string();
    const bool legacyOnly = scenario == "replacement" ||
        (args.is_object() && args["legacyOnly"].as_bool().value_or(false));
    if (scenario == "mixed" && legacyOnly) {
        result.set("success", false);
        result.set("error", "Mixed Channel API coverage requires the manager listener");
        return result;
    }
    LegacyAsyncManagerScope managerScope(legacyOnly);
#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
    if (scenario == "standalone" || scenario == "enable" || scenario == "backpressure") {
        if (count < 1000) { count = 1000; }
        result = legacyAsyncControllerProbe(scenario, pin, count, driver);
        result.set("legacyOnly", legacyOnly);
        return result;
    }
    if (scenario == "replacement") {
        if (count < 1000) { count = 1000; }
        result = legacyAsyncReplacementProbe(pin, count, driver);
        result.set("legacyOnly", legacyOnly);
        return result;
    }
    if (scenario == "mixed") {
        if (count < 1000) { count = 1000; }
        return legacyAsyncMixedProbe(pin, count, driver);
    }
    if (args.is_object() && args["overflow"].is_bool() &&
        args["overflow"].as_bool().value()) {
        if (count < 1000) { count = 1000; }
        return legacyAsyncOverflowProbe(count, driver);
    }
#endif
    fl::vector<CRGB> leds;
    leds.resize(static_cast<size_t>(count));
    if (leds.size() != static_cast<size_t>(count)) { return legacyPixelAllocationFailure(); }
    fill_solid(leds.data(), count, CRGB::White);
    LegacyClocklessProxy proxy(pin, leds.data(), count);
    if (!proxy.valid()) {
        result.set("success", false);
        result.set("error", "Unsupported legacy pin");
        return result;
    }
    FastLED.setBrightness(255);
    const fl::u32 showStarted = fl::micros();
    FastLED.show();
    const fl::u32 showDurationUs = fl::micros() - showStarted;
    auto stateAtReturn = driver->poll().state;
    const bool returnedActive = stateAtReturn == fl::IChannelDriver::DriverState::BUSY ||
                                stateAtReturn == fl::IChannelDriver::DriverState::DRAINING;
    size_t encodedBytes = 0;
    int firstEncodedByte = -1;
#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
    {
        auto frame = proxy.frameData().lock();
        if (frame) {
            encodedBytes = frame->getSize();
            if (encodedBytes) { firstEncodedByte = frame->getData()[0]; }
        }
    }
#endif
    // Mutating the caller's pixels must not mutate the encoded frame in flight.
    fill_solid(leds.data(), count, CRGB::Blue);
    unsigned highSamples = 0;
    const fl::u32 start = fl::micros();
    int firstHighDelayUs = -1;
    // Startup may be queued when show returns. Sample through the full drain
    // window without calling any driver service, wait, yield, or delay.
    while (fl::micros() - start < 200000) {
        const fl::u32 elapsed = fl::micros() - start;
        if (digitalRead(pin)) {
            ++highSamples;
            if (firstHighDelayUs < 0) { firstHighDelayUs = static_cast<int>(elapsed); }
        }
    }
    const auto stateAfterIdle = driver->poll().state;
    const bool completedIndependently = stateAfterIdle == fl::IChannelDriver::DriverState::READY;
    const bool drained = FastLED.wait(1000);
    result.set("returnedActive", returnedActive);
    result.set("stateAtReturn", static_cast<int>(stateAtReturn));
    result.set("showDurationUs", static_cast<int>(showDurationUs));
    result.set("encodedBytes", static_cast<int>(encodedBytes));
    result.set("firstEncodedByte", firstEncodedByte);
    result.set("firstHighDelayUs", firstHighDelayUs);
    result.set("padHighSamplesAfterReturn", static_cast<int>(highSamples));
    result.set("completedWithoutApplicationPolling", completedIndependently);
    result.set("waitSucceeded", drained);
#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES
    addLegacyTxDiagnostics(result);
#endif
#if AUTORESEARCH_LEGACY_OBSERVES_RMT4_FRAMES && FL_RMT4_ASYNC // ok platform specific - measure the RMT4 worker's stack without adding a cross-platform virtual API.
    auto& selected = static_cast<fl::Rmt4AsyncDriver&>(fl::BusTraits<fl::Bus::RMT>::instance());
    result.set("workerStackUnusedBytes", static_cast<int>(selected.workerStackHighWaterBytes()));
#endif
    result.set("success", returnedActive && highSamples > 0 && completedIndependently && drained);
    return result;
}

} // namespace autoresearch
