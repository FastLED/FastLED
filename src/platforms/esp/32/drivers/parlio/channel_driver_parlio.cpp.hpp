// IWYU pragma: private

/// @file channel_driver_parlio.cpp
/// @brief Parallel IO implementation of ChannelDriver for ESP32-P4/C6/H2/C5
///
/// This implementation uses ESP32's Parallel IO (PARLIO) peripheral to drive
/// multiple LED strips simultaneously on parallel GPIO pins. It supports
/// ESP32-P4, ESP32-C6, ESP32-H2, and ESP32-C5 variants that have PARLIO
/// hardware. Note: ESP32-S3 does NOT have PARLIO (it has LCD peripheral
/// instead).
///
/// This is a runtime-configurable implementation supporting 1-16 channels with
/// power-of-2 data widths (1, 2, 4, 8, 16) determined at construction time.
///
/// This file now contains only channel management logic. All hardware-specific
/// operations have been moved to parlio_engine.{h,cpp}.

#include "fl/stl/compiler_control.h"

#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.h"
#include "fl/channels/config.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/system/delay.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/log.h"
#include "fl/stl/algorithm.h"
#include "fl/trace.h"
#include "fl/stl/assert.h"

    // The test may have 3000 LEDs, but we use streaming buffers for large strips
#ifndef FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL
#define FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL 300
#endif // defined(FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL)

namespace fl {

//=============================================================================
// Constructors / Destructors - Implementation Class
//=============================================================================

ChannelDriverPARLIOImpl::ChannelDriverPARLIOImpl(size_t data_width)
    : mDriver(detail::ParlioEngine::getInstance()),
      mInitialized(false),
      mDataWidth(data_width),
      mCurrentGroupIndex(0),
      mReversedPinOrder(false) {

    // Validate data width
    if (data_width != 1 && data_width != 2 && data_width != 4 &&
        data_width != 8 && data_width != 16) {
        FL_WARN("PARLIO: Invalid data_width="
                << data_width << " (must be 1, 2, 4, 8, or 16)");
        // Constructor will still complete, but initialization will fail
        return;
    }
}

ChannelDriverPARLIOImpl::~ChannelDriverPARLIOImpl() {
    // Wait for any active transmissions to complete
    // Must check for READY (not just !BUSY) since poll() can return DRAINING
    DriverState state = poll();
    u32 start = mDriver.peripheral()->millis();
    while (state.state != DriverState::READY && state.state != DriverState::ERROR) {
        if (mDriver.peripheral()->millis() - start >= 2000) {
            FL_ERROR("PARLIO: Destructor timeout waiting for READY");
            break;
        }
        mDriver.peripheral()->delayMicroseconds(100);
        state = poll();
    }

    // Clear state
    mScratchBuffer.clear();
    mEnqueuedChannels.clear();
    mTransmittingChannels.clear();
    mChipsetGroups.clear();
    mCurrentGroupIndex = 0;
}

bool ChannelDriverPARLIOImpl::canHandle(const ChannelDataPtr& data) const {
    if (!data) {
        return false;
    }
    // Clockless drivers only handle non-SPI chipsets
    return !data->isSpi();
}

// NOTE: Old ISR callback functions (txDoneCallback, transposeAndQueueNextChunk, generateRingBuffer)
// have been removed. These belonged to the old architecture where the channel driver implemented
// its own ISR handling. The new architecture delegates all hardware operations to ParlioEngine HAL.


//=============================================================================
// Public Interface - IChannelDriver Implementation
//=============================================================================

void ChannelDriverPARLIOImpl::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mEnqueuedChannels.push_back(channelData);
    }
}

void ChannelDriverPARLIOImpl::setReversedPinOrder(bool reversed_pin_order) {
    mReversedPinOrder = reversed_pin_order;
}

