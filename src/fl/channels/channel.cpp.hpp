/// @file channel.cpp
/// @brief LED channel implementation

#include "platforms/is_platform.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"
#include "fl/channels/chipset_helpers.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/bus_manager.h"
#include "fl/stl/atomic.h"
#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/channels/options.h"
#include "fl/pixel_iterator_any.h"
#include "pixel_controller.h"
#include "fl/trace.h"

#ifdef FL_IS_ESP32
FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
FL_EXTERN_C_END
#endif

namespace fl {


i32 Channel::nextId() {
    static fl::atomic<i32> gNextChannelId(0); // okay static in header
    return gNextChannelId.fetch_add(1);
}

fl::string Channel::makeName(i32 id, const fl::optional<fl::string>& configName) {
    if (configName.has_value()) {
        return configName.value();
    }
    return "Channel_" + fl::to_string(static_cast<i64>(id));
}

ChannelPtr Channel::create(const ChannelConfig &config) {
    // Create channel without engine binding (engine pointer removed from architecture)
    auto channel = fl::make_shared<Channel>(config.chipset, config.mLeds,
                                              config.rgb_order, config.options);
    channel->mName = makeName(channel->mId, config.mName);

    // Note: Registration with ChannelBusManager happens in CFastLED::add()
    // This ensures proper lifetime management via shared_ptr

    auto& events = ChannelEvents::instance();
    events.onChannelCreated(*channel);
    return channel;
}

Channel::Channel(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                 EOrder rgbOrder, const ChannelOptions& options)
    : CPixelLEDController<RGB>(RegistrationMode::DeferRegister)  // Defer registration until FastLED.add()
    , mChipset(chipset)
    , mPin(getDataPinFromChipset(chipset))
    , mTiming(getTimingFromChipset(chipset))
    , mRgbOrder(rgbOrder)
    , mId(nextId())
    , mName(makeName(mId)) {
#ifdef FL_IS_ESP32
    // ESP32: Initialize GPIO with pulldown to ensure stable LOW state
    // This prevents RX from capturing noise/glitches on uninitialized pins
    // Must happen before any engine initialization
    gpio_set_pull_mode(static_cast<gpio_num_t>(mPin), GPIO_PULLDOWN_ONLY);

    // For SPI chipsets, also initialize the clock pin
    if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        gpio_set_pull_mode(static_cast<gpio_num_t>(spi->clockPin), GPIO_PULLDOWN_ONLY);
    }
#endif

    // Set the LED data array
    setLeds(leds);

    // Set color correction/temperature/dither/rgbw from ChannelOptions
    setCorrection(options.mCorrection);
    setTemperature(options.mTemperature);
    setDither(options.mDitherMode);
    setRgbw(options.mRgbw);

    // Create ChannelData during construction with chipset variant
    mChannelData = ChannelData::create(mChipset);
}

// Backwards-compatible constructor (deprecated)
Channel::Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                 EOrder rgbOrder, const ChannelOptions& options)
    : CPixelLEDController<RGB>(RegistrationMode::DeferRegister)  // Defer registration until FastLED.add()
    , mChipset(ClocklessChipset(pin, timing))  // Convert to variant
    , mPin(pin)
    , mTiming(timing)
    , mRgbOrder(rgbOrder)
    , mId(nextId())
    , mName(makeName(mId)) {
#ifdef FL_IS_ESP32
    // ESP32: Initialize GPIO with pulldown to ensure stable LOW state
    gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_PULLDOWN_ONLY);
#endif

    // Set the LED data array
    setLeds(leds);

    // Set color correction/temperature/dither/rgbw from ChannelOptions
    setCorrection(options.mCorrection);
    setTemperature(options.mTemperature);
    setDither(options.mDitherMode);
    setRgbw(options.mRgbw);

    // Create ChannelData during construction
    mChannelData = ChannelData::create(mPin, mTiming);
}

Channel::~Channel() {
    auto& events = ChannelEvents::instance();
    events.onChannelBeginDestroy(*this);

    // Note: Unregistration from ChannelBusManager now happens in CFastLED::remove()
    // This ensures proper cleanup via shared_ptr
}

void Channel::applyConfig(const ChannelConfig& config) {
    mRgbOrder = config.rgb_order;
    if (config.mName.has_value()) {
        mName = config.mName.value();
    }
    setLeds(config.mLeds);
    setCorrection(config.options.mCorrection);
    setTemperature(config.options.mTemperature);
    setDither(config.options.mDitherMode);
    setRgbw(config.options.mRgbw);
    auto& events = ChannelEvents::instance();
    events.onChannelConfigured(*this, config);
}

int Channel::getClockPin() const {
    if (const SpiChipsetConfig* spi = mChipset.ptr<SpiChipsetConfig>()) {
        return spi->clockPin;
    }
    return -1;  // Clockless chipsets don't have a clock pin
}


