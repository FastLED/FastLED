#pragma once

// IWYU pragma: private

/// @file platforms/stub/clockless_channel_engine_stub.h
/// @brief Stub channel engine that simulates WS2812 GPIO output
///
/// When the new FastLED.add() API is used (Channel objects), show() routes
/// through this engine rather than through ClocklessController. This engine
/// calls fl::stub::simulateWS2812Output() on enqueue(), which fires
/// SimEdgeObserver callbacks. NativeRxDevice registers as an observer in
/// begin() and captures those edges, completing the TX→RX loopback simulation.

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/string.h"
#include "platforms/stub/stub_gpio.h"

namespace fl {
namespace stub {

/// @brief Stub channel engine that drives SimEdgeObserver notifications
///
/// Registered with higher priority than the generic no-op StubChannelEngine
/// so that Channel::showPixels() routes here when on the stub platform.
class ClocklessChannelEngineStub : public IChannelEngine {
public:
    virtual ~ClocklessChannelEngineStub() = default;

    virtual bool canHandle(const ChannelDataPtr& data) const override {
        // Only handle clockless (WS2812-style) channels
        return data && data->isClockless();
    }

    virtual void enqueue(ChannelDataPtr channelData) override {
        if (!channelData || channelData->getData().empty()) return;
        if (!channelData->isClockless()) return;

        // Simulate WS2812 GPIO output — fires SimEdgeObserver callbacks so
        // any registered NativeRxDevice captures the edges.
        const fl::ChipsetTimingConfig& timing = channelData->getTiming();
        const auto& data = channelData->getData();
        fl::stub::simulateWS2812Output(
            channelData->getPin(),
            fl::span<const u8>(data.data(), data.size()),
            timing
        );
    }

    virtual void show() override {
        // No hardware to drive — transmission is synchronous in enqueue()
    }

    virtual EngineState poll() override {
        return EngineState(EngineState::READY);
    }

    virtual fl::string getName() const override {
        return fl::string::from_literal("STUB");
    }

    virtual Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }
};

}  // namespace stub
}  // namespace fl
