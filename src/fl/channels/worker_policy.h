#pragma once

#include "fl/channels/bus.h"
#include "fl/channels/driver.h"
#include "fl/channels/worker_config.h"
#include "fl/stl/static_assert.h"

namespace fl {
namespace detail {

/// No per-controller storage: the template value is submitted to the shared
/// engine directly. The default specialization does not configure any worker.
template<Core CORE> struct WorkerPolicy {
    FL_STATIC_ASSERT(CORE == Core::Any || CORE == Core::CallingCore ||
                     CORE == Core::Core0 || CORE == Core::Core1,
                     "Invalid worker core");
    static bool apply(IChannelDriver& driver) FL_NO_EXCEPT {
        WorkerConfig config;
        config.core = CORE;
        return driver.configureWorker(config);
    }
};

template<> struct WorkerPolicy<Core::Auto> {
    static bool apply(IChannelDriver&) FL_NO_EXCEPT { return true; }
};

template<typename...> struct WorkerVoid { using type = void; };

// Some legacy platforms have no Channel default bus. Keep that lookup
// dependent so their ordinary addLeds<> path remains available unchanged.
template<typename Chipset, typename = void>
struct WorkerDefaultBus { static constexpr Bus value = Bus::AUTO; };

template<typename Chipset>
struct WorkerDefaultBus<Chipset,
    typename WorkerVoid<decltype(DefaultBus<Chipset>::value)>::type> {
    static constexpr Bus value = DefaultBus<Chipset>::value;
};

/// Preserve platform-specific legacy controllers unless explicit worker
/// affinity is requested. Slim bridges expose a rebind alias; unsupported
/// legacy controllers diagnose the request at compile time.
template<typename Controller, Bus B, Core CORE,
         bool USE_DEFAULT = (CORE == Core::Auto &&
             (B == Bus::AUTO || B == WorkerDefaultBus<ClocklessChipset>::value)),
         typename = void>
struct ClocklessWorkerController {
    FL_STATIC_ASSERT(CORE == Core::Auto,
                     "This legacy controller does not support worker affinity");
    using type = Controller;
};

template<typename Controller, Bus B, Core CORE>
struct ClocklessWorkerController<Controller, B, CORE, false,
    typename WorkerVoid<typename Controller::template WithWorker<B, CORE>>::type> {
    using type = typename Controller::template WithWorker<B, CORE>;
};

} // namespace detail
} // namespace fl
