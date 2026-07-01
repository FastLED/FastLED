#pragma once

// IWYU pragma: private

/// @file _build.cpp.hpp
/// @brief Unity build header for platforms/arm/lpc/drivers/sct_dma/.

// UNITY_BUILD_ORDER(runtime must precede engine — engine instantiates `LpcSctDmaTransmitter` from runtime as a member. Alphabetically `channel_engine_lpc_sct_dma.cpp.hpp` sorts before `lpc_sct_dma_runtime.cpp.hpp`, but that would break the compile so we invert here.)

// begin current directory includes
// Runtime SCT+DMA helper. Definitions must precede the engine cpp.hpp
// because the engine instantiates a `LpcSctDmaTransmitter` member.
#include "platforms/arm/lpc/drivers/sct_dma/lpc_sct_dma_runtime.cpp.hpp"
#include "platforms/arm/lpc/drivers/sct_dma/channel_engine_lpc_sct_dma.cpp.hpp"

// BusTraits<Bus::BIT_BANG> specialization + BusSupports for ClocklessChipset.
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"
