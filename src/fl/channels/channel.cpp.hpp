/// @file channel.cpp
/// @brief LED channel implementation

#include "platforms/is_platform.h"
#include "fl/channels/channel.h"
#include "fl/channels/channel_events.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/atomic.h"
#include "fl/system/log.h"
#include "fl/channels/options.h"
#include "fl/gfx/pixel_iterator_any.h"
#include "pixel_controller.h"
#include "fl/trace.h"

#include "fl/system/pin.h"
#include "fl/chipsets/encoders/ucs7604.h"
#include "fl/chipsets/ucs7604.h"
#include "fl/math/ease.h"
#include "fl/stl/iterator.h"
#include "fl/system/engine_events.h"
#include "fl/math/xymap.h"
#include "fl/stl/singleton.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace {

/// @brief Encapsulates pixel iterator construction with optional XYMap reordering
class ReorderingPixelIteratorAny {
  private:
    /// @brief Get thread-local buffer for addressing transformation
    /// @return Reference to thread-local CRGB vector (reused across calls)
    static fl::vector<CRGB>& getReorderBufferTLS() {
        return SingletonThreadLocal<fl::vector<CRGB>>::instance();
    }
    fl::optional<PixelController<RGB, 1, 0xFFFFFFFF>> mAddressedController;
    PixelIteratorAny mPixelIterator;

  public:
    /// @brief Construct pixel iterator with optional addressing transformation
    /// @param pixels Source pixel controller
    /// @param addressing Optional XYMap for transformation
    /// @param rgbOrder RGB color order
    /// @param rgbw RGBW settings
    /// @param channelName Channel name for error messages
    ReorderingPixelIteratorAny(
        PixelController<RGB, 1, 0xFFFFFFFF>& pixels,
        const XYMap* addressing,
        EOrder rgbOrder,
        Rgbw rgbw,
        const fl::string& channelName)
        : mPixelIterator(pixels, rgbOrder, rgbw) {

        // Apply addressing transformation if configured
        if (addressing) {
            u16 numLeds = pixels.size();
            u16 width = addressing->getWidth();
            u16 height = addressing->getHeight();
            u16 expectedLeds = width * height;

            // Validate that XYMap dimensions match channel LED count
            if (expectedLeds != numLeds) {
                FL_ERROR("Channel '" << channelName << "': XYMap dimensions (" << width << "x" << height
                        << "=" << expectedLeds << ") don't match LED count (" << numLeds
                        << "). Addressing transformation may produce unexpected results.");
            }

            // Cast mData to CRGB array
            const CRGB* pixelArray = (const CRGB*)pixels.mData;

            // Get thread-local buffer (reuses allocation if size unchanged)
            fl::vector<CRGB>& buffer = getReorderBufferTLS();
            buffer.clear();
            buffer.resize(numLeds);

            // Fill buffer by mapping each physical index to its source
            for (u16 physicalIdx = 0; physicalIdx < numLeds; physicalIdx++) {
                u16 x = physicalIdx % width;
                u16 y = physicalIdx / width;
                u16 sourceIdx = addressing->mapToIndex(x, y);
                buffer[physicalIdx] = (sourceIdx < numLeds) ? pixelArray[sourceIdx] : CRGB::Black;
            }

            // Construct PixelController with reordered buffer
            mAddressedController.emplace(
                buffer.data(), buffer.size(),
                pixels.mColorAdjustment, DISABLE_DITHER);
            mPixelIterator = PixelIteratorAny(mAddressedController.value(), rgbOrder, rgbw);
        }
    }

    /// @brief Get the constructed pixel iterator
    PixelIterator& get() { return mPixelIterator.get(); }
    const PixelIterator& get() const { return mPixelIterator.get(); }
};

}  // anonymous namespace


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
    // Late binding strategy: Always create with empty driver
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

