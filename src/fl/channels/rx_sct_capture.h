/// @file fl/channels/rx_sct_capture.h
/// @brief Public re-export of the LPC SCT-capture RX driver (FastLED#3015).
///
/// The driver itself lives in `platforms/arm/lpc/rx_sct_capture.h`; this
/// header sits in `src/fl/channels/` so the test file
/// `tests/fl/channels/rx_sct_capture.cpp` has a matching source path
/// (per `TestPathStructureChecker`) and so application code can
/// reference the driver from a stable `fl/channels/...` include without
/// reaching into `platforms/arm/lpc/`.
///
/// The `RxBackend::LPC_SCT_CAPTURE` dispatch already lives in
/// `fl/channels/rx/types.h` + `rx.cpp.hpp`; this header doesn't add any
/// new API surface, it only re-exports the platform impl class for
/// callers that want a direct `LpcSctRxChannel` reference (e.g. tests).

#pragma once

#include "platforms/is_platform.h"

#if defined(FL_IS_ARM_LPC)
// IWYU pragma: begin_keep
#include "platforms/arm/lpc/rx_sct_capture.h"  // ok platform headers
// IWYU pragma: end_keep
#endif
