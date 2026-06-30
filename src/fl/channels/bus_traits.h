#pragma once

/// @file bus_traits.h
/// @brief Per-driver traits keyed on `fl::Bus`.
///
/// Each concrete driver provides a specialization of `BusTraits<Bus::X>`
/// (typically in a `bus_traits.h` adjacent to the driver header). The
/// specialization carries:
///   - `using Driver = ...;`        the concrete driver class.
///   - `static Driver& instance();` Meyers singleton accessor.
///   - One or more `BusSupports<Bus::X, Chipset>` `true_type` specializations
///     declaring which chipset families the driver can handle.
///
/// Naming `BusTraits<Bus::X>::instance()` anywhere in the program is what
/// causes the linker to keep the driver's translation unit. If no template
/// instantiation references it, the driver TU is dead-stripped. See #2428.

#include "fl/channels/bus.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"

namespace fl {

/// @brief Primary template — intentionally undefined.
///
/// Specializations live next to each driver under
/// `src/platforms/.../<driver>/bus_traits.h`. Naming an unsupported bus value
/// (e.g. `BusTraits<Bus::FLEX_IO, 0>::instance()` on a platform without
/// a matching parallel-I/O driver)
/// produces an "implicit instantiation of undefined template" diagnostic.
template<fl::Bus B, fl::u8 Which = 0> struct BusTraits;

/// @brief Capability check: does bus `B` accept chipset family `Chipset`?
///
/// Defaults to `false_type`. Each driver's `bus_traits.h` adds `true_type`
/// specializations for the chipset families it supports, e.g.
///
/// @code
/// template<> struct BusSupports<Bus::FLEX_IO, ClocklessChipset, 0> : fl::true_type {};
/// template<> struct BusSupports<Bus::SPI,    SpiChipsetConfig>  : fl::true_type {};
/// @endcode
///
/// `Channel<B, Chipset>` and `add<B>()` use this in a `static_assert` to reject
/// nonsense combinations (e.g. driving an SPI chipset over a clockless-only bus)
/// at compile time rather than at runtime.
template<fl::Bus B, typename Chipset, fl::u8 Which = 0>
struct BusSupports : fl::false_type {};

namespace detail {

/// @brief Helper used by `enableDrivers<Bus...>()` to expand the parameter pack.
/// Each call ODR-uses `BusTraits<B>::registerWithManager()`, which lazily
/// constructs the driver singleton AND registers it with `ChannelManager`.
template<Bus B, fl::u8 Which = 0>
inline int bus_register_one() FL_NO_EXCEPT {
    BusTraits<B, Which>::registerWithManager();
    return 0;
}

}  // namespace detail

/// @brief Register the named drivers with `ChannelManager` for runtime selection.
///
/// Each named bus's `BusTraits<B>::registerWithManager()` is invoked, which:
///  1. Lazily constructs the driver singleton (links its translation unit).
///  2. Calls `ChannelManager::addDriver(priority, instance)` so the runtime
///     manager can route channels to it.
///
/// **Pre-condition:** the per-driver `bus_traits.h` for each `B` named here
/// must be reachable from the calling translation unit. Including the
/// per-platform driver headers brings the relevant specializations in.
/// Naming a `Bus` whose `BusTraits<B>` specialization is not visible
/// produces a clear "implicit instantiation of undefined template" diagnostic.
///
/// @code
///   #include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"
///   #include "platforms/esp/32/drivers/parlio/bus_traits.h"
///   fl::enableDriver<fl::Bus::FLEX_IO, 0>();  // PARLIO/LCD_CAM/etc. slot
/// @endcode
template<Bus... Buses>
inline void enableDrivers() FL_NO_EXCEPT {
    // C++11-compatible pack expansion via array initializer trick.
    using expand = int[];
    (void)expand{0, detail::bus_register_one<Buses>()...};
}

template<Bus B, fl::u8 Which = 0>
inline void enableDriver() FL_NO_EXCEPT {
    detail::bus_register_one<B, Which>();
}

/// @brief Compile-time linker keep-alive hook for a single `fl::Bus`.
///
/// Used by the legacy `FastLED.addLeds<..., fl::Bus B>(...)` template path
/// (see #2460). When `B != Bus::AUTO`, naming `BusTraits<B>::instance` here
/// is what makes `--gc-sections` retain the named driver's translation unit
/// even though the legacy controller body never invokes the singleton
/// directly. The `Bus::AUTO` specialization is a no-op so calls without an
/// explicit Bus param remain byte-for-byte identical to pre-#2460 code.
namespace detail {

template<fl::Bus B, fl::u8 Which = 0>
struct BusKeepAliveImpl {
    static inline void odr_use() FL_NO_EXCEPT {
        // ODR-use the singleton accessor's address — sufficient to keep
        // the driver TU alive. We do NOT call `instance()` here because
        // some platforms construct the driver eagerly inside the Meyers
        // singleton and we want `Bus::X`-tagged `addLeds<>` to behave as
        // a pure compile-time annotation (no runtime side effect beyond
        // the linker keep-alive itself).
        (void)&BusTraits<B, Which>::instance;
    }
};

template<fl::u8 Which>
struct BusKeepAliveImpl<fl::Bus::AUTO, Which> {
    static inline void odr_use() FL_NO_EXCEPT {}  // AUTO: no pinning, no ODR-use.
};

}  // namespace detail

template<fl::Bus B, fl::u8 Which = 0>
inline void busKeepAlive() FL_NO_EXCEPT {
    detail::BusKeepAliveImpl<B, Which>::odr_use();
}

}  // namespace fl
