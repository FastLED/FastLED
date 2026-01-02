#pragma once

/// @file platforms/arm/samd/pin_samd.hpp
/// SAMD (Arduino Zero, M0, M4, etc.) pin implementation (header-only)
///
/// Provides native SAMD pin functions using direct PORT register access.
/// This implementation does not depend on Arduino.h and uses pure SAMD register manipulation.
///
/// Uses new fl/pin.h API with enum classes (PinMode, PinValue, AdcRange).

// ok no namespace fl
#include "pin_samd_native.hpp"
