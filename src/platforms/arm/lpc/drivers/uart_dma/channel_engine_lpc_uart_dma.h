#pragma once

// IWYU pragma: private

/// @file channel_engine_lpc_uart_dma.h
/// @brief LPC845 UART + DMA0 clockless channel engine.

#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_UART_DMA)

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "platforms/arm/lpc/uart_arm_lpc_dma.h"

namespace fl {

class ChannelEngineLpcUartDma : public IChannelDriver {
public:
    ChannelEngineLpcUartDma() FL_NO_EXCEPT = default;
    ~ChannelEngineLpcUartDma() override = default;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("UART");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(/*clockless=*/true, /*spi=*/false);
    }

    void setPollNeededCallback(PollNeededCallback callback) FL_NO_EXCEPT override {
        mPollNeededCallback.set(callback);
    }

private:
    bool startNextTransmission() FL_NO_EXCEPT;
    bool beginTransmission(const ChannelDataPtr& channel) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
    fl::vector<u8> mEncodedBuffer;
    size_t mCurrentChannel = 0;
    bool mTransmissionActive = false;
    PollNeededCallbackSlot mPollNeededCallback;
};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA
