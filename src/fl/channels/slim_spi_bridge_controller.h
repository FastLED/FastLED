/// @file slim_spi_bridge_controller.h
/// @brief Shared "slim bridge" legacy `addLeds<ESPIChipsets ...>()` controller
///        for the SPI channel API -- driver-agnostic over `IChannelDriver`
///        without ever instantiating a runtime `fl::Channel`.
///
/// Extracted (issue #4585, meta #4580) as the SPI sibling of
/// `fl/channels/slim_bridge_controller.h` (clockless). Where the clockless
/// slim bridge owns one `DATA_PIN`/`TIMING` pair built once at construction,
/// this owns one `SpiChipsetConfig` and re-resolves its driver every frame
/// via `ChannelManager::selectDriverForChannel()` -- mirroring
/// `Channel::resolveDynamicDriver()` (`fl/channels/channel.cpp.hpp`)
/// bit-for-bit. That mirroring is safe because
/// `TypedChannel<B, SpiChipsetConfig, Which>::create()` always resolves
/// `Bus::AUTO` to a concrete bus via `detail::resolve_bus<B, Chipset>` and
/// writes that *resolved* bus into `ChannelConfig::options.mBus` before
/// constructing the runtime `Channel` -- so the `Channel` this controller
/// replaces was never actually `Bus::AUTO`-dispatched at `showPixels()` time
/// either. This controller resolves the same way, using `kBus` (the
/// resolved bus) rather than the raw template argument `B`.
///
/// Colour-profile note: legacy `addLeds<ESPIChipsets ...>()` returns a bare
/// `CLEDController&` (a `Channel&`, under the old `TypedChannel` path).
/// Nothing in the public `Channel` surface (`fl/channels/channel.h`) exposes
/// a `setColorProfile()` -- profile binding is only reachable through
/// `ChannelConfig`/`ChannelOptions` at `Channel::create(cfg)` time
/// (`fl/channels/options.h`, `fl/channels/pipeline_binding.h`), and the
/// legacy `addLeds<>()` overloads in `FastLED.h` never surface that config
/// to the sketch. So a legacy SPI channel can never have a colour profile
/// bound, and the `tryEncodeManagedSpi()` branch inside
/// `Channel::encodeAPA102()` et al. (`fl/channels/channel.cpp.hpp`) is
/// always a no-op on this path. This controller therefore omits the
/// colour-managed branch entirely and calls the unmanaged `PixelIterator`
/// writers directly -- byte-identical to the old path's `managed == false`
/// case, which is the only case the old path could ever reach here.
///
/// `SpiChipset::MY9221` is never routed here: `FastLED.h` dispatches legacy
/// `addLeds<MY9221>` to the bit-bang `MY9221Controller`
/// (`fl/chipsets/my9221.h`), because the MY9221 clocks data on both clock
/// edges (DDR) and cannot be carried as SPI bytes (#4636).

#pragma once

#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

#include "cpixel_ledcontroller.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"  // IWYU pragma: keep (DriverTraits<Bus::X> satisfies the DriverTraits contract)
#include "fl/channels/channel_typed.h"  // IWYU pragma: keep (detail::resolve_bus, BusSupports)
#include "fl/channels/config.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/log/log.h"

