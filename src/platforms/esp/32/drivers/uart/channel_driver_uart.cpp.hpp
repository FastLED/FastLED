// IWYU pragma: private

/// @file channel_driver_uart.cpp
/// @brief UART implementation of ChannelEngine for ESP32
///
/// Uses wave10 encoding with dynamic LUT generation from chipset timing.
/// Supports multi-timing by reinitializing the UART peripheral when the
/// baud rate changes between chipset groups.

#include "platforms/esp/32/drivers/uart/channel_driver_uart.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/system/delay.h"
#include "fl/error.h"
#include "fl/stl/async.h"
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
      mCurrentBaudRate(0),
      mCurrentGroupIndex(0) {
    if (!mPeripheral) {
        FL_WARN("UART: Null peripheral pointer in constructor");
    }
}

ChannelEngineUART::~ChannelEngineUART() {
    // Wait for any active transmissions to complete
    while (poll() == DriverState::BUSY || poll() == DriverState::DRAINING) {
        async_run(250, AsyncFlags::SYSTEM);
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
    mLutCache.clear();
    mCurrentGroupIndex = 0;
}

bool ChannelEngineUART::canHandle(const ChannelDataPtr& data) const {
    if (!data) {
        return false;
    }
    // Reject SPI chipsets
    if (data->isSpi()) {
        return false;
    }
    // Validate that the chipset's timing is representable via UART wave10
    const ChipsetTimingConfig& timing = data->getTiming();
    return canRepresentTiming(timing);
}

//=============================================================================
// Public Interface - IChannelDriver Implementation
//=============================================================================

void ChannelEngineUART::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelEngineUART::show() {
    FL_SCOPED_TRACE;

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
                     size_t maxSizeA = 0;
                     for (const auto& channel : a.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeA) {
                             maxSizeA = size;
                         }
                     }

                     size_t maxSizeB = 0;
                     for (const auto& channel : b.mChannels) {
                         size_t size = channel->getSize();
                         if (size > maxSizeB) {
                             maxSizeB = size;
                         }
                     }

                     u64 transmissionTimeA =
                         static_cast<u64>(maxSizeA) *
                         a.mTiming.total_period_ns();
                     u64 transmissionTimeB =
                         static_cast<u64>(maxSizeB) *
                         b.mTiming.total_period_ns();

                     return transmissionTimeA < transmissionTimeB;
                 });

        // UART is single-lane: flatten multi-channel groups into sequential transmissions
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

IChannelDriver::DriverState ChannelEngineUART::poll() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized) {
        return DriverState::READY;
    }

    // Poll UART peripheral state
    bool busy = mPeripheral->isBusy();

    if (!busy) {
        // Current group completed - check if more groups need transmission
        if (!mChipsetGroups.empty() &&
            mCurrentGroupIndex < mChipsetGroups.size() - 1) {
            // More groups remaining - start next group
            mCurrentGroupIndex++;

            bool is_last_group =
                (mCurrentGroupIndex == mChipsetGroups.size() - 1);

            beginTransmission(fl::span<const ChannelDataPtr>(
                mChipsetGroups[mCurrentGroupIndex].mChannels.data(),
                mChipsetGroups[mCurrentGroupIndex].mChannels.size()));

            return is_last_group ? DriverState::DRAINING : DriverState::READY;
        }

        // All groups completed - clear state and return READY
        mTransmittingChannels.clear();
        mChipsetGroups.clear();
        mCurrentGroupIndex = 0;
        return DriverState::READY;
    }

    // Transmission in progress - return DRAINING
    return DriverState::DRAINING;
}

//=============================================================================
// Private Methods - LUT Cache
//=============================================================================

