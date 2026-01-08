/// @file channel_engine_uart.cpp
/// @brief UART implementation of ChannelEngine for ESP32-C3/S3
///
/// This implementation uses ESP32's UART peripheral to drive LED strips on
/// single GPIO pins. It follows the same architecture as PARLIO channel engine
/// but optimized for single-lane operation (no transposition needed).
///
/// Key differences from PARLIO:
/// - Single-lane: UART transmits on one pin only
/// - No transposition: Data is already in serial format
/// - Simpler ISR: Polling-based completion via waitTxDone()
/// - 2-bit encoding: More efficient than full wave8 (4:1 vs 8:1 expansion)

#include "channel_engine_uart.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/delay.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/charconv.h"
#include "fl/trace.h"
#include "fl/warn.h"

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

ChannelEngineUART::ChannelEngineUART(fl::shared_ptr<IUartPeripheral> peripheral)
    : mPeripheral(fl::move(peripheral)),
      mInitialized(false),
      mCurrentGroupIndex(0) {
    if (!mPeripheral) {
        FL_WARN("UART: Null peripheral pointer in constructor");
    }
}

ChannelEngineUART::~ChannelEngineUART() {
    // Wait for any active transmissions to complete
    while (poll() == EngineState::BUSY || poll() == EngineState::DRAINING) {
        fl::delayMicroseconds(100);
    }

    // Deinitialize peripheral
    if (mPeripheral && mPeripheral->isInitialized()) {
        mPeripheral->deinitialize();
    }

    // Clear state
    mScratchBuffer.clear();
    mEncodedBuffer.clear();
    mEnqueuedChannels.clear();
    mTransmittingChannels.clear();
    mChipsetGroups.clear();
    mCurrentGroupIndex = 0;
}

//=============================================================================
// Public Interface - IChannelEngine Implementation
//=============================================================================

void ChannelEngineUART::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEngineUART::show() {
    FL_SCOPED_TRACE;
    FL_DBG("UART: show() called with " << mEnqueuedChannels.size() << " enqueued channel(s)");

    if (!mEnqueuedChannels.empty()) {
        // Move enqueued channels to transmitting channels
        mTransmittingChannels = fl::move(mEnqueuedChannels);
        mEnqueuedChannels.clear();

        // Group channels by chipset timing configuration
        mChipsetGroups.clear();
        mCurrentGroupIndex = 0;

        for (size_t i = 0; i < mTransmittingChannels.size(); i++) {
            const ChannelDataPtr& channel = mTransmittingChannels[i];
            const ChipsetTimingConfig& timing = channel->getTiming();

            // Find existing group with matching timing
            bool found = false;
            for (size_t j = 0; j < mChipsetGroups.size(); j++) {
                // Compare timing configurations
                if (mChipsetGroups[j].mTiming == timing) {
                    mChipsetGroups[j].mChannels.push_back(channel);
                    found = true;
                    break;
                }
            }

            // Create new group if no match found
            if (!found) {
                ChipsetGroup newGroup{timing, {}};
                newGroup.mChannels.push_back(channel);
                mChipsetGroups.push_back(fl::move(newGroup));
            }
        }

        // Sort groups by transmission time (fastest first)
        fl::sort(mChipsetGroups.begin(), mChipsetGroups.end(),
                 [](const ChipsetGroup& a, const ChipsetGroup& b) {
                     // Find max channel size in group a
                     size_t maxSizeA = 0;
                     for (const auto& channel : a.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeA) {
                             maxSizeA = size;
                         }
                     }

                     // Find max channel size in group b
                     size_t maxSizeB = 0;
                     for (const auto& channel : b.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeB) {
                             maxSizeB = size;
                         }
                     }

                     // Calculate transmission time: LED count * bit period
                     uint64_t transmissionTimeA =
                         static_cast<uint64_t>(maxSizeA) *
                         a.mTiming.total_period_ns();
                     uint64_t transmissionTimeB =
                         static_cast<uint64_t>(maxSizeB) *
                         b.mTiming.total_period_ns();

                     return transmissionTimeA < transmissionTimeB;
                 });

        // UART is single-lane: flatten multi-channel groups into sequential transmissions
        // Create a flat list of individual channels to transmit one-by-one
        fl::vector<ChannelDataPtr> flatChannelList;
        for (const auto& group : mChipsetGroups) {
            for (const auto& channel : group.mChannels) {
                flatChannelList.push_back(channel);
            }
        }

        // Replace chipset groups with single-channel "groups" for sequential transmission
        mChipsetGroups.clear();
        for (const auto& channel : flatChannelList) {
            ChipsetGroup singleChannelGroup{channel->getTiming(), {}};
            singleChannelGroup.mChannels.push_back(channel);
            mChipsetGroups.push_back(fl::move(singleChannelGroup));
        }

        // Begin transmission of first channel
        if (!mChipsetGroups.empty()) {
            beginTransmission(fl::span<const ChannelDataPtr>(
                mChipsetGroups[0].mChannels.data(),
                mChipsetGroups[0].mChannels.size()));
        }
    }
}

