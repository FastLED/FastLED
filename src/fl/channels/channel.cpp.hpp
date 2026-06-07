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
#include "fl/log/log.h"
#include "fl/channels/options.h"
#include "fl/gfx/pixel_iterator_any.h"
#include "pixel_controller.h"
#include "fl/system/trace.h"

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

/// @brief Apply the white-channel selection from ChannelOptions to a
/// CLEDController. The variant alternative chooses RGB-only / RGBW / RGBWW
/// without forcing every call site to duplicate the dispatch.
inline void applyWhiteCfg(CLEDController& ctrl,
                          const ChannelOptions& options) FL_NOEXCEPT {
    if (auto* p = options.mWhiteCfg.ptr<Rgbww>()) {
        ctrl.setRgbww(*p);
    } else if (auto* p = options.mWhiteCfg.ptr<Rgbw>()) {
        ctrl.setRgbw(*p);
    } else {
        // Empty alternative (or default-constructed) → plain RGB.
        ctrl.clearWhiteChannel();
    }
}

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
        Rgbww rgbww,
        const fl::string& channelName)
        : mPixelIterator(pixels, rgbOrder, rgbw, rgbww) {

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
            mPixelIterator = PixelIteratorAny(mAddressedController.value(), rgbOrder, rgbw, rgbww);
        }
    }

    /// @brief Get the constructed pixel iterator
    PixelIterator& get() { return mPixelIterator.get(); }
    const PixelIterator& get() const { return mPixelIterator.get(); }
};

/// @brief Out-of-line cold-path emitter for the #2517 silent-drop
/// DISABLED-driver diagnostic in `Channel::showPixels`.
///
/// `showPixels` runs every frame, but this diagnostic is gated behind a
/// one-shot latch (`mDisabledDriverWarned`) and only fires when a bound
/// driver has been runtime-disabled (typically by
/// `FastLED.setExclusiveDriver<OtherBus>()`). Inlining the two
/// alternative FL_ERROR chains was costing ~10 `operator<<`
/// instantiations in showPixels' prologue/epilogue for code that runs
/// at most once per channel lifetime. Splitting matches the same
/// pattern the maintainer adopted for `resolveDynamicDriver()` in PR
/// #2830 — keep the diagnostic chain in a `FL_NO_INLINE` helper so it
/// can't fold back into the hot path. (#2773 follow-up to #2832.)
FL_NO_INLINE
static void emitDisabledDriverError(const fl::string& channelName,
                                    const fl::string& driverName,
                                    const fl::string& exclusive) FL_NOEXCEPT {
    if (!exclusive.empty()) {
        FL_ERROR("Channel '" << channelName << "': bound driver '" << driverName
            << "' is currently DISABLED by exclusive-driver selection '"
            << exclusive << "'. Frame will be silently dropped. "
            << "Resolve with: FastLED.enableDrivers<fl::Bus::"
            << driverName << ">() or FastLED.enableAllDrivers().");
    } else {
        FL_ERROR("Channel '" << channelName << "': bound driver '" << driverName
            << "' is currently DISABLED. Frame will be silently dropped. "
            << "Resolve with: FastLED.enableDrivers<fl::Bus::"
            << driverName << ">() or FastLED.enableAllDrivers().");
    }
}

}  // anonymous namespace


i32 Channel::nextId() {
    static fl::atomic<i32> gNextChannelId(0); // okay static in header
    return gNextChannelId.fetch_add(1);
}

