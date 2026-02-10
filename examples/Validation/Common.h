// Common.h - Common data structures for Validation.ino
// Shared types used across multiple files

#pragma once

#include <FastLED.h>

// ============================================================================
// Test Configuration Constants
// ============================================================================
// Strip sizes are now fully runtime-configurable via JSON-RPC commands:
// - setStripSizeValues(short, long)
// - setStripSizes({small: bool, large: bool})
// Default values are set in TestMatrixConfig constructor

namespace fl {

/// @brief Driver failure tracking with detailed error information
struct DriverFailureInfo {
    fl::string driver_name;
    fl::string failure_details;  // e.g., "Byte mismatch at offset 5: expected 0xFF, got 0x00"
    uint32_t frame_number;       // Frame/iteration number when failure occurred

    DriverFailureInfo(const char* name, const char* details, uint32_t frame)
        : driver_name(name), failure_details(details), frame_number(frame) {}
};

/// @brief Chipset timing configuration with name for testing
struct NamedTimingConfig {
    fl::ChipsetTimingConfig timing;
    const char* name;

    NamedTimingConfig(const fl::ChipsetTimingConfig& timing_, const char* name_)
        : timing(timing_), name(name_) {}
};

// Legacy test matrix structures removed - validation now uses one-test-per-RPC architecture
// See src/fl/channels/validation.h for the new SingleTestConfig/SingleTestResult API

} // namespace fl
