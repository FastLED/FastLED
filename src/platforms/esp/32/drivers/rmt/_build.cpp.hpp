// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms/esp/32/drivers/rmt/ directory
/// Includes all RMT driver implementations
///
/// RMT is the platform-default clockless driver on every ESP32 variant
/// EXCEPT P4 / C6 / H2 / C5 (which lack RMT hardware and default to PARLIO).
/// The legacy clockless controller pre-binds to
/// `BusTraits<Bus::RMT>::instancePtr()` which links this TU on platforms
/// where RMT exists. The rmt_4 / rmt_5 sub-aggregators select the correct
/// impl based on `FASTLED_RMT5` / `FASTLED_ESP32_HAS_RMT`; nothing compiles
/// on platforms without RMT hardware.

// begin sub directory includes
#include "platforms/esp/32/drivers/rmt/rmt_4/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/_build.cpp.hpp"
