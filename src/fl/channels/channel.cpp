/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_data.h"
#include "channel_engine.h"
#include "fl/atomic.h"
#include "fl/dbg.h"
#include "fl/led_settings.h"
#include "fl/pixel_iterator_any.h"
#include "pixel_controller.h"

namespace fl {


int32_t Channel::nextId() {
    static fl::atomic<int32_t> gNextChannelId(0);
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig &config, ChannelEngine *engine) {
    return fl::make_shared<Channel>(config.pin, config.timing, config.mLeds,
                                     config.rgb_order, engine, config.settings);
}

Channel::Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                 EOrder rgbOrder, ChannelEngine* engine, const LEDSettings& settings)
    : CPixelLEDController<RGB>()
    , mPin(pin)
    , mTiming(timing)
    , mRgbOrder(rgbOrder)
    , mEngine(engine)
    , mId(nextId()) {
    // Set the LED data array
    setLeds(leds);

    // Set color correction/temperature/dither/rgbw from LEDSettings
    setCorrection(settings.correction);
    setTemperature(settings.temperature);
    setDither(settings.ditherMode);
    setRgbw(settings.rgbw);

    // Create ChannelData during construction
    mChannelData = ChannelData::create(mPin, mTiming);
}

Channel::~Channel() {}



void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    // BIG TODO: CHANNEL NEEDS AN ENCODER:
    // Convert pixels to channel data using the configured color order and RGBW settings

    // Safety check: don't modify buffer if engine is currently transmitting it
    if (mChannelData->isInUse()) {
        FL_ASSERT(false, "Channel " << mId << ": Skipping update - buffer in use by engine");
        return;
    }

    // Encode pixels into the channel data
    PixelIteratorAny any(pixels, mRgbOrder, mSettings.rgbw);
    PixelIterator& pixelIterator = any;
    // FUTURE WORK: This is where we put the encoder
    auto& data = mChannelData->getData();
    data.clear();
    pixelIterator.writeWS2812(&data);

    // Enqueue for transmission (will be sent when engine->show() is called)
    mEngine->enqueue(mChannelData);
}

void Channel::init() {
    // TODO: Implement initialization
}

} // namespace fl