void ChannelDriverPARLIOImpl::show() {
    FL_SCOPED_TRACE;
    if (!mEnqueuedChannels.empty()) {
        // Wait for previous transmission to complete
        waitForReady();

        // Mark all channels as in use before transmission
        for (auto& channel : mEnqueuedChannels) {
            channel->setInUse(true);
        }

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
        // Transmission time = LED count * (T1 + T2 + T3)
        // This allows faster transmissions to complete first, reducing latency
        // for shorter/faster strips while longer/slower strips are still being prepared
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
                     // Note: Using 64-bit to avoid overflow (max ~4 billion ns)
                     u64 transmissionTimeA = static_cast<u64>(maxSizeA) * a.mTiming.total_period_ns();
                     u64 transmissionTimeB = static_cast<u64>(maxSizeB) * b.mTiming.total_period_ns();

                     // Sort by transmission time ascending (faster groups first)
                     return transmissionTimeA < transmissionTimeB;
                 });

        for (auto& group : mChipsetGroups) {
            // sort each member of the group by their pin orders.
            bool reversed = mReversedPinOrder;
            fl::sort(group.mChannels.begin(), group.mChannels.end(), [reversed](const ChannelDataPtr& a, const ChannelDataPtr& b) {
                if (reversed) {
                    return b->getPin() < a->getPin();
                }
                return a->getPin() < b->getPin();
            });
            bool has_unique_pins = true;
            for (size_t i = 1; i < group.mChannels.size(); i++) {
                if (group.mChannels[i]->getPin() == group.mChannels[i - 1]->getPin()) {
                    has_unique_pins = false;
                    break;
                }
            }
            if (!has_unique_pins) {
                FL_LOG_PARLIO("PARLIO: Channels in group " << group.mTiming.name << " have non-unique pins");
            }
        }

        // Begin transmission of first chipset group
        // Note: show() always starts first group synchronously, then returns
        // The async behavior happens when first group is also the last group
        // (common case: single chipset) - poll() will return DRAINING immediately
        if (!mChipsetGroups.empty()) {
            beginTransmission(fl::span<const ChannelDataPtr>(
                mChipsetGroups[0].mChannels.data(),
                mChipsetGroups[0].mChannels.size()));
        }
    }
}

IChannelDriver::DriverState ChannelDriverPARLIOImpl::poll() {
    // If not initialized, we're ready (no hardware to poll)
    if (!mInitialized) {
        return DriverState::READY;
    }

    // Poll HAL state
    detail::ParlioEngineState halState = mDriver.poll();

    switch (halState) {
        case detail::ParlioEngineState::READY:
            // Current group completed - check if more groups need transmission
            if (!mChipsetGroups.empty() && mCurrentGroupIndex < mChipsetGroups.size() - 1) {
                // More groups remaining - start next group
                mCurrentGroupIndex++;

                // Check if this is the last group
                bool is_last_group = (mCurrentGroupIndex == mChipsetGroups.size() - 1);

                beginTransmission(fl::span<const ChannelDataPtr>(
                    mChipsetGroups[mCurrentGroupIndex].mChannels.data(),
                    mChipsetGroups[mCurrentGroupIndex].mChannels.size()));

                // If this is the last group, return DRAINING immediately (async)
                // Otherwise return READY and wait for next poll
                return is_last_group ? DriverState::DRAINING : DriverState::READY;
            }

            // All groups completed - clear in-use flags and state
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
            mChipsetGroups.clear();
            mCurrentGroupIndex = 0;
            return DriverState::READY;

        case detail::ParlioEngineState::DRAINING:
            // Current group still transmitting - return DRAINING
            return DriverState::DRAINING;

        case detail::ParlioEngineState::BUSY:
            return DriverState::BUSY;

        case detail::ParlioEngineState::ERROR:
            // Clear in-use flags on error
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
            mChipsetGroups.clear();
            mCurrentGroupIndex = 0;
            return DriverState::ERROR;

        default:
            return DriverState::ERROR;
    }
}

//=============================================================================
// Private Methods - Transmission
//=============================================================================

void ChannelDriverPARLIOImpl::beginTransmission(
    fl::span<const ChannelDataPtr> channelData) {

    // Validate channel data first (before initialization)
    if (channelData.size() == 0) {
        return;
    }

    // Validate channel count is within bounds
    size_t channel_count = channelData.size();
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many channels (got " << channel_count
                                                  << ", max 16)");
        return;
    }

    // Validate channel count matches data_width constraints
    size_t required_width = selectDataWidth(channel_count);
    if (required_width != mDataWidth) {
        FL_WARN("PARLIO: Channel count "
                << channel_count << " requires data_width=" << required_width
                << " but this instance is data_width=" << mDataWidth);
        return;
    }

    // Extract and validate GPIO pins from channel data
    fl::vector<int> pins;
    for (size_t i = 0; i < channel_count; i++) {
        int pin = channelData[i]->getPin();
        pins.push_back(pin);
    }

    // Fill remaining pins with -1 for dummy lanes (unused)
    for (size_t i = channel_count; i < mDataWidth; i++) {
        pins.push_back(-1);
    }

    // Extract timing configuration from first channel
    const ChipsetTimingConfig &timing = channelData[0]->getTiming();

    // Validate LED data and find maximum channel size
    size_t maxChannelSize = 0;
    bool hasData = false;

    for (size_t i = 0; i < channelData.size(); i++) {
        if (!channelData[i]) {
            FL_WARN("PARLIO: Null channel data at index " << i);
            return;
        }

        size_t dataSize = channelData[i]->getSize();

        if (dataSize > 0) {
            hasData = true;
            if (dataSize > maxChannelSize) {
                maxChannelSize = dataSize;
            }
        }
    }

    if (!hasData || maxChannelSize == 0) {
        return;
    }

    // Calculate max LEDs for buffer sizing (cap at 300 to avoid excessive memory usage)

    size_t actualLeds = maxChannelSize / 3;
    size_t maxLeds = (actualLeds < FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL) ? actualLeds : FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL;

    // Initialize HAL if needed
    if (!mDriver.initialize(mDataWidth, pins, timing, maxLeds)) {
        FL_WARN("PARLIO: HAL initialization failed (data_width=" << mDataWidth
                << ", channels=" << channel_count << ", maxLeds=" << maxLeds
                << "). Try reducing FL_ESP_PARLIO_MAX_LEDS_PER_CHANNEL or "
                << "FASTLED_PARLIO_MAX_RING_BUFFER_TOTAL_BYTES");
        return;
    }

    mInitialized = true;

    // Prepare scratch buffer (per-lane layout)
    prepareScratchBuffer(channelData, maxChannelSize);

    // Calculate total buffer size (all lanes)
    size_t totalBufferSize = channelData.size() * maxChannelSize;

    // Delegate to HAL (blocking call)
    if (!mDriver.beginTransmission(
            mScratchBuffer.data(),
            totalBufferSize,        // ITERATION 9 FIX: Pass total buffer size, not per-lane size
            channelData.size(),
            maxChannelSize)) {
        FL_WARN("PARLIO: Transmission failed");
    }
}

