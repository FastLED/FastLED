// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\parlio/ directory
/// Includes all implementation files in alphabetical order

#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_engine.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.cpp.hpp"

// BusTraits<Bus::PARLIO> specialization.
#include "platforms/esp/32/drivers/parlio/bus_traits.h"
