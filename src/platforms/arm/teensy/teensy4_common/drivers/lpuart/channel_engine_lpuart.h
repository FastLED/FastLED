/// @file channel_engine_lpuart.h
/// @brief LPUART-based channel engine for Teensy 4.x
///
/// Implements IChannelDriver via the LPUART wave8 encoding (see
/// lpuart_encoder.h). One strip per LPUARTn instance; pin filtering
/// matches kLpuartPins[] in lpuart_driver.cpp.hpp.

#pragma once

// IWYU pragma: private

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

class ILPUARTPeripheral;
class ILPUARTInstance;

/// @brief LPUART-based channel engine for Teensy 4.x
class ChannelEngineLPUART : public IChannelDriver {
public:
    ChannelEngineLPUART() FL_NO_EXCEPT;
    explicit ChannelEngineLPUART(fl::shared_ptr<ILPUARTPeripheral> peripheral) FL_NO_EXCEPT;
    ~ChannelEngineLPUART() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("LPUART");
    }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);
    }

private:
    fl::shared_ptr<ILPUARTPeripheral> mPeripheral;
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
    fl::unique_ptr<ILPUARTInstance> mInstance;
    bool mHwInitialized;
    u8 mCurrentPin;
    u32 mCurrentRawBytes = 0;
    ChipsetTimingConfig mCurrentTiming;
};

} // namespace fl
