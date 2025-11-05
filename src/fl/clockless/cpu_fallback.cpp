/// @file fl/clockless/cpu_fallback.cpp
/// @brief Implementation file for CPU fallback controller

#include "cpu_fallback.h"
#include "fl/dbg.h"

namespace fl {

CPUFallbackController::CPUFallbackController(int pin, CRGB* buffer, int count)
    : mPin(pin), mBuffer(buffer), mCount(count) {
    // We'll store a pointer to the actual CLEDController once created
    // For now, this is a placeholder - actual implementation needs platform-specific
    // controller instantiation
}

void CPUFallbackController::show(const BulkStrip::Settings& settings, fl::u8 brightness) {
    // TODO: Implement actual show logic using platform-specific clockless controller
    // For now, this is a stub
    FL_DBG("CPUFallbackController::show() called for pin " << mPin);
    (void)settings;    // Suppress unused parameter warning
    (void)brightness;  // Suppress unused parameter warning
}

int CPUFallbackController::getPin() const { return mPin; }

CRGB* CPUFallbackController::getBuffer() const { return mBuffer; }

int CPUFallbackController::getCount() const { return mCount; }

} // namespace fl
