#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief `BusTraits<Bus::STUB>` specialization for native/host/test builds.
///
/// Phase 1 canary — proves the trait machinery resolves end-to-end on the host
/// build. The driver implementation lives in
/// `platforms/stub/clockless_channel_engine_stub.h`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"  // ClocklessChipset
#include "fl/stl/singleton.h"
#include "fl/stl/type_traits.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)

#include "platforms/stub/clockless_channel_engine_stub.h"

namespace fl {

template<> struct BusTraits<Bus::STUB> {
    using Driver = fl::stub::ClocklessChannelEngineStub;

    /// @brief Singleton accessor — naming this is what links the driver TU.
    /// Uses SingletonShared so a single instance is shared across DLL boundaries
    /// (matters on Windows where per-DLL static locals would otherwise diverge).
    static Driver& instance() FL_NOEXCEPT {
        return SingletonShared<Driver>::instance();
    }
};

template<> struct BusSupports<Bus::STUB, ClocklessChipset> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_STUB || FL_IS_WASM