ChannelDataPtr Channel::encodePixels(u8 brightness) {
    FL_SCOPED_TRACE;

    // Safety check: don't modify buffer if engine is currently transmitting it
    if (mChannelData->isInUse()) {
        FL_ASSERT(false, "Channel " << mId << ": Skipping update - buffer in use by engine");
        return;
    }

    // Create pixel iterator with color order and RGBW conversion
    // PixelIteratorAny creates a second PixelController via copy constructor (doesn't increment R)
    PixelIteratorAny any(pixels, mRgbOrder, mSettings.mRgbw);

    PixelIterator& pixelIterator = any;

    // Encode pixels based on chipset type
    auto& data = mChannelData->getData();
    data.clear();

    if (mChipset.is<ClocklessChipset>()) {
        // Clockless chipsets: use WS2812 encoding (timing-sensitive byte stream)
        pixelIterator.writeWS2812(&data);
    } else if (mChipset.is<SpiChipsetConfig>()) {
        // SPI chipsets: dispatch based on chipset type (zero allocation)
        const SpiChipsetConfig* spi = mChipset.ptr<SpiChipsetConfig>();
        const SpiEncoder& config = spi->timing;

        // Switch on enum WITHOUT default case - compiler will warn if new enum values are added
        // TODO: Consolidate these PixelIterator methods with template controllers in src/fl/chipsets/
        switch (config.chipset) {
            case SpiChipset::APA102:
            case SpiChipset::DOTSTAR:
            case SpiChipset::HD107:
                pixelIterator.writeAPA102(&data, false);
                break;

            case SpiChipset::APA102HD:
            case SpiChipset::DOTSTARHD:
            case SpiChipset::HD107HD:
                pixelIterator.writeAPA102(&data, true);
                break;

            case SpiChipset::SK9822:
                pixelIterator.writeSK9822(&data, false);
                break;

            case SpiChipset::SK9822HD:
                pixelIterator.writeSK9822(&data, true);
                break;

            case SpiChipset::WS2801:
                pixelIterator.writeWS2801(&data);
                break;

            case SpiChipset::WS2803:
                pixelIterator.writeWS2803(&data);
                break;

            case SpiChipset::P9813:
                pixelIterator.writeP9813(&data);
                break;

            case SpiChipset::LPD8806:
                pixelIterator.writeLPD8806(&data);
                break;

            case SpiChipset::LPD6803:
                pixelIterator.writeLPD6803(&data);
                break;

            case SpiChipset::SM16716:
                pixelIterator.writeSM16716(&data);
                break;

            case SpiChipset::HD108:
                pixelIterator.writeHD108(&data);
                break;
        }
        // No default case - compiler will error if any enum value is missing
    }

    // Return encoded data for enqueuing by caller (ChannelBusManager)
    return mChannelData;
}

void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    FL_SCOPED_TRACE;

    // Architecture change (GitHub #2167): All encoding happens top-down via ChannelBusManager
    // Channel no longer stores engine pointer or handles enqueuing
    // This method is a no-op - encoding is triggered by ChannelBusManager::encodeTrackedChannels()

    (void)pixels;  // Suppress unused parameter warning
}

void Channel::init() {
    // TODO: Implement initialization
}

// Stub engine - provides no-op implementation for testing or unsupported platforms
namespace {
class StubChannelEngine : public IChannelEngine {
public:
    virtual ~StubChannelEngine() = default;

    virtual bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;  // Test engine accepts all channel types
    }

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
        return EngineState(EngineState::READY);  // Always "ready" (does nothing)
    }

    virtual fl::string getName() const override {
        return fl::string::from_literal("STUB");
    }

    virtual Capabilities getCapabilities() const override {
        return Capabilities(true, true);  // Stub accepts both clockless and SPI
    }
};

} // anonymous namespace

IChannelEngine* getStubChannelEngine() {
    static StubChannelEngine instance;
    return &instance;
}

// Re-exposed protected base class methods
int Channel::size() const {
    return CPixelLEDController<RGB>::size();
}

void Channel::showLeds(u8 brightness) {
    CPixelLEDController<RGB>::showLeds(brightness);
}

fl::span<CRGB> Channel::leds() {
    return fl::span<CRGB>(CPixelLEDController<RGB>::leds(), CPixelLEDController<RGB>::size());
}

fl::span<const CRGB> Channel::leds() const {
    return fl::span<const CRGB>(CPixelLEDController<RGB>::leds(), CPixelLEDController<RGB>::size());
}

CRGB Channel::getCorrection() {
    return CPixelLEDController<RGB>::getCorrection();
}

CRGB Channel::getTemperature() {
    return CPixelLEDController<RGB>::getTemperature();
}

u8 Channel::getDither() {
    return CPixelLEDController<RGB>::getDither();
}

void Channel::setDither(u8 ditherMode) {
    CPixelLEDController<RGB>::setDither(ditherMode);
}

Rgbw Channel::getRgbw() const {
    return CPixelLEDController<RGB>::getRgbw();
}

} // namespace fl
