/// @file channel_driver_uart.h
/// @brief UART implementation of ChannelEngine for ESP32
///
/// This file implements a ChannelEngine that uses ESP32's UART peripheral to
/// drive clockless LED strips on single GPIO pins. Uses the wave10 encoding
/// model that dynamically generates LUTs from chipset timing parameters.
///
/// ## Wave10 Encoding
///
/// The wave10 model treats the 10-bit UART frame (START=H + 8 data + STOP=L
/// with TX inversion) as encoding 2 LED bits with 5 pulses each. The LUT is
/// computed from ChipsetTimingConfig, enabling support for any clockless
/// protocol whose timing is representable at the UART's resolution.
///
/// ## Timing Feasibility
///
/// The driver's canHandle() validates that a chipset's timing can be accurately
/// encoded at a baud rate ≤ 5.0 Mbps with quantization tolerance. Chipsets with
/// periods too short (e.g., 1600kHz TM1829) are rejected.
///
/// ## Multi-Timing Support
///
/// Multiple LED strips with different chipset timings can be driven
/// sequentially. The UART peripheral is reinitialized when the baud rate
/// changes between chipset groups.

#pragma once

// IWYU pragma: private

#include "fl/compiler_control.h"

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/led_timing.h"
#include "fl/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "platforms/esp/32/drivers/uart/wave8_encoder_uart.h"

namespace fl {

/// @brief UART-based channel driver for single-lane LED control
///
/// Implements IChannelDriver interface using ESP32 UART peripheral for
/// LED data transmission. Uses wave10 encoding with dynamic LUT generation.
class ChannelEngineUART : public IChannelDriver {
public:
    explicit ChannelEngineUART(fl::shared_ptr<IUartPeripheral> peripheral);
    ~ChannelEngineUART() override;

    bool canHandle(const ChannelDataPtr& data) const override;

    void enqueue(ChannelDataPtr channelData) override;
    void show() override;
    DriverState poll() override;

    fl::string getName() const override { return fl::string::from_literal("UART"); }

    Capabilities getCapabilities() const override {
        return Capabilities(true, false);  // Clockless only
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                              size_t maxChannelSize);

    /// @brief Get or create a Wave10Lut for the given timing
    /// @param timing Chipset timing configuration
    /// @return Reference to cached Wave10Lut
    const Wave10Lut& getOrBuildLut(const ChipsetTimingConfig& timing);

private:
    struct ChipsetGroup {
        ChipsetTimingConfig mTiming;
        fl::vector<ChannelDataPtr> mChannels;
    };

    /// @brief Cached LUT entry (timing → LUT mapping)
    struct LutCacheEntry {
        ChipsetTimingConfig mTiming;
        Wave10Lut mLut;
        u32 mBaudRate;
    };

    fl::shared_ptr<IUartPeripheral> mPeripheral;
    bool mInitialized;
    u32 mCurrentBaudRate; ///< Currently configured baud rate

    fl::vector<u8> mScratchBuffer;
    fl::vector<u8> mEncodedBuffer;

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    fl::vector<ChipsetGroup> mChipsetGroups;
    size_t mCurrentGroupIndex;

    /// @brief LUT cache for multi-timing support
    fl::vector<LutCacheEntry> mLutCache;
};

/// @brief Factory function to create UART driver with real hardware peripheral
fl::shared_ptr<IChannelDriver> createUartEngine(int uart_num,
                                                int tx_pin,
                                                u32 baud_rate = 4000000);

} // namespace fl
