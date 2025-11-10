/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_manager.h"
#include "fl/atomic.h"

namespace fl {

namespace {
fl::atomic<int32_t> gNextChannelId(0);
}

int32_t Channel::nextId() {
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig& config, IChannelEngine* engine) {
    return ChannelPtr(new Channel(config, engine));
}

Channel::Channel(const ChannelConfig& config, IChannelEngine* engine)
    : mConfig(config)
    , mEngine(engine)
    , mManager(NULL)
    , mId(nextId())
{}

Channel::~Channel() {
}

Channel& Channel::setCorrection(CRGB correction) {
    // TODO: Implement color correction
    (void)correction;
    return *this;
}

Channel& Channel::setTemperature(CRGB temperature) {
    // TODO: Implement color temperature
    (void)temperature;
    return *this;
}

Channel& Channel::setDither(fl::u8 ditherMode) {
    // TODO: Implement dithering
    (void)ditherMode;
    return *this;
}

Channel& Channel::setRgbw(const Rgbw& rgbw) {
    // TODO: Implement RGBW conversion
    (void)rgbw;
    return *this;
}

void Channel::dispose() {
    // TODO: Implement disposal - remove from CLEDController linked list
}

}  // namespace fl
