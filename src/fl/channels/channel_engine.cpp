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

    // Create a const span from the pending channels and pass to beginTransmission
    fl::span<const ChannelDataPtr> channelSpan(mPendingChannels.data(), mPendingChannels.size());
    beginTransmission(channelSpan);

    // Clear the queue after transmission
    mPendingChannels.clear();
}

} // namespace fl
