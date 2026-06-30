#pragma once

/// @file bus_info.json.h
/// @brief JSON adapter for bus_info.h.

#include "fl/channels/bus_info.h"
#include "fl/stl/json.h"

namespace fl {

template<Bus B, fl::u8 Which = 0>
inline fl::json deviceJson() {
    const DeviceInfo info = deviceInfo<B, Which>();
    fl::json out = fl::json::object();
    out.set("bus", static_cast<i64>(static_cast<int>(info.bus)));
    out.set("bus_name", info.bus_name);
    out.set("vendor_name", info.vendor_name);
    out.set("device_name", info.device_name);
    out.set("which", static_cast<i64>(info.which));
    out.set("is_noop", info.is_noop);
    out.set("notes", info.notes);

    fl::json runtime = fl::json::object();
    runtime.set("available", info.runtime.available);
    runtime.set("initialized", info.runtime.initialized);
    runtime.set("ready", info.runtime.ready);
    runtime.set("busy", info.runtime.busy);
    runtime.set("error", info.runtime.error);
    out.set("runtime", runtime);
    return out;
}

}  // namespace fl
