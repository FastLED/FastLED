#pragma once

// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for platforms/arm/lpc/drivers/sct_dma/.

// Runtime SCT+DMA helper. Definitions must precede the engine cpp.hpp
// because the engine instantiates a `LpcSctDmaTransmitter` member.
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.cpp.hpp"

#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.cpp.hpp"

// BusTraits<Bus::BIT_BANG> specialization + BusSupports for ClocklessChipset.
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"
