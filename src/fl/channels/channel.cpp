/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_manager.h"
#include "fl/atomic.h"

namespace fl {

int32_t Channel::nextId() {
    static fl::atomic<int32_t> gNextChannelId(0);
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig& config, IChannelEngine* engine) {
    return fl::make_shared<Channel>(config, engine);
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
    CLEDController::setCorrection(correction);
    return *this;
}

Channel& Channel::setTemperature(CRGB temperature) {
    CLEDController::setTemperature(temperature);
    return *this;
}

Channel& Channel::setDither(fl::u8 ditherMode) {
    CLEDController::setDither(ditherMode);
    return *this;
}

Channel& Channel::setRgbw(const Rgbw& rgbw) {
    CLEDController::setRgbw(rgbw);
    return *this;
}

void Channel::dispose() {
    // Remove this channel from the CLEDController linked list
    CLEDController::removeFromList(this);
}

void Channel::showColor(const CRGB& data, int nLeds, fl::u8 brightness) {
    // TODO: Implement showColor
    (void)data;
    (void)nLeds;
    (void)brightness;
}

void Channel::show(const CRGB* data, int nLeds, fl::u8 brightness) {
    // TODO: Implement show
    (void)data;
    (void)nLeds;
    (void)brightness;
}

void Channel::init() {
    // TODO: Implement initialization
}

}  // namespace fl