fl::string Channel::makeName(i32 id, const fl::optional<fl::string>& configName) {
    if (configName.has_value()) {
        return configName.value();
    }
    // Auto-naming `"Channel_<id>"` only matters for diagnostics — in release
    // builds (NDEBUG → FASTLED_LOG_VERBOSITY=0 per Stage 1) every FL_WARN /
    // FL_ERROR site that reads mName collapses to `do { } while(0)`, so the
    // string and the supporting `fl::to_string` + `operator+` chain go
    // unused. Empty in release saves the `fl::to_string<i64>` instantiation
    // plus the heap allocation per Channel ctor. See #2942 / #2886.
#if FASTLED_LOG_RUNTIME_ENABLED
    return "Channel_" + fl::to_string(static_cast<i64>(id));
#else
    (void)id;
    return {};
#endif
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
    , mBus(Bus::AUTO)
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
    , mBus(options.mBus)  // Bus selection (#2459)
    , mId(nextId())
    , mName(makeName(mId)) {
    // NOTE: Do NOT call fl::pinMode() here. The pin may already be
    // configured for SPI output by a persistent driver (ChannelEngineSpi).
    // Calling pinMode(InputPulldown) would disconnect the SPI MOSI from the
    // GPIO matrix, causing subsequent transmissions to produce no output.
    // The driver will configure the pin when it first uses it.

    // Set the LED data array
    setLeds(leds);

    // Sync mSettings from ChannelOptions so the mWhiteCfg variant (and any
    // other future fields) survives to showPixels(). The setter calls below
    // are then idempotent overlays on top of the same data.
    mSettings = options;

    // Set color correction/temperature/dither/rgbw from ChannelOptions
    setCorrection(options.mCorrection);
    setTemperature(options.mTemperature);
    setDither(options.mDitherMode);
    applyWhiteCfg(*this, options);

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
    , mBus(options.mBus)  // Bus selection (#2459)
    , mId(nextId())
    , mName(makeName(mId)) {
    // NOTE: Do NOT call fl::pinMode() here — see comment in the
    // Channel(ChipsetVariant, span, EOrder, ChannelOptions) constructor.

    // Set the LED data array
    setLeds(leds);

    // Sync mSettings from ChannelOptions so the mWhiteCfg variant survives
    // to showPixels(). See sibling ChipsetVariant constructor for rationale.
    mSettings = options;

    // Set color correction/temperature/dither/rgbw from ChannelOptions
    setCorrection(options.mCorrection);
    setTemperature(options.mTemperature);
    setDither(options.mDitherMode);
    applyWhiteCfg(*this, options);

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
    // Sync mSettings from incoming options so the mWhiteCfg variant survives
    // to showPixels(). Setters below are idempotent overlays on the same data.
    mSettings = config.options;
    setCorrection(config.options.mCorrection);
    setTemperature(config.options.mTemperature);
    setDither(config.options.mDitherMode);
    applyWhiteCfg(*this, config.options);
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

    // (#2558) UCS7604 is a 4-channel chipset (always RGBW). If the user
    // configured this channel for 5-channel RGBWW (warm-W + cool-W), the
    // chipset can't carry the second W byte. Warn loudly — silently dropping
    // the white channels would be very hard to diagnose. The pixel iterator
    // continues to emit RGB-only bytes (is_rgbw=false) so the strip still
    // lights up, just without the W diode.
    if (pixelIterator.get_rgbww().active() && !is_rgbw) {
        FL_WARN_ONCE("UCS7604 cannot carry 5-channel RGBWW — this chipset "
                     "always emits 4-channel RGBW. The warm/cool white "
                     "channels in your Rgbww config will be dropped. "
                     "Set ChannelOptions.mWhiteCfg = Rgbw{...} instead to "
                     "silence this warning.");
    }

    // Encode into the data buffer
    encodeUCS7604(pixelIterator, num_leds, fl::back_inserter(*data),
                  mode, wire_current, is_rgbw, gamma8.get());
}

} // anonymous namespace

/// @brief Cold fallback for the non-pre-bound driver path. Handles dynamic
///        `ChannelManager::selectDriverForChannel` lookup AND the
///        bus-key-miss diagnostic chain. Hoisted out of `showPixels` so the
///        hot legacy `addLeds<>` path stays compact — see #2773 item 2.1.
///
/// Marked `noinline` (via `FL_NOINLINE`) so the compiler doesn't fold the
/// cold body back into `showPixels`. The whole helper is reachable only
/// when `mDriverPreBound == false`, which on stock Blink is never true —
/// LTO can use that across the call site to keep the cold body off the
/// hot icache line.
FL_NO_INLINE
fl::shared_ptr<IChannelDriver> Channel::resolveDynamicDriver() {
#if defined(FASTLED_DISABLE_DYNAMIC_DRIVER) && FASTLED_DISABLE_DYNAMIC_DRIVER
    // Body excluded via FASTLED_DISABLE_DYNAMIC_DRIVER (#2926). The
    // showPixels call site is also gated so this is unreachable; the
    // empty body lets the linker dead-strip ChannelManager::findDriverByName
    // and selectDriverForChannel along with this symbol.
    return {};
#else
    // Build busKey only when we actually need it (mBus != AUTO).
    fl::string busKey;
    if (mBus != Bus::AUTO) {
        busKey = fl::string::from_literal(busName(mBus));
    }

    fl::shared_ptr<IChannelDriver> driver =
        ChannelManager::instance().selectDriverForChannel(mChannelData, busKey);
    mDriver = driver;

    // #2455 / #2459: one-shot diagnostic when a typed-Bus miss happens.
    // Probe via `findDriverByName` (silent) to distinguish "driver wasn't
    // instantiated" from "driver exists but canHandle() rejected this
    // chipset" — resolution paths differ. The mBusWarned guard suppresses
    // duplicate logs on subsequent shows of the same channel.
#if FASTLED_LOG_RUNTIME_ENABLED
    if (mBus != Bus::AUTO && !mBusWarned &&
        (!driver || driver->getName() != busKey)) {
        auto busDriver = ChannelManager::instance().findDriverByName(busKey);
        if (!busDriver) {
            // Typed Bus miss — emit the actionable hint with the three
            // currently-shipping remediations (option 3 added in #2460).
            FL_ERROR("Channel '" << mName << "': Driver '" << busKey
                << "' wasn't instantiated. Resolve with: "
                << "(1) fl::enableDrivers<fl::Bus::" << busKey << ">() "
                << "(links only this driver), "
                << "(2) FastLED.enableAllDrivers() (links every driver), or "
                << "(3) FastLED.addLeds<..., fl::Bus::" << busKey << ">(...) "
                << "(legacy API; pins Bus + triggers linker keep-alive). "
                << "Defaulting to AUTO/priority dispatch.");
        } else {
            // Registered, but canHandle() said no — bus/chipset mismatch.
            FL_ERROR("Channel '" << mName << "': Driver '" << busKey
                << "' is registered but cannot handle this channel's chipset "
                << "(bus/chipset mismatch). Defaulting to AUTO/priority dispatch.");
        }
        mBusWarned = true;
    }
    if (!driver) {
        FL_ERROR("Channel '" << mName << "': No compatible driver found - cannot transmit");
    }
#endif  // FASTLED_LOG_RUNTIME_ENABLED — release skips the per-frame
        // driver->getName() != busKey compare + the silent
        // findDriverByName probe (used only to differentiate the
        // FL_ERROR message). The functional return value below is
        // preserved in both builds. See #2952.
    return driver;
#endif  // !FASTLED_DISABLE_DYNAMIC_DRIVER
}

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

    // Phase 5b of #2428: if the driver was pre-bound via setDriver() (legacy
    // addLeds<>-style controllers naming BusTraits<Bus::X>::instancePtr() in
    // their constructor), bypass ChannelManager entirely. Channels created via
    // the manager-based API (Channel::create(cfg) without affinity) keep their
    // existing per-frame re-selection so users can swap drivers at runtime.
    //
    // **Fast path (#2773 item 2.1):** the legacy `addLeds<NEOPIXEL>` flow is
    // by far the hot per-frame path on stock Blink. It needs no busKey
    // construction, no dynamic driver lookup, no busKey-miss diagnostics,
    // and no fallback `FL_ERROR` reporting — the driver was already pre-bound
    // in the controller's constructor. Pulling all of that boilerplate out
    // of `showPixels` lets the compiler keep the hot path compact and lets
    // the slow path's `fl::string` ops / `ChannelManager::selectDriverForChannel`
    // / diagnostic literals tree-shake on the slow-path branch's coldness.
    fl::shared_ptr<IChannelDriver> driver;
    if (mDriverPreBound) {
        driver = mDriver.lock();
        if (!driver) {
            // Pre-bound driver got destroyed (singleton shutdown, etc.). Silent
            // bail — this is unrecoverable from showPixels.
            return;
        }
    } else {
#if !defined(FASTLED_DISABLE_DYNAMIC_DRIVER) || !FASTLED_DISABLE_DYNAMIC_DRIVER
        driver = resolveDynamicDriver();
        if (!driver) {
            return;
        }
#else
        // Dynamic-driver lookup gated out via FASTLED_DISABLE_DYNAMIC_DRIVER
        // (#2926). The else branch is dead at runtime for every legacy
        // `addLeds<>` flavor — those pre-bind their driver in the ctor. The
        // gate lets `--gc-sections` drop the resolveDynamicDriver body plus
        // the ChannelManager::findDriverByName / selectDriverForChannel
        // chain (~400-900 B). Channels created via `Channel::create(cfg)`
        // without a pre-bound driver silently emit nothing under this flag —
        // user accepts the constraint.
        return;
#endif
    }

    // Build pixel iterator with optional addressing transformation
    // (#2558) Pass both Rgbw and Rgbww from the channel options; the iterator
    // carries both, and the encoder dispatch below picks the right path based
    // on which variant alternative ChannelOptions::mWhiteCfg holds.
    ReorderingPixelIteratorAny iterator(pixels, mScreenMap.getXYMap(), mRgbOrder,
                                        mSettings.rgbw(), mSettings.rgbww(),
                                        mName);
    PixelIterator& pixelIterator = iterator.get();

    // Encode pixels based on chipset type
    auto& data = mChannelData->getData();
    data.clear();

    if (mChipset.is<ClocklessChipset>()) {
        // Clockless chipsets: dispatch based on encoder type
        const ClocklessChipset* clockless = mChipset.ptr<ClocklessChipset>();
        switch (clockless->encoder) {
            case ClocklessEncoder::CLOCKLESS_ENCODER_WS2812:
                pixelIterator.writeWS2812(&data);
                break;
#if !defined(FASTLED_DISABLE_UCS7604) || !FASTLED_DISABLE_UCS7604
            // Gated by FASTLED_DISABLE_UCS7604 (#2920). For WS2812-only
            // sketches the UCS7604 case is dead at runtime, but each
            // `writeUCS7604(...)` reference is statically reachable,
            // keeping the encoder bodies linked. Setting
            // `-DFASTLED_DISABLE_UCS7604=1` drops the case + the
            // `encodeUCS7604_16bit_RGB` / `encodeUCS7604_16bit_RGBW`
            // template instantiations (~400-600 B). When the gate is
            // enabled, calling showPixels() on a UCS7604 channel
            // silently emits nothing.
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
            case ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
                writeUCS7604(&data, pixelIterator, clockless->encoder,
                             mSettings, mRgbOrder);
                break;
#endif  // !FASTLED_DISABLE_UCS7604
        }
#if !defined(FASTLED_DISABLE_SPI_CHIPSETS) || !FASTLED_DISABLE_SPI_CHIPSETS
    } else if (mChipset.is<SpiChipsetConfig>()) {
        // SPI chipsets: dispatch based on chipset type (zero allocation).
        //
        // Gated by FASTLED_DISABLE_SPI_CHIPSETS (#2913). For NEOPIXEL-only
        // sketches on ESP32-S3 the SPI branch is dead at runtime, but the
        // compiler cannot prove that — each pixelIterator.writeXXX(...)
        // reference below is statically reachable, keeping ~720 B of
        // encoder bodies (writeAPA102, writeSK9822, writeLPD8806,
        // writeSM16716) plus the 11-case switch table linked. Setting
        // `-DFASTLED_DISABLE_SPI_CHIPSETS=1` in build_flags drops the
        // whole branch and recovers ~1.0-1.2 KB on a NEOPIXEL Blink.
        //
        // When the gate is enabled, calling showPixels() on an
        // SpiChipsetConfig channel silently emits nothing — the user
        // accepts that constraint by setting the flag.
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
#endif  // !FASTLED_DISABLE_SPI_CHIPSETS

    // Fire event after encoding completes
    {
        auto& events = ChannelEvents::instance();
        events.onChannelDataEncoded(*this, *mChannelData);
    }



    // #2517: detect the silent-drop scenario before enqueuing — if the
    // resolved driver is registered with ChannelManager but currently
    // disabled (typically by `FastLED.setExclusiveDriver<OtherBus>()`),
    // `ChannelManager::onEndFrame()` will skip its `show()` call and the
    // frame is silently dropped on the floor. Emit an actionable
    // FL_ERROR so users can diagnose the missing output.
    //
    // One-shot via `mDisabledDriverWarned` to avoid per-frame spam.
    // Reset the latch whenever the driver returns to ENABLED so a later
    // disable re-emits the diagnostic.
    fl::string driverName = driver->getName();
    auto status = ChannelManager::instance().driverStatus(driverName);
    if (status == ChannelManager::DriverStatus::STATUS_DISABLED) {
#if FASTLED_LOG_RUNTIME_ENABLED
        if (!mDisabledDriverWarned) {
            emitDisabledDriverError(
                mName, driverName,
                ChannelManager::instance().exclusiveDriverName());
            mDisabledDriverWarned = true;
        }
#endif
        // Skip the enqueue — the data wouldn't be sent anyway, and dropping
        // it here matches the existing behaviour (rather than leaving stale
        // bytes in the driver's pending queue across an enable/disable flip).
        // The drop happens in both debug and release; only the diagnostic
        // and the one-shot latch are gated. In release, the empty
        // emitDisabledDriverError + the exclusiveDriverName() call + the
        // 3-arg fl::string chain dead-strip (see #2950).
        return;
    } else if (status == ChannelManager::DriverStatus::STATUS_ENABLED) {
#if FASTLED_LOG_RUNTIME_ENABLED
        // Reset the one-shot guard so a future disable re-emits the diagnostic.
        mDisabledDriverWarned = false;
#endif
    }
    // NOT_REGISTERED: driver is not managed by ChannelManager (e.g. a custom
    // test driver, or one that was removed). Fall through to enqueue and let
    // the driver decide what to do — this is the historic behaviour.

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
