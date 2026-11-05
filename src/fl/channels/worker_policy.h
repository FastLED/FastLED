#pragma once

#include "fl/channels/bus.h"

namespace fl {
namespace detail {

template<typename...> struct ControllerVoid { using type = void; };

// Some legacy platforms have no Channel default bus. Keep that lookup
// dependent so their ordinary addLeds<> path remains available unchanged.
template<typename Chipset, typename = void>
struct ClocklessDefaultBus { static constexpr Bus value = Bus::AUTO; };

template<typename Chipset>
struct ClocklessDefaultBus<Chipset,
    typename ControllerVoid<decltype(DefaultBus<Chipset>::value)>::type> {
    static constexpr Bus value = DefaultBus<Chipset>::value;
};

/// Preserve platform-specific legacy controllers for the default bus. Slim
/// bridges expose a bus-only rebind alias for explicit transport selection.
template<typename Controller, Bus B,
         bool USE_DEFAULT = (B == Bus::AUTO ||
             B == ClocklessDefaultBus<ClocklessChipset>::value),
         typename = void>
struct ClocklessBusController {
    using type = Controller;
};

template<typename Controller, Bus B>
struct ClocklessBusController<Controller, B, false,
    typename ControllerVoid<typename Controller::template WithBus<B>>::type> {
    using type = typename Controller::template WithBus<B>;
};

} // namespace detail
} // namespace fl
