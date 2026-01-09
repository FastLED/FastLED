// ok no namespace fl
// allow-include-after-namespace
#pragma once

/// @file platforms/arm/rp/init_rp.h
/// @brief RP2040/RP2350 platform initialization
///
/// RP2040/RP2350 platforms use PIO (Programmable I/O) for parallel LED output.
/// This initialization ensures the PIO parallel group system is initialized early.

namespace fl {
namespace platforms {

/// @brief Initialize RP2040/RP2350 platform
///
/// Performs one-time initialization of RP-specific subsystems:
/// - PIO Parallel Group: Manages automatic grouping of consecutive pins for parallel output
/// - PIO state machine and DMA channel allocation
///
/// The PIO parallel group system allows multiple LED strips to share PIO resources
/// efficiently. Initializing this early ensures consistent resource allocation.
///
/// This function is called once during FastLED::init() and is safe to call
/// multiple times (subsequent calls are no-ops).
///
/// @note Implementation is in src/platforms/arm/rp/init_rp.cpp
void init();

} // namespace platforms
} // namespace fl
