// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for drivers/objectfled/ directory

#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/channel_engine_objectfled.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_diagnostics.cpp.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_peripheral_real.cpp.hpp"

// #3428 unified clockless+SPI engine: SPI-mode hardware helpers (stub today,
// real DMA-bit-banged SPI impl in a follow-up PR). See objectfled_spi_mode.h
// header for rationale.
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_spi_mode.cpp.hpp"

// BusTraits<Bus::FLEX_IO, 0> specialization + BusSupports for both chipset families.
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"
