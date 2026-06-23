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

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/stl/string.h"
#include "platforms/stub/stub_gpio.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace stub {

/// @brief Stub channel driver that drives SimEdgeObserver notifications
///
/// Registered with higher priority than the generic no-op StubChannelDriver
/// so that Channel::showPixels() routes here when on the stub platform.
class ClocklessChannelEngineStub : public IChannelDriver {
public:
    virtual ~ClocklessChannelEngineStub() = default;

    virtual bool canHandle(const ChannelDataPtr& data) const FL_NOEXCEPT override {
        // Only handle clockless (WS2812-style) channels
        return data && data->isClockless();
    }

    virtual void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override {
        if (!channelData || channelData->getData().empty()) return;
        if (!channelData->isClockless()) return;

        // Simulate WS2812 GPIO output — fires SimEdgeObserver callbacks so
        // any registered NativeRxDevice captures the edges.
        const fl::ChipsetTimingConfig& timing = channelData->getTiming();
        const auto& data = channelData->getData();
        fl::stub::simulateWS2812Output(
            channelData->getPin(),
            data,
            timing
        );
    }

    virtual void show() FL_NOEXCEPT override {
        // No hardware to drive — transmission is synchronous in enqueue()
    }

    virtual DriverState poll() FL_NOEXCEPT override {
        return DriverState(DriverState::READY);
    }

    virtual fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("STUB");
    }

    virtual Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(true, false);  // Clockless only
    }
};

}  // namespace stub
}  // namespace fl
