/// @file channel_manager.cpp
/// @brief Channel manager implementation

#include "channel_manager.h"
#include "channel.h"
#include "channel_data.h"
#include "channel_config.h"
#include "channel_engine.h"
#include "fl/map.h"
#include "fl/vector.h"
#include "fl/singleton.h"

namespace fl {

ChannelManager* ChannelManager::getInstance() {
    static ChannelManager instance;
    return &instance;
}

ChannelManager::ChannelManager() {
}

ChannelManager::~ChannelManager() {
}

ChannelPtr ChannelManager::createInternal(const ChannelConfig& config, IChannelEngine* engine) {
    ChannelPtr channel = Channel::create(config, engine);
    channel->setChannelEngine(engine);
    channel->setChannelManager(this);
    return channel;
}

void ChannelManager::enqueueForDraw(IChannelEngine* engine, ChannelDataPtr channelData) {
    FASTLED_ASSERT(engine, "Engine cannot be null");
    FASTLED_ASSERT(channelData, "ChannelData cannot be null");
    mPendingChannels[engine].push_back(channelData);
}

void ChannelManager::show() {
    // Iterate through each engine and its associated channel data
    for (auto& pair : mPendingChannels) {
        IChannelEngine* engine = pair.first;
        auto& channelDataList = pair.second;

        // Skip if no channel data for this engine
        if (channelDataList.empty()) {
            continue;
        }

        // Create a span from the channel data vector and call beginTransmission
        fl::span<ChannelDataPtr> channelDataSpan(channelDataList.data(), channelDataList.size());
        engine->beginTransmission(channelDataSpan);
    }

    mPendingChannels.clear();
}

}  // namespace fl
