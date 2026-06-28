// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for drivers/flexio/ directory

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_driver.cpp.hpp"

// #3428 unified clockless+SPI engine: SPI-mode hardware helpers (stub today,
// real impl in a follow-up PR). See flexio_spi_mode.h header for rationale.
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_spi_mode.cpp.hpp"

// BusTraits<Bus::FLEX_IO> specialization + BusSupports for both chipset families.
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/bus_traits.h"
