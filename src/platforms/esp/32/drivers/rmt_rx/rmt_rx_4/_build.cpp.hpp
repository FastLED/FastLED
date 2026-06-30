// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for the IDF 4.x RmtRxChannel impl.
///
/// Mirrors `drivers/rmt/rmt_4/_build.cpp.hpp`. The implementation's
/// own gating ensures it only compiles on the classic ESP32 / S2
/// IDF 4.x path; on chips with IDF 5.x the parallel
/// `../rmt_rx_5/_build.cpp.hpp` provides the implementation instead.
/// See issue #3465.

#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_4/rmt_rx_channel_4.cpp.hpp"