namespace fl {

/// @brief Shared legacy `addLeds<ESPIChipsets ...>()` bridge controller over
///        an `IChannelDriver`, for chipsets whose encoding never needs the
///        colour-managed pipeline (see file-level comment).
///
/// @tparam CHIPSET    The SPI chipset protocol to encode for.
/// @tparam RGB_ORDER  Color byte ordering (RGB, GBR, etc.) -- applied by the
///                    base `CPixelLEDController<RGB_ORDER>`'s own
///                    `PixelController`, exactly like every non-Channel
///                    legacy SPI controller (`APA102Controller`, etc.).
/// @tparam B          Driver bus identifier. `Bus::AUTO` resolves to
///                    `DefaultBus<SpiChipsetConfig>::value` for the current
///                    platform (see `kBus` below).
/// @tparam B_WHICH    Bus instance index, for platforms with more than one
///                    instance of a given bus kind.
template <fl::SpiChipset CHIPSET, EOrder RGB_ORDER, fl::Bus B, fl::u8 B_WHICH>
class SlimSpiBridgeController : public CPixelLEDController<RGB_ORDER> {
public:
    /// The bus actually used after resolving `Bus::AUTO` -- mirrors
    /// `TypedChannel<B, SpiChipsetConfig, B_WHICH>::kBus`.
    static constexpr Bus kBus = detail::resolve_bus<B, SpiChipsetConfig>::value;

    // Compile-time contract, mirrors TypedChannel<B, SpiChipsetConfig, Which>.
    FL_STATIC_ASSERT(
        BusSupports<kBus, SpiChipsetConfig, B_WHICH>::value,
        "SlimSpiBridgeController: Bus does not support SpiChipsetConfig");

    explicit SlimSpiBridgeController(const SpiChipsetConfig& cfg) FL_NO_EXCEPT {
        mData = ChannelData::create(ChipsetVariant(cfg));
        // Idempotent by contract -- safe to call from every instantiation's
        // constructor (mirrors TypedChannel::create()'s registerWithManager() call).
        BusTraits<kBus, B_WHICH>::registerWithManager();
    }

    void init() FL_NO_EXCEPT override {}

