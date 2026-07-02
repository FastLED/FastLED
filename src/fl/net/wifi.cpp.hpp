// IWYU pragma: private
// src/fl/net/wifi.cpp.hpp
//
// Stub implementations for platforms without WiFi support. On WiFi
// silicon the real implementation is compiled via the platform build
// chain (platforms/esp/32/net/_build.cpp.hpp). Same split as
// fl/net/ble.cpp.hpp.

#pragma once

#include "fl/net/wifi.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace net {
namespace wifi {

const char* toString(Status s) FL_NO_EXCEPT {
    switch (s) {
    case Status::UNSUPPORTED: return "UNSUPPORTED";
    case Status::IDLE:        return "IDLE";
    case Status::CONNECTING:  return "CONNECTING";
    case Status::CONNECTED:   return "CONNECTED";
    case Status::FAILED:      return "FAILED";
    case Status::AP_ACTIVE:   return "AP_ACTIVE";
    }
    return "UNKNOWN";
}

} // namespace wifi
} // namespace net
} // namespace fl

#if !FL_WIFI_AVAILABLE

namespace fl {
namespace net {
namespace wifi {

bool connectSta(const char*, const char*) FL_NO_EXCEPT { return false; }
bool startAp(const char*, const char*, u8) FL_NO_EXCEPT { return false; }
void stop() FL_NO_EXCEPT {}
Status status() FL_NO_EXCEPT { return Status::UNSUPPORTED; }
bool isConnected() FL_NO_EXCEPT { return false; }
fl::string ipAddress() FL_NO_EXCEPT { return fl::string(); }
fl::string apIpAddress() FL_NO_EXCEPT { return fl::string(); }

} // namespace wifi
} // namespace net
} // namespace fl

#endif // !FL_WIFI_AVAILABLE
