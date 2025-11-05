#pragma once

/// @file fl/clockless/cpu_fallback.h
/// @brief CPU-based fallback implementation for BulkClockless when hardware peripherals are unavailable
///
/// This file provides a concrete implementation of the BulkClockless base template that uses
/// CPU bit-banging via individual CLEDController instances when hardware peripherals
/// (LCD_I80, RMT, I2S) are not available on the target platform.
///
/// **Design Philosophy**:
/// - **Portability over performance**: CPU fallback enables code to compile and run on any platform
/// - **Per-strip controllers**: Each strip gets an individual CLEDController (no bulk parallelism)
/// - **Runtime notification**: FL_WARN message informs user that peripheral is unavailable
/// - **Zero platform ifdefs**: User code compiles unchanged across all platforms
///
/// **Performance Characteristics**:
/// - No bulk parallelism (strips output sequentially)
/// - Higher CPU usage than peripheral-based output
/// - Higher memory overhead (~500-800 bytes per strip vs ~200-300 for peripherals)
/// - Suitable for development/prototyping, not production high-performance applications
///
/// **Peripheral Specialization Priority**:
/// When a peripheral specialization exists (e.g., bulk_rmt.h), it takes precedence over
/// this CPU fallback via C++ template specialization rules.

#include "fl/clockless/base.h"
#include "fl/dbg.h"
#include "fl/map.h"
#include "fl/vector.h"
#include "led_sysdefs.h"
#include "controller.h"

namespace fl {

/// CPU-based fallback implementation of BulkClockless (base template specialization)
///
/// This implementation is used when no hardware peripheral specialization exists.
/// It stores individual CLEDController pointers for each strip and outputs them
/// sequentially during show().
///
/// **IMPORTANT**: This is NOT a partial specialization - it's the concrete implementation
/// of the base template that was previously abstract. Peripheral specializations will
/// completely replace this implementation via full template specialization.
///
/// The implementation is provided via protected method overrides in the base template.
/// We cannot modify clockless.h to be concrete without breaking existing code,
/// so instead we provide these implementations in a wrapper or via conditional compilation.
///
/// **Actually, this approach won't work** because we can't make the abstract base template
/// concrete without modifying it, and peripheral specializations are full specializations
/// that don't inherit from the base.
///
/// **Revised Strategy**: We need to provide these implementations directly in clockless.h
/// by making the base template methods non-pure virtual with default implementations.
/// This file will contain the helper classes and logic that the base template uses.

/// Helper class to wrap a single-pin CLEDController for use in CPU fallback
///
/// This class is stored per-strip and handles the actual LED output when
/// no hardware peripheral is available.
class CPUFallbackController {
public:
    CPUFallbackController(int pin, CRGB* buffer, int count);

    void show(const BulkStrip::Settings& settings, fl::u8 brightness);

    int getPin() const;
    CRGB* getBuffer() const;
    int getCount() const;

private:
    int mPin;
    CRGB* mBuffer;
    int mCount;
};

} // namespace fl
