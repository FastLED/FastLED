#include "pixel_iterator.h"

FASTLED_NAMESPACE_BEGIN

PixelIterator::~PixelIterator() {}

void PixelIterator::loadAndScale_APA102_HD(uint8_t *b0_out, uint8_t *b1_out,
                                           uint8_t *b2_out, uint8_t *brightness_out) {
    // Default implementation
    loadAndScaleRGB(b0_out, b1_out, b2_out);
    *brightness_out = 0xFF; // Set maximum brightness by default
}

FASTLED_NAMESPACE_END
