// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for the IDF 5.x RmtRxChannel impl.
///
/// Mirrors `drivers/rmt/rmt_5/_build.cpp.hpp`. The implementation's
/// own `#if FASTLED_RMT5` guard ensures it only compiles on the
/// IDF 5.x path; on classic ESP32 / S2 (IDF 4.x) the parallel
/// `../rmt_rx_4/_build.cpp.hpp` provides the implementation
/// instead. See issue #3465.

#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_5/rmt_rx_channel_5.cpp.hpp"
