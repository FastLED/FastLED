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
// IWYU pragma: begin_keep
#include "driver/gpio.h"
// IWYU pragma: end_keep
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
    // Late binding strategy: Always create with empty engine
    // Engine binding happens on first showPixels() call:
    // - Affinity channels: Look up by name and cache
    // - Non-affinity channels: Select dynamically each frame

    auto channel = fl::make_shared<Channel>(config.chipset, config.mLeds,
                                              config.rgb_order, config.options);
    channel->mName = makeName(channel->mId, config.mName);
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
    , mEngine()  // Empty weak_ptr - late binding on first showPixels()
    , mAffinity(options.mAffinity)  // Get affinity from ChannelOptions
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
    , mEngine()  // Empty weak_ptr - late binding on first showPixels()
    , mAffinity(options.mAffinity)  // Get affinity from ChannelOptions
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


void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    FL_SCOPED_TRACE;

    // Safety check: don't modify buffer if engine is currently transmitting it
    if (mChannelData->isInUse()) {
        FL_WARN("Channel '" << mName << "': showPixels() called while mChannelData is in use by engine, attempting to wait");
        auto engine = mEngine.lock();
        if (!engine) {
            FL_ERROR("Channel '" << mName << "': No engine bound yet the mChannelData is in use - cannot transmit");
            return;
        }
        // wait until the engine is in a READY state.
        bool ok = engine->waitForReady();
        if (!ok) {
            FL_ERROR("Channel '" << mName << "': Timeout occurred while waiting for engine to become READY");
            return;
        }
        FL_WARN("Channel '" << mName << "': Engine became READY after waiting");
    }

    auto engine = ChannelBusManager::instance().selectEngineForChannel(mChannelData, mAffinity);
    mEngine = engine;
    if (!engine) {
        FL_ERROR("Channel '" << mName << "': No compatible engine found - cannot transmit");
        return;
    }

    // Create pixel iterator with color order and RGBW conversion
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

    // Fire event after encoding completes
    {
        auto& events = ChannelEvents::instance();
        events.onChannelDataEncoded(*this, *mChannelData);
    }



    // Enqueue for transmission (will be sent when engine->show() is called)
    engine->enqueue(mChannelData);
    auto& events = ChannelEvents::instance();
    events.onChannelEnqueued(*this, engine->getName());
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
void Channel::addToDrawList() {
    if (isInList()) {
        FL_WARN("Channel '" << mName << "': Skipping addToDrawList() - already in draw list");
        return;
    }
    CPixelLEDController<RGB>::addToList();
    // Fire event after adding to draw list (detectable even if user bypasses FastLED.add())
    auto& events = ChannelEvents::instance();
    events.onChannelAdded(*this);
}

void Channel::removeFromDrawList() {
    if (!isInList()) {
        FL_WARN("Channel '" << mName << "': Skipping removeFromDrawList() - not in draw list");
        return;
    }
    CPixelLEDController<RGB>::removeFromDrawList();
    // Fire event after removing from draw list (detectable even if user bypasses FastLED.remove())
    auto& events = ChannelEvents::instance();
    events.onChannelRemoved(*this);

    // Clear engine weak_ptr when removed from draw list
    mEngine.reset();
}

int Channel::size() const {
    return CPixelLEDController<RGB>::size();
}

void Channel::showLeds(u8 brightness) {
    CPixelLEDController<RGB>::showLeds(brightness);
}

bool Channel::isInDrawList() const {
    return CPixelLEDController<RGB>::isInList();
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

Rgbw Channel::getRgbw() const {
    return CPixelLEDController<RGB>::getRgbw();
}

fl::string Channel::getEngineName() const {
    // Lock the weak_ptr to get a shared_ptr
    auto engine = mEngine.lock();
    if (engine) {
        return engine->getName();
    }
    return fl::string();  // Return empty string if no engine bound
}

} // namespace fl
