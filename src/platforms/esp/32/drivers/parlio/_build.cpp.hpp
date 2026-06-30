// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\parlio/ directory
///
/// PARLIO is the platform-default clockless driver on ESP32-P4 / C6 / H2 / C5.
/// On those variants the legacy clockless controller pre-binds to
/// `BusTraits<Bus::FLEX_IO, 0>::instancePtr()` which links this TU. On other
/// platforms (including host unit tests) the driver impl is also compiled --
/// the impl files self-gate on the appropriate hardware / mock macros, and
/// `--gc-sections` strips the unused driver TU when nothing ODR-uses the
/// singleton. The mock-peripheral file in particular must stay in the host
/// build so `cleanup_parlio_mock()` (called from `tests/doctest_main.cpp`)
/// has a definition.

#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_engine.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.cpp.hpp"

// BusTraits<Bus::FLEX_IO, 0> specialization.
#include "platforms/esp/32/drivers/parlio/bus_traits.h"