void ChannelDriverPARLIOImpl::prepareScratchBuffer(
    fl::span<const ChannelDataPtr> channelData,
    size_t maxChannelSize) {

    // Resize scratch buffer (per-lane layout)
    size_t totalSize = channelData.size() * maxChannelSize;
    mScratchBuffer.resize(totalSize);

    // Copy channel data to scratch buffer with right-padding
    for (size_t i = 0; i < channelData.size(); i++) {
        size_t dataSize = channelData[i]->getSize();
        u8* laneDst = mScratchBuffer.data() + (i * maxChannelSize);

        const auto& srcData = channelData[i]->getData();

        // Safety check: Skip empty channels (shouldn't happen in normal operation)
        if (srcData.size() == 0 || dataSize == 0) {
            FL_WARN("PARLIO: Channel " << i << " has empty data (size=" << srcData.size() << "), skipping transmission");
            return;  // Abort transmission - channel not ready
        }

        if (dataSize < maxChannelSize) {
            // Right-pad with zeros
            fl::memcpy(laneDst, srcData.data(), dataSize);
            fl::memset(laneDst + dataSize, 0, maxChannelSize - dataSize);
        } else {
            fl::memcpy(laneDst, srcData.data(), maxChannelSize);
        }
    }
}

//=============================================================================
// Polymorphic Wrapper Class Implementation
//=============================================================================

ChannelDriverPARLIO::ChannelDriverPARLIO()
    : mCurrentDataWidth(0),
      mPhase(TransmitPhase::IDLE),
      mCurrentSpiChannelIndex(0),
      mSpiInitialized(false) {}

ChannelDriverPARLIO::~ChannelDriverPARLIO() {
    mCurrentDataWidth = 0;
}

bool ChannelDriverPARLIO::canHandle(const ChannelDataPtr& data) const {
    if (!data) {
        return false;
    }
    // Accept both clockless and SPI channels
    return true;
}

void ChannelDriverPARLIO::enqueue(ChannelDataPtr channelData) {
    if (channelData) {
        mTransmittingChannels.push_back(channelData);
    }
}

void ChannelDriverPARLIO::show() {
    FL_SCOPED_TRACE;
    if (mTransmittingChannels.empty()) return;

    // Mark all channels as in use
    for (auto& channel : mTransmittingChannels) {
        channel->setInUse(true);
    }

    // Separate clockless and SPI channels
    fl::vector<ChannelDataPtr> clocklessChannels;
    mPendingSpi.clear();

    for (auto& channel : mTransmittingChannels) {
        if (channel->isSpi()) {
            mPendingSpi.push_back(channel);
        } else {
            clocklessChannels.push_back(channel);
        }
    }

    // Begin transmission: clockless first, then SPI
    if (!clocklessChannels.empty()) {
        beginClocklessTransmission(clocklessChannels);
        mPhase = TransmitPhase::CLOCKLESS;
    } else if (!mPendingSpi.empty()) {
        beginSpiTransmission();
        mPhase = TransmitPhase::SPI;
    }
}

