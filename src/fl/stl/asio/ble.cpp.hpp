// IWYU pragma: private
// src/fl/stl/asio/ble.cpp.hpp
//
// Stub implementations for platforms without BLE support.
// On ESP32 with NimBLE, the real implementation is compiled via the
// platform build chain (platforms/esp/32/drivers/ble/_build.cpp.hpp).
// Both paths are guarded by #pragma once in ble_esp32.cpp.hpp.

#pragma once

#include "fl/stl/asio/ble.h"

#if !FL_BLE_AVAILABLE

namespace fl {

BleTransportState* createBleTransport(const char*) {
    return nullptr;
}

void destroyBleTransport(BleTransportState*) {
}

BleStatusInfo queryBleStatus(const BleTransportState*) {
    return BleStatusInfo{};
}

fl::pair<fl::function<fl::optional<fl::Json>()>, fl::function<void(const fl::Json&)>>
getBleTransportCallbacks(BleTransportState*) {
    return {
        fl::function<fl::optional<fl::Json>()>([]() -> fl::optional<fl::Json> { return fl::optional<fl::Json>(); }),
        fl::function<void(const fl::Json&)>([](const fl::Json&) {})
    };
}

} // namespace fl

#endif // !FL_BLE_AVAILABLE