IChannelEngine::EngineState ChannelEngineUART::poll() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized) {
        return EngineState::READY;
    }

    // Poll UART peripheral state
    bool busy = mPeripheral->isBusy();

    if (!busy) {
        // Current group completed - check if more groups need transmission
        if (!mChipsetGroups.empty() &&
            mCurrentGroupIndex < mChipsetGroups.size() - 1) {
            // More groups remaining - start next group
            mCurrentGroupIndex++;

            // Check if this is the last group
            bool is_last_group =
                (mCurrentGroupIndex == mChipsetGroups.size() - 1);

            beginTransmission(fl::span<const ChannelDataPtr>(
                mChipsetGroups[mCurrentGroupIndex].mChannels.data(),
                mChipsetGroups[mCurrentGroupIndex].mChannels.size()));

            // If this is the last group, return DRAINING immediately (async)
            // Otherwise return READY and wait for next poll
            return is_last_group ? EngineState::DRAINING : EngineState::READY;
        }

        // All groups completed - clear state and return READY
        mTransmittingChannels.clear();
        mChipsetGroups.clear();
        mCurrentGroupIndex = 0;
        return EngineState::READY;
    }

    // Transmission in progress - return DRAINING
    return EngineState::DRAINING;
}

//=============================================================================
// Private Methods - Transmission
//=============================================================================

