// IWYU pragma: private
// src/fl/net/ble.cpp.hpp
//
// Stub implementations for platforms without BLE support.
// On ESP32 with NimBLE, the real implementation is compiled via the
// platform build chain (platforms/esp/32/drivers/ble/_build.cpp.hpp).
// Both paths are guarded by #pragma once in ble_esp32.cpp.hpp.

#pragma once

#include "fl/net/ble.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

#if !FL_BLE_AVAILABLE

namespace fl {
namespace net {
namespace ble {

TransportState* createTransport(const char*) FL_NOEXCEPT {
    FL_ERROR("BLE not implemented on this platform");
    return nullptr;
}

void destroyTransport(TransportState*) FL_NOEXCEPT {
    FL_ERROR("BLE not implemented on this platform");
}

StatusInfo queryStatus(const TransportState*) FL_NOEXCEPT {
    FL_ERROR("BLE not implemented on this platform");
    return StatusInfo{};
}

fl::pair<fl::function<fl::optional<fl::json>()>, fl::function<void(const fl::json&)>>
getTransportCallbacks(TransportState*) FL_NOEXCEPT {
    FL_ERROR("BLE not implemented on this platform");
    return {
        fl::function<fl::optional<fl::json>()>([]() -> fl::optional<fl::json> { return fl::optional<fl::json>(); }),
        fl::function<void(const fl::json&)>([](const fl::json&) {})
    };
}

} // namespace ble
} // namespace net
} // namespace fl

#endif // !FL_BLE_AVAILABLE
