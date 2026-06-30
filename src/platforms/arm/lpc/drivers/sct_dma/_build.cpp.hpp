// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for platforms/arm/lpc/drivers/sct_dma/.

#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.cpp.hpp"

// BusTraits<Bus::BIT_BANG> specialization + BusSupports for ClocklessChipset.
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"
