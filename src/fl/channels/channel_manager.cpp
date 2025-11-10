/// @file channel_manager.cpp
/// @brief Channel manager implementation

#include "channel_manager.h"
#include "channel.h"
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

void ChannelManager::onBeginShow(ChannelPtr channel) {
    IChannelEngine* engine = channel->getChannelEngine();
    FASTLED_ASSERT(engine, "Channel has no engine");
    mPendingChannels[engine].push_back(channel);
}

void ChannelManager::show() {
    // Iterate through each engine and its associated channels
    for (auto& pair : mPendingChannels) {
        IChannelEngine* engine = pair.first;
        auto& channels = pair.second;

        // Skip if no channels for this engine
        if (channels.empty()) {
            continue;
        }

        // Create a span from the channels vector and call beginTransmission
        fl::span<ChannelPtr> channelSpan(channels.data(), channels.size());
        engine->beginTransmission(channelSpan);
    }

    mPendingChannels.clear();
}

}  // namespace fl
