#pragma once

#include "fl/channels/bus.h"
#include "fl/channels/driver.h"
#include "fl/channels/worker_config.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/type_traits.h"

namespace fl {
namespace detail {

/// No per-controller storage: the template value is submitted to the shared
/// engine directly. The default specialization does not configure any worker.
template<Core CORE, Priority PRIORITY = Priority::Auto> struct WorkerPolicy {
    FL_STATIC_ASSERT(CORE == Core::Auto || CORE == Core::Any || CORE == Core::CallingCore ||
                     CORE == Core::Core0 || CORE == Core::Core1,
                     "Invalid worker core");
    FL_STATIC_ASSERT(PRIORITY == Priority::Auto || PRIORITY == Priority::High ||
                     PRIORITY == Priority::Medium || PRIORITY == Priority::Low,
                     "Invalid worker priority");
    static bool apply(IChannelDriver& driver) FL_NO_EXCEPT {
        WorkerConfig config;
        config.core = CORE;
        config.priority = PRIORITY;
        return driver.configureWorker(config);
    }
};

template<> struct WorkerPolicy<Core::Auto, Priority::Auto> {
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
/// scheduling is requested. Slim bridges expose a rebind alias; unsupported
/// legacy controllers diagnose the request at compile time.
template<typename Controller, Bus B, Core CORE, Priority PRIORITY = Priority::Auto,
         bool USE_DEFAULT = (CORE == Core::Auto && PRIORITY == Priority::Auto &&
             (B == Bus::AUTO || B == WorkerDefaultBus<ClocklessChipset>::value)),
         typename = void>
struct ClocklessWorkerController {
    FL_STATIC_ASSERT(CORE == Core::Auto && PRIORITY == Priority::Auto,
                     "This legacy controller does not support worker scheduling");
    using type = Controller;
};

template<typename Controller, Bus B, Core CORE, Priority PRIORITY>
struct ClocklessWorkerController<Controller, B, CORE, PRIORITY, false,
    typename WorkerVoid<
        typename fl::enable_if<PRIORITY != Priority::Auto>::type,
        typename Controller::template WithWorker<B, CORE, PRIORITY>>::type> {
    using type = typename Controller::template WithWorker<B, CORE, PRIORITY>;
};

// Existing custom controllers may implement only the core-aware rebind.
// Keep that interface usable when no explicit priority was requested.
template<typename Controller, Bus B, Core CORE>
struct ClocklessWorkerController<Controller, B, CORE, Priority::Auto, false,
    typename WorkerVoid<typename Controller::template WithWorker<B, CORE>>::type> {
    using type = typename Controller::template WithWorker<B, CORE>;
};

} // namespace detail
} // namespace fl
