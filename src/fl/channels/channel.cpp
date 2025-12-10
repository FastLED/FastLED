/// @file channel.cpp
/// @brief LED channel implementation

#include "channel.h"
#include "channel_config.h"
#include "channel_data.h"
#include "channel_engine.h"
#include "ftl/atomic.h"
#include "fl/dbg.h"
#include "fl/led_settings.h"
#include "fl/pixel_iterator_any.h"
#include "pixel_controller.h"

#ifdef ESP32
FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
FL_EXTERN_C_END
#endif

namespace fl {


int32_t Channel::nextId() {
    static fl::atomic<int32_t> gNextChannelId(0);
    return gNextChannelId.fetch_add(1);
}

ChannelPtr Channel::create(const ChannelConfig &config, IChannelEngine* engine) {
    return fl::make_shared<Channel>(config.pin, config.timing, config.mLeds,
                                     config.rgb_order, engine, config.settings);
}

Channel::Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                 EOrder rgbOrder, IChannelEngine* engine, const LEDSettings& settings)
    : CPixelLEDController<RGB>()
    , mPin(pin)
    , mTiming(timing)
    , mRgbOrder(rgbOrder)
    , mEngine(engine)
    , mId(nextId()) {
#ifdef ESP32
    // ESP32: Initialize GPIO with pulldown to ensure stable LOW state
    // This prevents RX from capturing noise/glitches on uninitialized pins
    // Must happen before any engine initialization
    gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_PULLDOWN_ONLY);
#endif

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
    if (mEngine) {
        mEngine->enqueue(mChannelData);
    }
}

void Channel::init() {
    // TODO: Implement initialization
}

// Stub engine - provides no-op implementation for testing or unsupported platforms
namespace {
class StubChannelEngine : public IChannelEngine {
public:
    virtual ~StubChannelEngine() = default;

    virtual void enqueue(ChannelDataPtr /*channelData*/) override {
        // No-op: stub engine does nothing
        static bool warned = false;
        if (!warned) {
            FL_DBG("StubChannelEngine: No-op enqueue (use for testing or unsupported platforms)");
            warned = true;
        }
    }

    virtual void show() override {
        // No-op: no hardware to drive
    }

    virtual EngineState poll() override {
        return EngineState::READY;  // Always "ready" (does nothing)
    }
};

} // anonymous namespace

IChannelEngine* getStubChannelEngine() {
    static StubChannelEngine instance;
    return &instance;
}

} // namespace fl
