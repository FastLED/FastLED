/// @file i2s_port_claim.h
/// @brief Cross-driver hardware-claim registry for the classic-ESP32
///        I2S ports (FastLED#3576 Phase 1).
///
/// Classic ESP32 has two identical I2S blocks. Three drivers can want
/// them:
///   - I2S1: the modern wave8 clockless bank (`ChannelEngineI2sEsp32Dev`
///     port 1, `Bus::FLEX_IO` instance 0)
///   - I2S0: EITHER the second clockless bank (port 0, `Bus::FLEX_IO`
///     instance 1) OR the clocked-SPI driver (`I2sSpiPeripheralEsp`,
///     APA102-class chipsets)
///
/// Each peripheral impl previously tracked ownership with its own
/// `mInitialized` member — invisible to the other driver classes, so
/// two drivers could program the same silicon. This registry is the
/// single ownership record: `initialize()` claims, `deinitialize()`
/// releases, first claim wins.
///
/// Not ISR-safe and not thread-safe by design: claims happen on the
/// show()/setup() path only (same as every other channel-driver
/// lifecycle operation).

#pragma once

// IWYU pragma: private

#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Try to claim an I2S port (0 or 1) for `owner`.
/// @param owner Stable string literal naming the claiming driver.
/// @return true if the port was free (or already held by this exact
///         owner pointer — claims are idempotent per owner); false if
///         another driver holds it.
bool i2sPortClaim(int port, const char *owner) FL_NO_EXCEPT;

/// @brief Release a port previously claimed by `owner`. No-op if the
///        port is not held by `owner`.
void i2sPortRelease(int port, const char *owner) FL_NO_EXCEPT;

/// @brief Current owner name for diagnostics, or nullptr if free.
const char *i2sPortOwner(int port) FL_NO_EXCEPT;

} // namespace fl