IChannelDriver::DriverState ChannelDriverPARLIO::poll() {
    switch (mPhase) {
        case TransmitPhase::IDLE:
            return DriverState::READY;

        case TransmitPhase::CLOCKLESS: {
            if (!mClocklessDriver) {
                mPhase = TransmitPhase::IDLE;
                return DriverState::ERROR;
            }
            DriverState state = mClocklessDriver->poll();
            if (state == DriverState::READY) {
                // Clockless done - transition to SPI if pending
                if (!mPendingSpi.empty()) {
                    beginSpiTransmission();
                    mPhase = TransmitPhase::SPI;
                    return DriverState::DRAINING;
                }
                // All done
                for (auto& channel : mTransmittingChannels) {
                    channel->setInUse(false);
                }
                mTransmittingChannels.clear();
                mPhase = TransmitPhase::IDLE;
                return DriverState::READY;
            }
            return state;
        }

        case TransmitPhase::SPI: {
            detail::ParlioEngineState halState = detail::ParlioEngine::getInstance().poll();

            if (halState == detail::ParlioEngineState::READY) {
                // Current SPI channel completed - check if more pending
                if (mCurrentSpiChannelIndex < mPendingSpi.size() - 1) {
                    mCurrentSpiChannelIndex++;
                    beginSingleSpiChannel(mPendingSpi[mCurrentSpiChannelIndex]);
                    return DriverState::DRAINING;
                }

                // All SPI channels completed
                for (auto& channel : mTransmittingChannels) {
                    channel->setInUse(false);
                }
                mTransmittingChannels.clear();
                mPendingSpi.clear();
                mCurrentSpiChannelIndex = 0;
                mPhase = TransmitPhase::IDLE;
                return DriverState::READY;
            }

            if (halState == detail::ParlioEngineState::DRAINING) {
                return DriverState::DRAINING;
            }
            if (halState == detail::ParlioEngineState::BUSY) {
                return DriverState::BUSY;
            }

            // Error
            for (auto& channel : mTransmittingChannels) {
                channel->setInUse(false);
            }
            mTransmittingChannels.clear();
            mPendingSpi.clear();
            mCurrentSpiChannelIndex = 0;
            mPhase = TransmitPhase::IDLE;
            return DriverState::ERROR;
        }

        default:
            return DriverState::ERROR;
    }
}

void ChannelDriverPARLIO::beginClocklessTransmission(
    fl::span<const ChannelDataPtr> channelData) {
    if (channelData.size() == 0) {
        return;
    }

    size_t channel_count = channelData.size();
    if (channel_count > 16) {
        FL_WARN("PARLIO: Too many clockless channels (got " << channel_count
                                                            << ", max 16)");
        return;
    }

    size_t required_width = selectDataWidth(channel_count);
    if (required_width == 0) {
        FL_WARN("PARLIO: Invalid clockless channel count " << channel_count);
        return;
    }

    // Create or reconfigure clockless driver
    if (!mClocklessDriver || mCurrentDataWidth != required_width) {
        mClocklessDriver = fl::make_unique<ChannelDriverPARLIOImpl>(required_width);
        mCurrentDataWidth = required_width;
    }

    // Enqueue all clockless channels and show
    for (const auto& ch : channelData) {
        mClocklessDriver->enqueue(ch);
    }
    mClocklessDriver->show();
}

void ChannelDriverPARLIO::beginSpiTransmission() {
    if (mPendingSpi.empty()) return;

    mCurrentSpiChannelIndex = 0;
    beginSingleSpiChannel(mPendingSpi[0]);
}

void ChannelDriverPARLIO::beginSingleSpiChannel(const ChannelDataPtr& channelData) {
    if (!channelData || !channelData->isSpi()) return;

    // Extract SPI config from channel data
    const ChipsetVariant& chipset = channelData->getChipset();
    const SpiChipsetConfig* spiConfig = chipset.ptr<SpiChipsetConfig>();
    if (!spiConfig) {
        FL_WARN("PARLIO_SPI: Channel is not SPI type");
        return;
    }

    // Build pin vector: lane 0 = clock, lane 1 = data
    fl::vector<int> pins;
    pins.push_back(spiConfig->clockPin);
    pins.push_back(spiConfig->dataPin);

    // Get encoded SPI bytes
    const auto& encodedData = channelData->getData();
    if (encodedData.empty()) return;

    size_t dataSize = encodedData.size();

    // Initialize engine in SPI mode
    auto& driver = detail::ParlioEngine::getInstance();
    if (!driver.initializeSpi(pins, spiConfig->timing.clock_hz, dataSize)) {
        FL_WARN("PARLIO_SPI: HAL initialization failed");
        return;
    }
    mSpiInitialized = true;

    // Prepare scratch buffer (single lane - raw SPI bytes)
    mSpiScratchBuffer.resize(dataSize);
    fl::memcpy(mSpiScratchBuffer.data(), encodedData.data(), dataSize);

    // Transmit (single lane, laneStride = dataSize)
    if (!driver.beginTransmission(mSpiScratchBuffer.data(), dataSize, 1, dataSize)) {
        FL_WARN("PARLIO_SPI: Transmission failed");
    }
}

} // namespace fl
