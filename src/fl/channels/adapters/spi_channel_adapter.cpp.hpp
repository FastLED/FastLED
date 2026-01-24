/// @file spi_channel_adapter.cpp
/// @brief Adapter implementation for HW SPI controllers

#include "spi_channel_adapter.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/stl/cstring.h"  // For memcpy
#include "fl/channels/config.h"  // For SpiChipsetConfig
#include "platforms/shared/spi_hw_1.h"  // For SpiHw1::Config // ok platform headers

namespace fl {

// Static factory method (multi-controller version)
fl::shared_ptr<SpiChannelEngineAdapter> SpiChannelEngineAdapter::create(
    fl::vector<fl::shared_ptr<SpiHwBase>> hwControllers,
    fl::vector<int> priorities,
    fl::vector<const char*> names,
    const char* adapterName
) {
    // Validation
    if (hwControllers.empty()) {
        FL_WARN("SpiChannelEngineAdapter::create: No controllers provided");
        return nullptr;
    }

    if (hwControllers.size() != priorities.size() || hwControllers.size() != names.size()) {
        FL_WARN("SpiChannelEngineAdapter::create: Size mismatch in arguments");
        return nullptr;
    }

    if (!adapterName || adapterName[0] == '\0') {
        FL_WARN("SpiChannelEngineAdapter::create: Empty adapter name");
        return nullptr;
    }

    // Create adapter
    auto adapter = fl::make_shared<SpiChannelEngineAdapter>(adapterName);

    // Register all controllers
    for (size_t i = 0; i < hwControllers.size(); i++) {
        if (!hwControllers[i]) {
            FL_WARN("SpiChannelEngineAdapter: Null controller at index " << i);
            continue;
        }

        adapter->mControllers.emplace_back(hwControllers[i], priorities[i], names[i]);

        FL_DBG("SpiChannelEngineAdapter: Registered controller '" << names[i]
               << "' (priority " << priorities[i]
               << ", lanes: " << static_cast<int>(hwControllers[i]->getLaneCount()) << ")");
    }

    if (adapter->mControllers.empty()) {
        FL_WARN("SpiChannelEngineAdapter: No valid controllers registered");
        return nullptr;
    }

    return adapter;
}

SpiChannelEngineAdapter::SpiChannelEngineAdapter(const char* name)
    : mName(name)
{
    // Controllers added via create() factory method
}

SpiChannelEngineAdapter::~SpiChannelEngineAdapter() {
    FL_DBG("SpiChannelEngineAdapter: Destructor for '" << mName << "'");

    // Clear any enqueued channels that were never transmitted
    // This prevents infinite loop in poll() which returns DRAINING if enqueued channels exist
    mEnqueuedChannels.clear();

    // Poll until READY to ensure cleanup of any in-flight transmissions
    while (poll() != EngineState::READY) {
        // Busy-wait for transmission completion
        // This is acceptable in destructor context
    }

    // Shutdown all initialized controllers
    for (auto& ctrl : mControllers) {
        if (ctrl.controller && ctrl.controller->isInitialized()) {
            ctrl.controller->end();
        }
    }
}

int SpiChannelEngineAdapter::getPriority() const {
    int maxPriority = -1;
    for (const auto& ctrl : mControllers) {
        if (ctrl.priority > maxPriority) {
            maxPriority = ctrl.priority;
        }
    }
    return maxPriority;
}

int SpiChannelEngineAdapter::selectControllerForClockPin(int clockPin) {
    // Check if clock pin already assigned
    for (const auto& assignment : mClockPinAssignments) {
        if (assignment.clockPin == clockPin) {
            return assignment.controllerIndex;
        }
    }

    // Find highest priority available controller
    int bestIndex = -1;
    int bestPriority = -1;

    for (size_t i = 0; i < mControllers.size(); i++) {
        if (!canControllerHandleClockPin(mControllers[i], clockPin)) {
            continue;
        }

        if (mControllers[i].priority > bestPriority) {
            bestIndex = i;
            bestPriority = mControllers[i].priority;
        }
    }

    // Record assignment
    if (bestIndex >= 0) {
        mClockPinAssignments.push_back({clockPin, static_cast<size_t>(bestIndex)});
    }

    return bestIndex;
}

bool SpiChannelEngineAdapter::canControllerHandleClockPin(
    const ControllerInfo& ctrl, int clockPin) const {

    // Uninitialized controllers can handle any pin
    if (!ctrl.isInitialized) {
        return true;
    }

    // Check if already handling this pin
    for (int assignedPin : ctrl.assignedClockPins) {
        if (assignedPin == clockPin) {
            return true;
        }
    }

    // ESP32 constraint: cannot re-initialize with different pins
    return false;
}

bool SpiChannelEngineAdapter::initializeControllerIfNeeded(
    ControllerInfo& ctrl, int clockPin, int dataPin) {

    if (ctrl.isInitialized) {
        // Verify compatibility
        for (int pin : ctrl.assignedClockPins) {
            if (pin == clockPin) {
                return true;  // Already configured for this pin
            }
        }
        FL_WARN("SpiChannelEngineAdapter: Controller " << ctrl.name
                << " already initialized with different clock pin");
        return false;
    }

    // Initialize controller based on lane count
    uint8_t lanes = ctrl.controller->getLaneCount();

    if (lanes == 1) {
        SpiHw1::Config config;
        config.clock_pin = clockPin;
        config.data_pin = dataPin;
        config.clock_speed_hz = 20000000;  // 20 MHz for APA102
        config.max_transfer_sz = 65536;

        if (!ctrl.controller->begin(&config)) {
            FL_WARN("SpiChannelEngineAdapter: Failed to initialize " << ctrl.name);
            return false;
        }
    } else {
        // TODO: Multi-lane initialization (SpiHw2/4/8/16)
        FL_WARN("SpiChannelEngineAdapter: Multi-lane init not yet implemented");
        return false;
    }

    ctrl.isInitialized = true;
    ctrl.assignedClockPins.push_back(clockPin);

    FL_DBG("SpiChannelEngineAdapter: Initialized " << ctrl.name
           << " with clock pin " << clockPin);

    return true;
}

bool SpiChannelEngineAdapter::canHandle(const ChannelDataPtr& data) const {
    if (!data) {
        return false;
    }

    // ⚠️ CRITICAL: Accept ONLY true SPI chipsets (APA102, SK9822, HD108)
    // Reject clockless chipsets (WS2812, SK6812) - those use ChannelEngineSpi or RMT
    //
    // This is the opposite of ChannelEngineSpi::canHandle(), which should be:
    //   return !data->isSpi();  // Accept clockless, reject true SPI
    //
    // Correct routing:
    //   APA102 → SpiChannelEngineAdapter (this adapter)
    //   WS2812 → ChannelEngineSpi (clockless-over-SPI) OR RMT/PARLIO
    return data->isSpi();
}

void SpiChannelEngineAdapter::enqueue(ChannelDataPtr channelData) {
    if (!channelData) {
        FL_WARN("SpiChannelEngineAdapter: Null channel data passed to enqueue()");
        return;
    }

    // Validate that we can handle this data
    if (!canHandle(channelData)) {
        FL_WARN("SpiChannelEngineAdapter: Cannot handle non-SPI channel data (chipset mismatch)");
        return;
    }

    mEnqueuedChannels.push_back(channelData);
    FL_DBG("SpiChannelEngineAdapter: Enqueued channel (total: " << mEnqueuedChannels.size() << ")");
}

void SpiChannelEngineAdapter::show() {
    if (mEnqueuedChannels.empty()) {
        FL_DBG("SpiChannelEngineAdapter: show() called with no enqueued channels");
        return;
    }

    FL_DBG("SpiChannelEngineAdapter: show() called with " << mEnqueuedChannels.size() << " channels");

    // Move enqueued channels to transmitting list
    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // Group channels by clock pin
    // Channels with the same clock pin can share SPI bus configuration
    auto groups = groupByClockPin(mTransmittingChannels);

    FL_DBG("SpiChannelEngineAdapter: Grouped into " << groups.size() << " clock pin groups");

    // Transmit each group sequentially
    for (size_t i = 0; i < groups.size(); i++) {
        const ClockPinGroup& group = groups[i];

        FL_DBG("SpiChannelEngineAdapter: Transmitting group with clock pin "
               << group.clockPin << " (" << group.channels.size() << " channels)");

        if (!transmitBatch(group.channels)) {
            FL_WARN("SpiChannelEngineAdapter: Failed to transmit batch for clock pin " << group.clockPin);
            // Continue with other groups rather than aborting entirely
        }
    }

    FL_DBG("SpiChannelEngineAdapter: show() complete");
}

IChannelEngine::EngineState SpiChannelEngineAdapter::poll() {
    // Check if ANY controller is busy
    bool anyBusy = false;
    for (const auto& ctrl : mControllers) {
        if (ctrl.controller && ctrl.controller->isBusy()) {
            anyBusy = true;
            break;
        }
    }

    if (anyBusy) {
        return EngineState::BUSY;
    }

    // All controllers idle - release transmitting channels
    if (!mTransmittingChannels.empty()) {
        FL_DBG("SpiChannelEngineAdapter: Releasing " << mTransmittingChannels.size()
               << " completed channels");

        for (const auto& channel : mTransmittingChannels) {
            if (channel) {
                channel->setInUse(false);
            }
        }
        mTransmittingChannels.clear();
    }

    // Check for draining state
    if (!mEnqueuedChannels.empty()) {
        return EngineState::DRAINING;
    }

    return EngineState::READY;
}

fl::vector<SpiChannelEngineAdapter::ClockPinGroup> SpiChannelEngineAdapter::groupByClockPin(
    fl::span<const ChannelDataPtr> channels
) {
    fl::vector<ClockPinGroup> groups;

    for (const auto& channel : channels) {
        if (!channel) {
            continue;
        }

        // Extract clock pin from chipset configuration
        const auto& chipset = channel->getChipset();
        if (!chipset.is<SpiChipsetConfig>()) {
            FL_WARN("SpiChannelEngineAdapter: Non-SPI chipset in groupByClockPin");
            continue;
        }

        const SpiChipsetConfig& spiConfig = chipset.get<SpiChipsetConfig>();
        int clockPin = spiConfig.clockPin;

        // Find existing group for this clock pin
        ClockPinGroup* existingGroup = nullptr;
        for (size_t i = 0; i < groups.size(); i++) {
            if (groups[i].clockPin == clockPin) {
                existingGroup = &groups[i];
                break;
            }
        }

        // Add to existing group or create new group
        if (existingGroup) {
            existingGroup->channels.push_back(channel);
        } else {
            ClockPinGroup newGroup;
            newGroup.clockPin = clockPin;
            newGroup.channels.push_back(channel);
            groups.push_back(newGroup);
        }
    }

    return groups;
}

bool SpiChannelEngineAdapter::transmitBatch(fl::span<const ChannelDataPtr> channels) {
    if (channels.empty()) {
        return true;
    }

    // Extract clock pin and data pin from first channel (all same in batch)
    const auto& chipset = channels[0]->getChipset();
    if (!chipset.is<SpiChipsetConfig>()) {
        FL_WARN("SpiChannelEngineAdapter: Non-SPI chipset in transmitBatch");
        return false;
    }

    const SpiChipsetConfig& spiConfig = chipset.get<SpiChipsetConfig>();
    int clockPin = spiConfig.clockPin;
    int dataPin = channels[0]->getPin();  // MOSI pin from channel config

    // Select controller for this clock pin
    int controllerIndex = selectControllerForClockPin(clockPin);
    if (controllerIndex < 0) {
        FL_WARN("SpiChannelEngineAdapter: No available controller for clock pin " << clockPin);
        return false;
    }

    ControllerInfo& ctrl = mControllers[controllerIndex];

    // Initialize controller if needed
    if (!initializeControllerIfNeeded(ctrl, clockPin, dataPin)) {
        return false;
    }

    // Transmit using selected controller
    for (const auto& channel : channels) {
        if (!channel) {
            continue;
        }

        // Mark channel as in use
        channel->setInUse(true);

        // Get encoded data
        const auto& data = channel->getData();
        if (data.empty()) {
            FL_WARN("SpiChannelEngineAdapter: Empty channel data");
            channel->setInUse(false);
            continue;
        }

        FL_DBG("SpiChannelEngineAdapter: Transmitting channel via " << ctrl.name
               << " (pin " << channel->getPin() << ", " << data.size() << " bytes)");

        // Acquire DMA buffer
        DMABuffer dmaBuffer = ctrl.controller->acquireDMABuffer(data.size());
        if (!dmaBuffer.ok()) {
            FL_WARN("SpiChannelEngineAdapter: Failed to acquire DMA buffer (error "
                   << static_cast<int>(dmaBuffer.error()) << ")");
            channel->setInUse(false);
            return false;
        }

        // Copy data to DMA buffer
        fl::span<uint8_t> buffer = dmaBuffer.data();
        if (buffer.size() < data.size()) {
            FL_WARN("SpiChannelEngineAdapter: DMA buffer too small ("
                   << buffer.size() << " < " << data.size() << ")");
            channel->setInUse(false);
            return false;
        }

        fl::memcpy(buffer.data(), data.data(), data.size());

        // Transmit (async mode for non-blocking operation)
        if (!ctrl.controller->transmit(TransmitMode::ASYNC)) {
            FL_WARN("SpiChannelEngineAdapter: Transmission failed");
            channel->setInUse(false);
            return false;
        }

        FL_DBG("SpiChannelEngineAdapter: Transmission queued successfully");
    }

    // Wait for transmission to complete (synchronous for now)
    // TODO: Make fully async by returning BUSY state and polling in poll()
    if (!ctrl.controller->waitComplete(1000)) {  // 1 second timeout
        FL_WARN("SpiChannelEngineAdapter: Transmission timeout");
        return false;
    }

    FL_DBG("SpiChannelEngineAdapter: Batch transmission complete");
    return true;
}

} // namespace fl