    /// @brief Expose the underlying `ChannelDataPtr` for conformance tests.
    const ChannelDataPtr& channelData() const FL_NO_EXCEPT { return mData; }

protected:
    void showPixels(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT override {
        fl::shared_ptr<IChannelDriver> driver = resolveDriver();
        if (!driver) {
            return;
        }

        // (A1) Respect runtime driver enable/disable: ChannelManager is the
        // single source of truth. A disabled driver means "drop this frame",
        // not "enqueue and hope" -- silently enqueuing to a disabled driver
        // is the #2517 silent-drop failure mode.
        if (!ChannelManager::registry().isDriverEnabled(driver->getName().c_str())) {
            FL_WARN_ONCE("SlimSpiBridgeController: driver '%s' is disabled - dropping frame",
                         driver->getName().c_str());
            return;
        }

        // (A8) If the previous frame's buffer is still in flight, block
        // (bounded) until the driver reports it can accept more work rather
        // than racing the driver's in-progress transfer.
        if (mData->isInUse()) {
            if (!driver->waitForReady()) {
                FL_WARN_ONCE("SlimSpiBridgeController: driver '%s' did not become ready in "
                             "time - dropping frame", driver->getName().c_str());
                return;
            }
        }

        // (A5) Publish the pixel byte layout so the driver (and any
        // downstream padding/diagnostic code) knows the encoded stride.
        if (this->getRgbww().active()) {
            mData->setPixelFormat(ChannelPixelFormat::RGBWW);
        } else if (this->getRgbw().active()) {
            mData->setPixelFormat(ChannelPixelFormat::RGBW);
        } else {
            mData->setPixelFormat(ChannelPixelFormat::RGB);
        }

        // Dither reseed mirrors Channel::encodeFrame(): only reseed when a
        // non-zero dither mode is set.
        if ((pixels.e[0] | pixels.e[1] | pixels.e[2]) != 0) {
            pixels.reseed_binary_dithering(mDitherPhase);
        }

        // (A3) Re-encode into the same buffer every frame -- clear() keeps
        // capacity, so steady-state operation is allocation-free.
        mData->getData().clear();
        fl::PixelIterator it(&pixels, this->getRgbw(), this->getRgbww());
        encode(it, mData->getData());

        driver->enqueue(mData);
        // Presented, per the driver contract: an accepted enqueue is the
        // frame -- mirrors Channel::submitFrame()'s ++mDitherPhase placement
        // (dither only advances when a frame is actually handed to the
        // driver, not on every showPixels() attempt).
        ++mDitherPhase;
    }

private:
    /// @brief Resolve the driver for this frame. Mirrors
    ///        `Channel::resolveDynamicDriver()` (`channel.cpp.hpp`),
    ///        including the `FASTLED_DISABLE_DYNAMIC_DRIVER` gate -- this
    ///        controller never pre-binds a driver, so that gate simply drops
    ///        every frame here, matching `Channel::showPixels()`'s
    ///        non-pre-bound `else` branch under the same flag.
    fl::shared_ptr<IChannelDriver> resolveDriver() FL_NO_EXCEPT {
#if defined(FASTLED_DISABLE_DYNAMIC_DRIVER) && FASTLED_DISABLE_DYNAMIC_DRIVER
        return {};
#else
        // Build busKey only when we actually need it (kBus != AUTO).
        fl::string busKey;
        if (kBus != Bus::AUTO) {
            busKey = fl::string::from_literal(busDriverName(kBus, B_WHICH, /*spi=*/true));
        }

        fl::shared_ptr<IChannelDriver> driver =
            ChannelManager::registry().selectDriverForChannel(mData, busKey);

#if FASTLED_LOG_RUNTIME_ENABLED
        if (!driver && !mBusWarned) {
            FL_ERROR("SlimSpiBridgeController: no compatible driver found for bus '%s' - "
                     "cannot transmit", busKey.c_str());
            mBusWarned = true;
        }
#endif  // FASTLED_LOG_RUNTIME_ENABLED
        return driver;
#endif  // !FASTLED_DISABLE_DYNAMIC_DRIVER
    }

    /// @brief Encode `it` into `out` using the writer selected by `CHIPSET`.
    ///        Mirrors the `managed == false` branch of `Channel::encodeAPA102()`
    ///        .. `Channel::encodeHD108()` (`channel.cpp.hpp`, ~712-815) -- see
    ///        file-level comment for why `managed` is always `false` here.
    static void encode(fl::PixelIterator& it, fl::vector_psram<u8>& out) FL_NO_EXCEPT {
        switch (CHIPSET) {
            case fl::SpiChipset::APA102:
            case fl::SpiChipset::DOTSTAR:
            case fl::SpiChipset::HD107:
                it.writeAPA102(&out, false);
                break;
            case fl::SpiChipset::APA102HD:
            case fl::SpiChipset::DOTSTARHD:
            case fl::SpiChipset::HD107HD:
                it.writeAPA102(&out, true);
                break;
            case fl::SpiChipset::SK9822:
                it.writeSK9822(&out, false);
                break;
            case fl::SpiChipset::SK9822HD:
                it.writeSK9822(&out, true);
                break;
            case fl::SpiChipset::WS2801:
                it.writeWS2801(&out);
                break;
            case fl::SpiChipset::WS2803:
                it.writeWS2803(&out);
                break;
            case fl::SpiChipset::P9813:
                it.writeP9813(&out);
                break;
            case fl::SpiChipset::LPD8806:
                it.writeLPD8806(&out);
                break;
            case fl::SpiChipset::LPD6803:
                it.writeLPD6803(&out);
                break;
            case fl::SpiChipset::SM16716:
                it.writeSM16716(&out);
                break;
            case fl::SpiChipset::HD108:
                it.writeHD108(&out, false);
                break;
            case fl::SpiChipset::MY9221:
                // Never routed here -- FastLED.h sends MY9221 to the bit-bang
                // MY9221Controller (see file header comment). Defensive
                // no-op if this controller is ever instantiated directly
                // with CHIPSET == MY9221.
                break;
        }
    }

    ChannelDataPtr mData;
    u8 mDitherPhase = 0;
#if FASTLED_LOG_RUNTIME_ENABLED
    bool mBusWarned = false;
#endif
};

}  // namespace fl
