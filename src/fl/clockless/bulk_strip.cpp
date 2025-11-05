/// @file fl/clockless/bulk_strip.cpp
/// @brief Implementation file for BulkStrip class

#include "bulk_strip.h"

namespace fl {

// Default constructor
BulkStrip::BulkStrip()
    : mPin(-1), mBuffer(nullptr), mCount(0), mScreenMap() {}

// Constructor with initialization
BulkStrip::BulkStrip(int pin, CRGB *buffer, int count,
                     const fl::ScreenMap &screenmap)
    : mPin(pin), mBuffer(buffer), mCount(count), mScreenMap(screenmap) {}

// Set color correction for this strip
BulkStrip &BulkStrip::setCorrection(CRGB correction) {
    settings.correction = correction;
    return *this;
}

// Set color correction for this strip (LEDColorCorrection overload)
BulkStrip &BulkStrip::setCorrection(LEDColorCorrection correction) {
    settings.correction = correction;
    return *this;
}

// Set color temperature for this strip
BulkStrip &BulkStrip::setTemperature(CRGB temperature) {
    settings.temperature = temperature;
    return *this;
}

// Set color temperature for this strip (ColorTemperature overload)
BulkStrip &BulkStrip::setTemperature(ColorTemperature temperature) {
    settings.temperature = temperature;
    return *this;
}

// Set dither mode for this strip
BulkStrip &BulkStrip::setDither(fl::u8 ditherMode) {
    settings.ditherMode = ditherMode;
    return *this;
}

// Set RGBW configuration for this strip
BulkStrip &BulkStrip::setRgbw(const Rgbw &arg) {
    settings.rgbw = arg;
    return *this;
}

// Set ScreenMap for this strip
BulkStrip &BulkStrip::setScreenMap(const fl::ScreenMap &map) {
    mScreenMap = map;
    // Note: EngineEvents notification happens in BulkClockless::add()
    return *this;
}

// Get the pin number for this strip
int BulkStrip::getPin() const { return mPin; }

// Get the LED buffer pointer (non-owning)
CRGB *BulkStrip::getBuffer() const { return mBuffer; }

// Get the number of LEDs in this strip
int BulkStrip::getCount() const { return mCount; }

// Get the ScreenMap for this strip
const fl::ScreenMap &BulkStrip::getScreenMap() const { return mScreenMap; }

} // namespace fl
