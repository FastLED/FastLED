/// @file channel.cpp
/// @brief LED channel implementation

#include "fl/channels/channel.h"
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

ChannelPtr Channel::create(const ChannelConfig &config) {
    // Select engine based on chipset type
    IChannelEngine* selectedEngine = nullptr;

    // Handle affinity binding if specified
    if (config.options.mAffinity && config.options.mAffinity[0]) {
        selectedEngine = ChannelBusManager::instance().getEngineByName(config.options.mAffinity);
        if (selectedEngine) {
            FL_DBG("Channel: Bound to engine '" << config.options.mAffinity << "' via affinity");
        } else {
            FL_ERROR("Channel: Affinity engine '" << config.options.mAffinity << "' not found - using ChannelBusManager");
        }
    }

    // If no affinity or not found, select engine based on chipset type
    if (!selectedEngine) {
        if (config.chipset.is<SpiChipsetConfig>()) {
            // For SPI chipsets, try to get SPI engine first
            IChannelEngine* spiEngine = ChannelBusManager::instance().getEngineByName("SPI");
            if (spiEngine) {
                selectedEngine = spiEngine;
                FL_DBG("Channel (SPI): Using SPI engine");
            } else {
                // Fall back to ChannelBusManager
                selectedEngine = &ChannelBusManager::instance();
                FL_DBG("Channel (SPI): SPI engine not available, using ChannelBusManager");
            }
        } else {
            // Clockless chipsets use ChannelBusManager
            selectedEngine = &ChannelBusManager::instance();
            FL_DBG("Channel (clockless): Using ChannelBusManager for engine selection");
        }
    }

    return fl::make_shared<Channel>(config.chipset, config.mLeds,
                                     config.rgb_order, selectedEngine, config.options);
}

// Helper to extract data pin from chipset variant
namespace {
int getDataPinFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->pin;
    } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;
}

// Helper to extract timing from chipset variant (clockless only)
ChipsetTimingConfig getTimingFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->timing;
    }
    return ChipsetTimingConfig(0, 0, 0, 0);  // Invalid/empty timing for SPI
}
} // anonymous namespace

Channel::Channel(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                 EOrder rgbOrder, IChannelEngine* engine, const ChannelOptions& options)
    : CPixelLEDController<RGB>()
    , mChipset(chipset)
    , mPin(getDataPinFromChipset(chipset))
    , mTiming(getTimingFromChipset(chipset))
    , mRgbOrder(rgbOrder)
    , mEngine(engine)
    , mId(nextId()) {
#ifdef ESP32
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

    // Create ChannelData during construction
    mChannelData = ChannelData::create(mPin, mTiming);
}

// Backwards-compatible constructor (deprecated)
Channel::Channel(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                 EOrder rgbOrder, IChannelEngine* engine, const ChannelOptions& options)
    : CPixelLEDController<RGB>()
    , mChipset(ClocklessChipset(pin, timing))  // Convert to variant
    , mPin(pin)
    , mTiming(timing)
    , mRgbOrder(rgbOrder)
    , mEngine(engine)
    , mId(nextId()) {
#ifdef ESP32
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

Channel::~Channel() {}

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
        FL_ASSERT(false, "Channel " << mId << ": Skipping update - buffer in use by engine");
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
