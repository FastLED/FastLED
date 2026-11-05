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
template<Priority PRIORITY = Priority::Auto> struct WorkerPolicy {
    FL_STATIC_ASSERT(PRIORITY == Priority::Auto || PRIORITY == Priority::High ||
                     PRIORITY == Priority::Medium || PRIORITY == Priority::Low,
                     "Invalid worker priority");
    static bool apply(IChannelDriver& driver) FL_NO_EXCEPT {
        WorkerConfig config;
        config.priority = PRIORITY;
        return driver.configureWorker(config);
    }
};

template<> struct WorkerPolicy<Priority::Auto> {
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
template<typename Controller, Bus B, Priority PRIORITY = Priority::Auto,
         bool USE_DEFAULT = (PRIORITY == Priority::Auto &&
             (B == Bus::AUTO || B == WorkerDefaultBus<ClocklessChipset>::value)),
         typename = void>
struct ClocklessWorkerController {
    FL_STATIC_ASSERT(PRIORITY == Priority::Auto,
                     "This legacy controller does not support worker priority");
    using type = Controller;
};

template<typename Controller, Bus B, Priority PRIORITY>
struct ClocklessWorkerController<Controller, B, PRIORITY, false,
    typename WorkerVoid<
        typename fl::enable_if<PRIORITY != Priority::Auto>::type,
        typename Controller::template WithWorker<B, PRIORITY>>::type> {
    using type = typename Controller::template WithWorker<B, PRIORITY>;
};

// Custom controllers may implement only bus rebinding. Keep that interface
// usable when no explicit priority was requested.
template<typename Controller, Bus B>
struct ClocklessWorkerController<Controller, B, Priority::Auto, false,
    typename WorkerVoid<typename Controller::template WithWorker<B>>::type> {
    using type = typename Controller::template WithWorker<B>;
};

} // namespace detail
} // namespace fl