int Channel::getPin() const {
    if (const ClocklessChipset* cs = mChipset.ptr<ClocklessChipset>()) {
        return cs->pin;
    }
    if (const SpiChipsetConfig* spi = mChipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;
}

const ChipsetTimingConfig& Channel::getTiming() const {
    if (const ClocklessChipset* cs = mChipset.ptr<ClocklessChipset>()) {
        return cs->timing;
    }
    static const ChipsetTimingConfig sEmpty(0, 0, 0, 0);
    return sEmpty;
}

Channel::Channel(const ChipsetVariant& chipset, EOrder rgbOrder, RegistrationMode mode)
    : CPixelLEDController<RGB>(mode)
    , mChipset(chipset)
    , mRgbOrder(rgbOrder)
    , mDriver()
    , mAffinity()
    , mId(nextId())
    , mName(makeName(mId)) {
    // NOTE: Do NOT call fl::pinMode() here — see comment in the
    // Channel(ChipsetVariant, span, EOrder, ChannelOptions) constructor.
    mChannelData = ChannelData::create(mChipset);
}

Channel::Channel(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                 EOrder rgbOrder, const ChannelOptions& options)
    : CPixelLEDController<RGB>(RegistrationMode::DeferRegister)  // Defer registration until FastLED.add()
    , mChipset(chipset)
    , mRgbOrder(rgbOrder)
    , mDriver()  // Empty weak_ptr - late binding on first showPixels()
    , mAffinity(options.mAffinity)  // Get affinity from ChannelOptions
    , mId(nextId())
    , mName(makeName(mId)) {
    // NOTE: Do NOT call fl::pinMode() here. The pin may already be
    // configured for SPI output by a persistent driver (ChannelEngineSpi).
    // Calling pinMode(InputPulldown) would disconnect the SPI MOSI from the
    // GPIO matrix, causing subsequent transmissions to produce no output.
    // The driver will configure the pin when it first uses it.

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
    , mRgbOrder(rgbOrder)
    , mDriver()  // Empty weak_ptr - late binding on first showPixels()
    , mAffinity(options.mAffinity)  // Get affinity from ChannelOptions
    , mId(nextId())
    , mName(makeName(mId)) {
    // NOTE: Do NOT call fl::pinMode() here — see comment in the
    // Channel(ChipsetVariant, span, EOrder, ChannelOptions) constructor.

    // Set the LED data array
    setLeds(leds);

    // Set color correction/temperature/dither/rgbw from ChannelOptions
    setCorrection(options.mCorrection);
    setTemperature(options.mTemperature);
    setDither(options.mDitherMode);
    setRgbw(options.mRgbw);

    // Create ChannelData during construction
    mChannelData = ChannelData::create(mChipset);
}

Channel::~Channel() FL_NOEXCEPT {
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


namespace {

/// @brief Encode UCS7604 pixel data into the channel data buffer
/// @param data Output byte vector (cleared by caller)
/// @param pixelIterator Pixel iterator with color order and RGBW conversion
/// @param encoder ClocklessEncoder value identifying the UCS7604 mode
/// @param settings Channel settings (for gamma override)
/// @param rgbOrder RGB ordering for current control reordering
void writeUCS7604(fl::vector_psram<u8>* data, PixelIterator& pixelIterator,
                  ClocklessEncoder encoder, const ChannelOptions& settings,
                  EOrder rgbOrder) {
    // Map encoder enum to UCS7604Mode
    UCS7604Mode mode;
    switch (encoder) {
        case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
            mode = UCS7604Mode::UCS7604_MODE_8BIT_800KHZ;
            break;
        case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
            mode = UCS7604Mode::UCS7604_MODE_16BIT_800KHZ;
            break;
        case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
            mode = UCS7604Mode::UCS7604_MODE_16BIT_1600KHZ;
            break;
        default:
            // Should never happen — caller already checked
            mode = UCS7604Mode::UCS7604_MODE_16BIT_800KHZ;
            break;
    }

    // Get current control from global UCS7604 brightness
    UCS7604CurrentControl current = ucs7604::brightness();

    // Reorder current control values to match wire order (same logic as UCS7604ControllerT)
    u8 rgb_currents[3] = {current.r, current.g, current.b};
    u8 pos0 = (static_cast<int>(rgbOrder) >> 6) & 0x3;
    u8 pos1 = (static_cast<int>(rgbOrder) >> 3) & 0x3;
    u8 pos2 = (static_cast<int>(rgbOrder) >> 0) & 0x3;
    UCS7604CurrentControl wire_current(
        rgb_currents[pos0], rgb_currents[pos1], rgb_currents[pos2], current.w);

    // Get gamma LUT
    float gamma = settings.mGamma.value_or(2.8f);
    fl::shared_ptr<const Gamma8> gamma8 = Gamma8::getOrCreate(gamma);

    bool is_rgbw = pixelIterator.get_rgbw().active();
    size_t num_leds = pixelIterator.size();

    // Encode into the data buffer
    encodeUCS7604(pixelIterator, num_leds, fl::back_inserter(*data),
                  mode, wire_current, is_rgbw, gamma8.get());
}

} // anonymous namespace

void Channel::showPixels(PixelController<RGB, 1, 0xFFFFFFFF> &pixels) {
    FL_SCOPED_TRACE;

    // Safety check: don't modify buffer if driver is currently transmitting it
    if (mChannelData->isInUse()) {
        FL_WARN("Channel '" << mName << "': showPixels() called while mChannelData is in use by driver, attempting to wait");
        auto driver = mDriver.lock();
        if (!driver) {
            FL_ERROR("Channel '" << mName << "': No driver bound yet the mChannelData is in use - cannot transmit");
            return;
        }
        // wait until the driver is in a READY state.
        bool ok = driver->waitForReady();
        if (!ok) {
            FL_ERROR("Channel '" << mName << "': Timeout occurred while waiting for driver to become READY");
            return;
        }
        FL_WARN("Channel '" << mName << "': Engine became READY after waiting");
    }

    auto driver = ChannelManager::instance().selectDriverForChannel(mChannelData, mAffinity);
    mDriver = driver;
    if (!driver) {
        FL_ERROR("Channel '" << mName << "': No compatible driver found - cannot transmit");
        return;
    }

    // Build pixel iterator with optional addressing transformation
    ReorderingPixelIteratorAny iterator(pixels, mScreenMap.getXYMap(), mRgbOrder, mSettings.mRgbw, mName);
    PixelIterator& pixelIterator = iterator.get();

    // Encode pixels based on chipset type
    auto& data = mChannelData->getData();
    data.clear();

    if (mChipset.is<ClocklessChipset>()) {
        // Clockless chipsets: dispatch based on encoder type
        const ClocklessChipset* clockless = mChipset.ptr<ClocklessChipset>();
        switch (clockless->timing.encoder) {
            case ClocklessEncoder::CLOCKLESS_ENCODER_WS2812:
                pixelIterator.writeWS2812(&data);
                break;
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
                writeUCS7604(&data, pixelIterator, clockless->timing.encoder,
                             mSettings, mRgbOrder);
                break;
        }
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



    // Enqueue for transmission (will be sent when driver->show() is called)
    driver->enqueue(mChannelData);
    auto& events = ChannelEvents::instance();
    events.onChannelEnqueued(*this, driver->getName());
}

void Channel::init() {
    // TODO: Implement initialization
}

// Stub driver - provides no-op implementation for testing or unsupported platforms
namespace {
class StubChannelEngine : public IChannelDriver {
public:
    virtual ~StubChannelEngine() FL_NOEXCEPT = default;

    virtual bool canHandle(const ChannelDataPtr& data) const override {
        (void)data;
        return true;  // Test driver accepts all channel types
    }

    virtual void enqueue(ChannelDataPtr /*channelData*/) override {
        // No-op: stub driver does nothing
        static bool warned = false;
        if (!warned) {
            FL_DBG("StubChannelEngine: No-op enqueue (use for testing or unsupported platforms)");
            warned = true;
        }
    }

    virtual void show() override {
        // No-op: no hardware to drive
    }

    virtual DriverState poll() override {
        return DriverState(DriverState::READY);  // Always "ready" (does nothing)
    }

    virtual fl::string getName() const override {
        return fl::string::from_literal("STUB");
    }

    virtual Capabilities getCapabilities() const override {
        return Capabilities(true, true);  // Stub accepts both clockless and SPI
    }
};

} // anonymous namespace

IChannelDriver* getStubChannelEngine() {
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

    // Clear driver weak_ptr when removed from draw list
    mDriver.reset();
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

Channel& Channel::setGamma(float gamma) {
    mSettings.mGamma = gamma;
    return *this;
}

fl::optional<float> Channel::getGamma() const {
    return mSettings.mGamma;
}

fl::string Channel::getEngineName() const {
    // Lock the weak_ptr to get a shared_ptr
    auto driver = mDriver.lock();
    if (driver) {
        return driver->getName();
    }
    return fl::string();  // Return empty string if no driver bound
}

Channel& Channel::setScreenMap(const fl::XYMap& map, float diameter) {
    fl::ScreenMap screenmap = map.toScreenMap();
    if (diameter <= 0.0f) {
        screenmap.setDiameter(.15f);  // Default diameter for small matrices
    } else {
        screenmap.setDiameter(diameter);
    }
    mScreenMap = screenmap;
    fl::EngineEvents::onCanvasUiSet(asController(), screenmap);
    return *this;
}

Channel& Channel::setScreenMap(const fl::ScreenMap& map) {
    mScreenMap = map;
    fl::EngineEvents::onCanvasUiSet(asController(), map);
    return *this;
}

Channel& Channel::setScreenMap(fl::u16 width, fl::u16 height, float diameter) {
    fl::XYMap xymap = fl::XYMap::constructRectangularGrid(width, height);
    return setScreenMap(xymap, diameter);
}

Channel& Channel::setScreenMap(const fl::XMap& map) {
    // Convert 1D XMap to 2D XYMap (width=length, height=1) and reuse existing logic
    fl::XYMap xymap = fl::XYMap::fromXMap(map);
    return setScreenMap(xymap, .15f);  // Use default diameter for 1D strips
}

const fl::ScreenMap& Channel::getScreenMap() const {
    return mScreenMap;
}

bool Channel::hasScreenMap() const {
    return mScreenMap.getLength() > 0;
}

} // namespace fl
