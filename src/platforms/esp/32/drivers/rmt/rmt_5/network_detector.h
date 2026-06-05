#pragma once

// ok no namespace fl
// IWYU pragma: private

/// @file platforms/esp/32/drivers/rmt/rmt_5/network_detector.h
/// @brief Forwarding shim — actual definition lives at
/// `platforms/esp/32/util/network_detector.h`.
///
/// Promoted in #2818 (Phase 1 of #2815) so non-RMT5 ESP32 builds can use
/// the runtime network-activity detector. Existing RMT5-internal callers
/// keep their original include path via this shim.

#include "platforms/esp/32/util/network_detector.h"
