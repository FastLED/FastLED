#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief `BusTraits<Bus::STUB>` specialization for native/host/test builds.
///
/// Phase 1 canary — proves the trait machinery resolves end-to-end on the host
/// build. The driver implementation lives in
/// `platforms/stub/clockless_channel_engine_stub.h`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"  // ClocklessChipset
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)

#include "platforms/stub/clockless_channel_engine_stub.h"

namespace fl {

template<> struct BusTraits<Bus::STUB> {
    using Driver = fl::stub::ClocklessChannelEngineStub;

    /// @brief Singleton accessor — naming this is what links the driver TU.
    static fl::shared_ptr<Driver> instancePtr() FL_NOEXCEPT {
        static fl::shared_ptr<Driver> gHolder = fl::make_shared<Driver>();
        return gHolder;
    }

    static Driver& instance() FL_NOEXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NOEXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::STUB), instancePtr());
    }
};

template<> struct BusSupports<Bus::STUB, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_STUB || FL_IS_WASM
