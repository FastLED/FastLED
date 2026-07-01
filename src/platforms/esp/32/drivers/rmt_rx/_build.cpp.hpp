// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build dispatcher for platforms/esp/32/drivers/rmt_rx/
///
/// The two sub-aggregators are mutually exclusive: `rmt_rx_4/`
/// compiles only when `FASTLED_RMT5 == 0` (classic ESP32 / S2 with
/// IDF 4.x) and `rmt_rx_5/` only when `FASTLED_RMT5 == 1` (S3, C3,
/// C6, H2, P4 with IDF 5.x). Mirrors the TX-side split in
/// `drivers/rmt/rmt_4` vs `drivers/rmt/rmt_5`. See issue #3465.

// begin sub directory includes
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_4/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_5/_build.cpp.hpp"