const Wave10Lut& ChannelEngineUART::getOrBuildLut(const ChipsetTimingConfig& timing) {
    // Search cache
    for (const auto& entry : mLutCache) {
        if (entry.mTiming == timing) {
            return entry.mLut;
        }
    }

    // Build and cache new LUT
    LutCacheEntry newEntry;
    newEntry.mTiming = timing;
    newEntry.mLut = buildWave10Lut(timing);
    newEntry.mBaudRate = Wave10Lut::computeBaudRate(timing);
    mLutCache.push_back(newEntry);
    return mLutCache.back().mLut;
}

//=============================================================================
// Private Methods - Transmission
//=============================================================================

void ChannelEngineUART::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {

    FL_DBG("UART: beginTransmission() called with " << channelData.size() << " channel(s)");

    if (channelData.size() == 0) {
        FL_DBG("UART: No channels to transmit (size==0)");
        return;
    }

    // UART is single-lane only - show() guarantees single channel per transmission
    if (channelData.size() != 1) {
        FL_WARN("UART: Expected exactly 1 channel, got " << channelData.size() << " (internal error)");
        return;
    }

    const ChannelDataPtr& channel = channelData[0];
    int pin = channel->getPin();
    const ChipsetTimingConfig& timing = channel->getTiming();
    size_t dataSize = channel->getSize();

    FL_DBG("UART: Channel pin=" << pin << ", dataSize=" << dataSize);

    if (dataSize == 0) {
        return;
    }

    // Compute required baud rate from timing
    u32 required_baud = Wave10Lut::computeBaudRate(timing);

    // Initialize or reinitialize UART peripheral if needed
    if (!mInitialized || mCurrentBaudRate != required_baud) {
        if (mInitialized) {
            // Reinitialize with new baud rate
            FL_DBG("UART: Reinitializing peripheral (baud change: " << mCurrentBaudRate << " -> " << required_baud << ")");
            mPeripheral->deinitialize();
            mInitialized = false;
        }

        FL_DBG("UART: Initializing peripheral with baud=" << required_baud << ", pin=" << pin);

        UartPeripheralConfig config(
            required_baud,     // mBaudRate (derived from timing)
            pin,               // mTxPin
            -1,                // mRxPin (not used)
            4096,              // mTxBufferSize (4 KB for DMA)
            256,               // mRxBufferSize (minimum required by ESP-IDF)
            1,                 // mStopBits (8N1)
            1                  // mUartNum (UART1)
        );

        if (!mPeripheral->initialize(config)) {
            FL_WARN("UART: Peripheral initialization failed");
            return;
        }

        FL_DBG("UART: Peripheral initialized successfully");
        mInitialized = true;
        mCurrentBaudRate = required_baud;
    }

    // Get or build the Wave10 LUT for this timing
    const Wave10Lut& lut = getOrBuildLut(timing);

    // Prepare scratch buffer (copy LED RGB data)
    prepareScratchBuffer(channelData, dataSize);

    // Encode LED data to UART bytes using wave10 encoding
    size_t required_encoded_size = calculateUartBufferSize(dataSize);
    mEncodedBuffer.resize(required_encoded_size);

    size_t encoded_bytes = encodeLedsToUart(
        mScratchBuffer.data(),
        dataSize,
        mEncodedBuffer.data(),
        mEncodedBuffer.size(),
        lut);

    FL_DBG("UART: Encoded " << encoded_bytes << " bytes from " << dataSize << " LED bytes");

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
}

void ChannelEngineUART::prepareScratchBuffer(
    fl::span<const ChannelDataPtr> channelData,
    size_t maxChannelSize) {

    mScratchBuffer.resize(maxChannelSize);

    const auto& srcData = channelData[0]->getData();
    fl::memcpy(mScratchBuffer.data(), srcData.data(), maxChannelSize);
}

//=============================================================================
// Factory Function
//=============================================================================

fl::shared_ptr<IChannelDriver> createUartEngine(int uart_num,
                                                int tx_pin,
                                                u32 baud_rate) {
    (void)uart_num;
    (void)tx_pin;
    (void)baud_rate;
    return nullptr;
}

} // namespace fl
