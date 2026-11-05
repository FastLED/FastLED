#pragma once

#include "fl/channels/bus.h"

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

/// Rebind an explicit bus without changing a custom controller's encoding.
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
