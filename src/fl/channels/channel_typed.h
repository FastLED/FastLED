#pragma once

/// @file channel_typed.h
/// @brief Phase 3b templated channel facade -- `Channel<Bus, Chipset>` with
///        compile-time bus/chipset enforcement.
///
/// This header introduces the templated `Channel<B, Chipset>` form requested by
/// issue #2428. It is intentionally a thin wrapper around the non-template
/// `fl::Channel` runtime object so that:
///
///   - Existing `Channel::create(cfg)` callers and `addLeds<>()` subclasses of
///     the non-template `Channel` keep working unchanged (zero source impact).
///   - The new templated entry point (`Channel<B, Chipset>::create(cfg)`,
///     `FastLED.add<B, Chipset>(cfg)`) enforces the `Bus`<->`Chipset` contract
///     at compile time via `static_assert(BusSupports<B, Chipset>::value, ...)`.
///   - Specifying an undefined `Bus` for the current platform produces a clean
///     "implicit instantiation of undefined template `BusTraits<Bus::X>`"
///     diagnostic rather than a silent runtime fallback.
///
/// The runtime polymorphic surface (`IChannel`, `ChannelEvents` callbacks, the
/// `ChannelManager` registry) is unchanged -- `Channel<B, Chipset>::create()`
/// returns a `ChannelPtr` (shared_ptr to the existing non-template `Channel`)
/// so everything downstream continues to see one channel type.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"

namespace fl {

namespace detail {

/// @brief Resolve a possibly-`Bus::AUTO` template argument to the concrete
///        platform default for the given `Chipset` family.
template<Bus B, typename Chipset>
struct resolve_bus {
    static constexpr Bus value = B;
};

template<typename Chipset>
struct resolve_bus<Bus::AUTO, Chipset> {
    static constexpr Bus value = DefaultBus<Chipset>::value;
};

}  // namespace detail

/// @brief Templated channel facade. See file-level comment for the design.
///
/// @tparam B       Driver bus identifier. `Bus::AUTO` resolves to
///                 `DefaultBus<Chipset>::value` for the current platform.
/// @tparam Chipset Chipset family (`ClocklessChipset` or `SpiChipsetConfig`).
///
/// `Channel<B, Chipset>` is **not** instantiated as a runtime object. It
/// exists so the templated `create()` factory can `static_assert` the
/// bus/chipset compatibility before constructing a (non-template) `fl::Channel`.
/// This matches the API requested in issue #2428 without forcing every existing
/// `Channel` consumer (subclasses like `ClocklessIdf5`, `ChannelEvents`
/// callbacks, `ChannelManager`) to be retemplated.
template<Bus B, typename Chipset, fl::u8 Which = 0>
class TypedChannel {
public:
    /// The bus actually used after resolving `Bus::AUTO`.
    static constexpr Bus kBus = detail::resolve_bus<B, Chipset>::value;

    // Compile-time contract: the named bus must accept this chipset family.
    //
    // The diagnostic message (suppressed on pre-C++17 toolchains by
    // FL_STATIC_ASSERT) describes the contract: route ClocklessChipset to a
    // clockless bus (RMT/PARLIO/I2S/...) and SpiChipsetConfig to an SPI bus.
    FL_STATIC_ASSERT(
        BusSupports<kBus, Chipset, Which>::value,
        "TypedChannel: Bus does not support this Chipset family");

    /// @brief Construct a runtime `Channel` from a typed configuration.
    ///
    /// The factory returns a `ChannelPtr` to the existing non-template
    /// `fl::Channel` runtime object so the rest of the system (events,
    /// manager, draw list) is unchanged.
    static ChannelPtr create(const ChannelConfigOf<Chipset>& cfg) FL_NO_EXCEPT {
        // Naming `BusTraits<kBus>::instance` here is the ODR-use that links
        // the driver translation unit even in the static-only `--gc-sections`
        // mode (issue #2428 Phase 5 binary-size fix).
        (void)&BusTraits<kBus, Which>::instance;
        ChannelConfig erased = cfg.toErased();
        erased.options.mBus = kBus;
        erased.options.mBusWhich = Which;
        return Channel::create(erased);
    }

    /// @brief Overload accepting the (type-erased) non-template config.
    ///
    /// Useful for callers that already hold a `ChannelConfig` but want
    /// compile-time bus/chipset validation. The runtime chipset variant must
    /// match `Chipset` -- there's no compile-time guarantee at this overload,
    /// so the static_assert above is the only contract.
    static ChannelPtr create(const ChannelConfig& cfg) FL_NO_EXCEPT {
        (void)&BusTraits<kBus, Which>::instance;
        ChannelConfig erased = cfg;
        erased.options.mBus = kBus;
        erased.options.mBusWhich = Which;
        return Channel::create(erased);
    }

private:
    TypedChannel() FL_NO_EXCEPT = delete;
};

}  // namespace fl
