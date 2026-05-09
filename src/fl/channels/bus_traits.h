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
#include "fl/stl/type_traits.h"

namespace fl {

/// @brief Primary template — intentionally undefined.
///
/// Specializations live next to each driver under
/// `src/platforms/.../<driver>/bus_traits.h`. Naming an unsupported bus value
/// (e.g. `BusTraits<Bus::PARLIO>::instance()` on a platform without PARLIO)
/// produces an "implicit instantiation of undefined template" diagnostic.
template<fl::Bus B> struct BusTraits;

/// @brief Capability check: does bus `B` accept chipset family `Chipset`?
///
/// Defaults to `false_type`. Each driver's `bus_traits.h` adds `true_type`
/// specializations for the chipset families it supports, e.g.
///
/// @code
/// template<> struct BusSupports<Bus::PARLIO, ClocklessChipset> : fl::true_type {};
/// template<> struct BusSupports<Bus::SPI,    SpiChipsetConfig>  : fl::true_type {};
/// @endcode
///
/// `Channel<B, Chipset>` and `add<B>()` use this in a `static_assert` to reject
/// nonsense combinations (e.g. driving an SPI chipset over a clockless-only bus)
/// at compile time rather than at runtime.
template<fl::Bus B, typename Chipset>
struct BusSupports : fl::false_type {};

}  // namespace fl
