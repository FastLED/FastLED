/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_data.h"
#include "channel_manager.h"
#include "fl/atomic.h"
#include "fl/pixel_iterator_any.h"
#include "pixel_controller.h"

namespace fl {


int32_t Channel::nextId() {
    static fl::atomic<int32_t> gNextChannelId(0);
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig &config,
                           IChannelEngine *engine) {
    return fl::make_shared<Channel>(config, engine);
}

Channel::Channel(const ChannelConfig &config, IChannelEngine *engine)
    : mConfig(config), mEngine(engine), mManager(NULL), mId(nextId()) {}

Channel::~Channel() {}

Channel &Channel::setCorrection(CRGB correction) {
    CLEDController::setCorrection(correction);
    return *this;
}

Channel &Channel::setTemperature(CRGB temperature) {
    CLEDController::setTemperature(temperature);
    return *this;
}

Channel &Channel::setDither(fl::u8 ditherMode) {
    CLEDController::setDither(ditherMode);
    return *this;
}

Channel &Channel::setRgbw(const Rgbw &rgbw) {
    CLEDController::setRgbw(rgbw);
    return *this;
}

void Channel::dispose() {
    // Remove this channel from the CLEDController linked list
    CLEDController::removeFromList(this);
}


/* the problem here is that the CLED controller sub class represents two things

  * read only link to the frame buffer
  * Encoder: RGB8 -> uint8_t[]
  * sw/hw bit banging

Now we are running into the real trifecta of the challenges fastled always had to face
the problem of naming things.

It's hard to name things in classic FastLED speak because what something did was fuzzy.

It's easier to name things now because we are  seperating them into their functional components.
  * Channel
  * Encoder
  * Controller

  */

void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    // BIG TODO: CHANNEL NEEDS AN ENCODER:
    // Convert pixels to channel data using the configured color order and RGBW settings

    // Create ChannelData if not already exists
    if (!mChannelData) {
        mChannelData = ChannelData::create(mConfig.pin, mConfig.timing);
    }

    // Encode pixels into the channel data
    PixelIteratorAny any(pixels, mConfig.rgb_order, mConfig.rgbw);
    PixelIterator& pixelIterator = any;
    // FUTURE WORK: This is where we put the encoder
    pixelIterator.writeWS2812(&mChannelData->getData());

    // Enqueue for transmission
    mManager->enqueueForDraw(mEngine, mChannelData);
}

void Channel::init() {
    // TODO: Implement initialization
}

} // namespace fl
