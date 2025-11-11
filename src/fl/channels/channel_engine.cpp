/// @file channel_engine.cpp
/// @brief Channel engine implementation

#include "channel_engine.h"
#include "channel_data.h"

namespace fl {

void ChannelEngine::enqueue(ChannelDataPtr channelData) {
    mPendingChannels.push_back(channelData);
}

void ChannelEngine::show() {
    if (mPendingChannels.empty()) {
        return;
    }
    FL_ASSERT(mTransmittingChannels.empty(), "ChannelEngine: Cannot show() while channels are still transmitting");
    FL_ASSERT(poll() == EngineState::READY, "ChannelEngine: Cannot show() while hardware is busy");

    // Mark all channels as in use before transmission
    for (auto& channel : mPendingChannels) {
        channel->setInUse(true);
    }

    // Create a const span from the pending channels and pass to beginTransmission
    fl::span<const ChannelDataPtr> channelSpan(mPendingChannels.data(), mPendingChannels.size());
    beginTransmission(channelSpan);

    // Move pending channels to transmitting list (async operation started)
    // Don't clear mInUse flags yet - poll() will do that when transmission completes
    mTransmittingChannels = fl::move(mPendingChannels);
}

ChannelEngine::EngineState ChannelEngine::poll() {
    // Call derived class implementation to get hardware status
    EngineState state = pollDerived();

    // When transmission completes (READY or ERROR), clear the "in use" flags
    // and release the transmitted channels
    if (state == EngineState::READY || state == EngineState::ERROR) {
        for (auto& channel : mTransmittingChannels) {
            channel->setInUse(false);
        }
        mTransmittingChannels.clear();
    }

    return state;
}

} // namespace fl
