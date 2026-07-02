// IWYU pragma: private

/// @file i2s_port_claim.cpp.hpp
/// @brief Impl of the classic-ESP32 I2S port-claim registry
///        (FastLED#3576 Phase 1). See i2s_port_claim.h.

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "platforms/esp/32/drivers/i2s/i2s_port_claim.h"

#include "fl/log/log.h"

namespace fl {

namespace {
// One owner slot per I2S block. Owner identity is the string POINTER
// (drivers pass stable literals), so idempotent re-claims by the same
// driver are free.
const char *g_i2s_port_owner[2] = {nullptr, nullptr};
} // anonymous namespace

bool i2sPortClaim(int port, const char *owner) FL_NO_EXCEPT {
    if (port < 0 || port > 1 || owner == nullptr) {
        return false;
    }
    if (g_i2s_port_owner[port] == nullptr ||
        g_i2s_port_owner[port] == owner) {
        g_i2s_port_owner[port] = owner;
        return true;
    }
    FL_WARN_F("i2sPortClaim: I2S%s already owned by %s (requested by %s)",
              port, g_i2s_port_owner[port], owner);
    return false;
}

void i2sPortRelease(int port, const char *owner) FL_NO_EXCEPT {
    if (port < 0 || port > 1) {
        return;
    }
    if (g_i2s_port_owner[port] == owner) {
        g_i2s_port_owner[port] = nullptr;
    }
}

const char *i2sPortOwner(int port) FL_NO_EXCEPT {
    if (port < 0 || port > 1) {
        return nullptr;
    }
    return g_i2s_port_owner[port];
}

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32
