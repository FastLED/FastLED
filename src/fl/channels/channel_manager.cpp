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
    return channel;
}

void ChannelManager::enqueueForDraw(IChannelEngine* engine, ChannelDataPtr channelData) {
    FASTLED_ASSERT(engine, "Engine cannot be null");
    FASTLED_ASSERT(channelData, "ChannelData cannot be null");
    // Delegate to the engine's enqueue method
    engine->enqueue(channelData);
}

void ChannelManager::show() {
    // Iterate through each engine and call its show() method
    for (auto& pair : mPendingChannels) {
        IChannelEngine* engine = pair.first;
        // Call the engine's show() method which will handle beginTransmission internally
        engine->show();
    }
    // Note: No need to clear mPendingChannels since engines clear their own queues
    mPendingChannels.clear();
}

}  // namespace fl
