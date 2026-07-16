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

#include "fl/stl/compiler_control.h"

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/chipsets/led_timing.h"
#include "fl/system/engine_events.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "fl/channels/uart_wave_encoder.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief UART-based channel driver for single-lane LED control
///
/// Implements IChannelDriver interface using ESP32 UART peripheral for
/// LED data transmission. Uses wave10 encoding with dynamic LUT generation.
class ChannelEngineUART : public IChannelDriver {
public:
    /// @param uart_num Which UART block this engine drives (FastLED#3576
    ///        Phase 2): 1 = primary lane ("UART"), 2 = second lane
    ///        ("UART2"). UART0 is the console and is never used.
    explicit ChannelEngineUART(fl::shared_ptr<IUartPeripheral> peripheral,
                               int uart_num = 1) FL_NO_EXCEPT;
    ~ChannelEngineUART() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;

    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override {
        // Distinct names per lane so affinity binding and the
        // AutoResearch exclusive-driver selector can target each block.
        return (mUartNum == 2) ? fl::string::from_literal("UART2")
                               : fl::string::from_literal("UART");
    }

    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, false);  // Clockless only
    }

private:
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) FL_NO_EXCEPT;

    void prepareScratchBuffer(fl::span<const ChannelDataPtr> channelData,
                              size_t maxChannelSize) FL_NO_EXCEPT;

    /// @brief Get or create a Wave10Lut for the given timing
    /// @param timing Chipset timing configuration
    /// @return Reference to cached Wave10Lut
    const Wave10Lut& getOrBuildLut(const ChipsetTimingConfig& timing) FL_NO_EXCEPT;

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
    u8 mCurrentDataBits;  ///< Currently configured UART word length (wave geometry)
    int mUartNum;         ///< Which UART block this engine drives (FastLED#3576 Phase 2)

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
                                                u32 baud_rate = 4000000) FL_NO_EXCEPT;

} // namespace fl
