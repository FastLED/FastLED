#pragma once

#include "fl/channels/bus.h"
#include "fl/stl/type_traits.h"

namespace fl {
namespace detail {

template<typename...> struct ControllerVoid { using type = void; };

// Some legacy platforms have no Channel default bus. Keep that lookup
// dependent so ordinary addLeds<> calls retain their original controller.
template<typename Chipset, typename = void>
struct ClocklessDefaultBus { static constexpr Bus value = Bus::AUTO; };

template<typename Chipset>
struct ClocklessDefaultBus<Chipset,
    typename ControllerVoid<decltype(DefaultBus<Chipset>::value)>::type> {
    static constexpr Bus value = DefaultBus<Chipset>::value;
};

// Native controllers without a rebind hook can retain their platform default.
// No type is exposed for an unsupported explicit transport.
template<typename Controller, Bus B,
         bool IS_DEFAULT = (B == ClocklessDefaultBus<ClocklessChipset>::value)>
struct ClocklessNativeBusController {};

template<typename Controller, Bus B>
struct ClocklessNativeBusController<Controller, B, true> {
    using type = Controller;
};

/// Auto preserves the original controller; every explicit bus uses its hook.
template<typename Controller, Bus B, bool IS_AUTO = (B == Bus::AUTO),
         typename = void>
struct ClocklessBusController : ClocklessNativeBusController<Controller, B> {};

template<typename Controller, Bus B>
struct ClocklessBusController<Controller, B, true, void> {
    using type = Controller;
};

template<typename Controller, Bus B>
struct ClocklessBusController<Controller, B, false,
    typename ControllerVoid<typename Controller::template WithBus<B>>::type> {
    using Rebound = typename Controller::template WithBus<B>;
    // An inherited alias for the current transport names an existing base.
    // Keep the derived controller's native initialization and encoding hooks.
    using type = typename fl::conditional<
        fl::is_base_of<Rebound, Controller>::value, Controller, Rebound>::type;
};

} // namespace detail
} // namespace fl