void ChannelEngineUART::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {

    FL_DBG("UART: beginTransmission() called with " << channelData.size() << " channel(s)");

    // Validate channel data first
    if (channelData.size() == 0) {
        FL_DBG("UART: No channels to transmit (size==0)");
        return;
    }

    // UART is single-lane only - show() guarantees single channel per transmission
    if (channelData.size() != 1) {
        FL_WARN("UART: Expected exactly 1 channel, got " << channelData.size() << " (internal error)");
        return;
    }

    // Extract channel data
    const ChannelDataPtr& channel = channelData[0];
    int pin = channel->getPin();
    // Note: Timing is embedded in channel, used for future baud rate calculation
    // const ChipsetTimingConfig& timing = channel->getTiming();
    size_t dataSize = channel->getSize();

    FL_DBG("UART: Channel pin=" << pin << ", dataSize=" << dataSize);

    // Validate LED data
    if (dataSize == 0) {
        return;
    }

    // Initialize UART peripheral if needed
    if (!mInitialized) {
        FL_DBG("UART: Initializing peripheral (first time)");
        // Calculate baud rate from LED timing
        // For WS2812: 3.2 Mbps achieves correct timing with 2-bit LUT encoding
        // TODO: Make baud rate configurable based on timing requirements
        uint32_t baud_rate = 3200000; // 3.2 Mbps

        UartConfig config(
            baud_rate,         // mBaudRate
            pin,               // mTxPin
            -1,                // mRxPin (not used)
            4096,              // mTxBufferSize (4 KB for DMA)
            256,               // mRxBufferSize (minimum required by ESP-IDF)
            1,                 // mStopBits (8N1)
            1                  // mUartNum (UART1)
        );

        FL_DBG("UART: Calling peripheral->initialize() with baud=" << baud_rate << ", pin=" << pin);
        if (!mPeripheral->initialize(config)) {
            FL_WARN("UART: Peripheral initialization failed");
            return;
        }

        FL_DBG("UART: Peripheral initialized successfully");
        mInitialized = true;
    }

    // Prepare scratch buffer (copy LED RGB data)
    FL_DBG("UART: Preparing scratch buffer (dataSize=" << dataSize << ")");
    prepareScratchBuffer(channelData, dataSize);

    // TX-side logging: Show first 3 LED bytes (first LED pixel)
    if (dataSize >= 3) {
        FL_DBG("UART TX: Pre-encoding LED bytes (GRB order):");
        FL_DBG("  Byte[0] (G) = 0x" << fl::to_hex(mScratchBuffer[0], false, true));
        FL_DBG("  Byte[1] (R) = 0x" << fl::to_hex(mScratchBuffer[1], false, true));
        FL_DBG("  Byte[2] (B) = 0x" << fl::to_hex(mScratchBuffer[2], false, true));
    }

    // Encode LED data to UART bytes using wave8 encoding
    size_t required_encoded_size = calculateUartBufferSize(dataSize);
    FL_DBG("UART: Required encoded size=" << required_encoded_size << " bytes");
    mEncodedBuffer.resize(required_encoded_size);

    size_t encoded_bytes = encodeLedsToUart(
        mScratchBuffer.data(),
        dataSize,
        mEncodedBuffer.data(),
        mEncodedBuffer.size());

    FL_DBG("UART: Encoded " << encoded_bytes << " bytes from " << dataSize << " LED bytes");

    // TX-side logging: Show first 12 UART frames (first LED pixel)
    if (encoded_bytes >= 12) {
        FL_DBG("UART TX: First LED encoded frames:");
        for (size_t i = 0; i < 12 && i < encoded_bytes; i++) {
            FL_DBG("  Frame[" << i << "] = 0x"
                   << fl::to_hex(mEncodedBuffer[i], false, true));
        }
    }

    if (encoded_bytes == 0) {
        FL_WARN("UART: Encoding failed (required="
                << required_encoded_size << " bytes)");
        return;
    }

    // Submit encoded data to UART peripheral
    FL_DBG("UART: Writing " << encoded_bytes << " bytes to peripheral");
    if (!mPeripheral->writeBytes(mEncodedBuffer.data(), encoded_bytes)) {
        FL_WARN("UART: Write failed (size=" << encoded_bytes << " bytes)");
        return;
    }

    FL_DBG("UART: Write successful, transmission started (non-blocking DMA)");
    // Non-blocking: UART DMA will handle transmission
    // Poll isBusy() to check completion status
}

void ChannelEngineUART::prepareScratchBuffer(
    fl::span<const ChannelDataPtr> channelData,
    size_t maxChannelSize) {

    // Resize scratch buffer
    mScratchBuffer.resize(maxChannelSize);

    // Copy LED RGB data from first channel (single-lane)
    const auto& srcData = channelData[0]->getData();
    fl::memcpy(mScratchBuffer.data(), srcData.data(), maxChannelSize);
}

//=============================================================================
// Factory Function
//=============================================================================

fl::shared_ptr<IChannelEngine> createUartEngine(int uart_num,
                                                int tx_pin,
                                                uint32_t baud_rate) {
    // Note: Factory function implementation requires UartPeripheralEsp
    // This will be implemented when UartPeripheralEsp is available
    // For now, return nullptr
    (void)uart_num;
    (void)tx_pin;
    (void)baud_rate;
    return nullptr;
}

} // namespace fl
